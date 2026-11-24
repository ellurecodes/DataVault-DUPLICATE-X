/**
 * @file win_crypto.c
 * @brief SHA-256 content fingerprint implementation using Windows BCrypt API.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "sha256.h"
#include "config.h"
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BCRYPT_SUCCESS
#define BCRYPT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

void sha256_binary_to_hex(const uint8_t binary[SHA256_DIGEST_LENGTH],
                          char hex_out[SHA256_HEX_LENGTH]) {
    static const char hex_digits[] = "0123456789abcdef";
    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hex_out[i * 2]     = hex_digits[(binary[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hex_digits[binary[i] & 0x0F];
    }
    hex_out[SHA256_HEX_LENGTH - 1] = '\0';
}

StatusCode sha256_calculate_file(const wchar_t *filepath,
                                 uint8_t binary_out[SHA256_DIGEST_LENGTH],
                                 char hex_out[SHA256_HEX_LENGTH],
                                 HashStatus *status_out) {
    if (!filepath || !binary_out || !hex_out || !status_out) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    *status_out = HASH_STATUS_PENDING;
    memset(binary_out, 0, SHA256_DIGEST_LENGTH);
    memset(hex_out, 0, SHA256_HEX_LENGTH);

    /* Open file with sharing enabled so other processes can read */
    HANDLE hFile = CreateFileW(
        filepath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            *status_out = HASH_STATUS_DISAPPEARED;
            return STATUS_ERROR_PATH_NOT_FOUND;
        } else if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION) {
            *status_out = HASH_STATUS_PERMISSION_DENIED;
            return STATUS_ERROR_ACCESS_DENIED;
        }
        *status_out = HASH_STATUS_READ_ERROR;
        return STATUS_ERROR_FILE_READ_FAILED;
    }

    /* Module 3: Safety check - Record file size & timestamps BEFORE hashing */
    BY_HANDLE_FILE_INFORMATION infoBefore;
    if (!GetFileInformationByHandle(hFile, &infoBefore)) {
        CloseHandle(hFile);
        *status_out = HASH_STATUS_READ_ERROR;
        return STATUS_ERROR_FILE_READ_FAILED;
    }

    /* Initialize BCrypt SHA-256 algorithm provider */
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    PBYTE pbHashObject = NULL;
    PBYTE buffer = NULL;
    StatusCode finalStatus = STATUS_SUCCESS;

    NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(
        &hAlg,
        BCRYPT_SHA256_ALGORITHM,
        NULL,
        0
    );

    if (!BCRYPT_SUCCESS(ntStatus)) {
        CloseHandle(hFile);
        *status_out = HASH_STATUS_READ_ERROR;
        return STATUS_ERROR_UNKNOWN;
    }

    DWORD cbHashObject = 0;
    DWORD cbData = 0;
    ntStatus = BCryptGetProperty(
        hAlg,
        BCRYPT_OBJECT_LENGTH,
        (PBYTE)&cbHashObject,
        sizeof(DWORD),
        &cbData,
        0
    );

    if (!BCRYPT_SUCCESS(ntStatus)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        CloseHandle(hFile);
        *status_out = HASH_STATUS_READ_ERROR;
        return STATUS_ERROR_UNKNOWN;
    }

    pbHashObject = (PBYTE)malloc(cbHashObject);
    if (!pbHashObject) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        CloseHandle(hFile);
        *status_out = HASH_STATUS_READ_ERROR;
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    ntStatus = BCryptCreateHash(
        hAlg,
        &hHash,
        pbHashObject,
        cbHashObject,
        NULL,
        0,
        0
    );

    if (!BCRYPT_SUCCESS(ntStatus)) {
        free(pbHashObject);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        CloseHandle(hFile);
        *status_out = HASH_STATUS_READ_ERROR;
        return STATUS_ERROR_UNKNOWN;
    }

    /* Allocate streaming buffer (64 KB) */
    buffer = (PBYTE)malloc(BUFFER_SIZE_64K);
    if (!buffer) {
        BCryptDestroyHash(hHash);
        free(pbHashObject);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        CloseHandle(hFile);
        *status_out = HASH_STATUS_READ_ERROR;
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    /* Stream file chunks through BCryptHashData */
    DWORD bytesRead = 0;
    uint64_t totalBytesHashed = 0;
    BOOL readSuccess = TRUE;

    while ((readSuccess = ReadFile(hFile, buffer, BUFFER_SIZE_64K, &bytesRead, NULL)) && bytesRead > 0) {
        ntStatus = BCryptHashData(hHash, buffer, bytesRead, 0);
        if (!BCRYPT_SUCCESS(ntStatus)) {
            finalStatus = STATUS_ERROR_UNKNOWN;
            break;
        }
        totalBytesHashed += bytesRead;
    }

    if (!readSuccess) {
        finalStatus = STATUS_ERROR_FILE_READ_FAILED;
    }

    /* Finalize hash if read succeeded */
    if (finalStatus == STATUS_SUCCESS) {
        ntStatus = BCryptFinishHash(hHash, binary_out, SHA256_DIGEST_LENGTH, 0);
        if (BCRYPT_SUCCESS(ntStatus)) {
            sha256_binary_to_hex(binary_out, hex_out);
            *status_out = (totalBytesHashed == 0) ? HASH_STATUS_EMPTY_FILE : HASH_STATUS_SUCCESS;
        } else {
            finalStatus = STATUS_ERROR_UNKNOWN;
        }
    }

    /* Module 3: Safety check - Re-verify file size & timestamps AFTER hashing */
    BY_HANDLE_FILE_INFORMATION infoAfter;
    if (!GetFileInformationByHandle(hFile, &infoAfter)) {
        *status_out = HASH_STATUS_DISAPPEARED;
        finalStatus = STATUS_ERROR_CONCURRENT_MODIFICATION;
    } else {
        bool sizeChanged = (infoBefore.nFileSizeLow != infoAfter.nFileSizeLow) ||
                           (infoBefore.nFileSizeHigh != infoAfter.nFileSizeHigh);
        bool timeChanged = (CompareFileTime(&infoBefore.ftLastWriteTime, &infoAfter.ftLastWriteTime) != 0);

        if (sizeChanged || timeChanged) {
            *status_out = HASH_STATUS_MODIFIED_DURING_SCAN;
            memset(binary_out, 0, SHA256_DIGEST_LENGTH);
            memset(hex_out, 0, SHA256_HEX_LENGTH);
            finalStatus = STATUS_ERROR_CONCURRENT_MODIFICATION;
        }
    }

    /* Release all resources and handles */
    free(buffer);
    BCryptDestroyHash(hHash);
    free(pbHashObject);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    CloseHandle(hFile);

    return finalStatus;
}

