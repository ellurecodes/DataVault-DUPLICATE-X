/**
 * @file win_file_compare.c
 * @brief Byte-by-byte streaming file comparison with modification safety checks on Windows.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "file_compare.h"
#include "config.h"
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Helper to convert UTF-8 path string to newly heap-allocated wide Unicode string.
 */
static wchar_t *utf8_to_wchar(const char *utf8) {
    if (!utf8) return NULL;
    int chars_needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (chars_needed <= 0) return NULL;

    wchar_t *wstr = (wchar_t *)malloc((size_t)chars_needed * sizeof(wchar_t));
    if (!wstr) return NULL;

    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, chars_needed);
    return wstr;
}

StatusCode file_compare_byte_by_byte_w(const wchar_t *wpath1,
                                       const wchar_t *wpath2,
                                       bool *is_identical) {
    if (!wpath1 || !wpath2 || !is_identical) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    *is_identical = false;

    /* Handle identical paths trivially */
    if (wcscmp(wpath1, wpath2) == 0) {
        *is_identical = true;
        return STATUS_SUCCESS;
    }

    /* Open first file */
    HANDLE hFile1 = CreateFileW(
        wpath1,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    if (hFile1 == INVALID_HANDLE_VALUE) {
        return STATUS_ERROR_FILE_READ_FAILED;
    }

    /* Open second file */
    HANDLE hFile2 = CreateFileW(
        wpath2,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    if (hFile2 == INVALID_HANDLE_VALUE) {
        CloseHandle(hFile1);
        return STATUS_ERROR_FILE_READ_FAILED;
    }

    /* Snapshot metadata before comparison */
    BY_HANDLE_FILE_INFORMATION info1_before, info2_before;
    if (!GetFileInformationByHandle(hFile1, &info1_before) ||
        !GetFileInformationByHandle(hFile2, &info2_before)) {
        CloseHandle(hFile1);
        CloseHandle(hFile2);
        return STATUS_ERROR_FILE_READ_FAILED;
    }

    uint64_t size1 = ((uint64_t)info1_before.nFileSizeHigh << 32) | info1_before.nFileSizeLow;
    uint64_t size2 = ((uint64_t)info2_before.nFileSizeHigh << 32) | info2_before.nFileSizeLow;

    /* Fast size pre-check: unequal sizes cannot be identical */
    if (size1 != size2) {
        CloseHandle(hFile1);
        CloseHandle(hFile2);
        *is_identical = false;
        return STATUS_SUCCESS;
    }

    /* Handle empty files: two verified 0-byte files are identical */
    if (size1 == 0) {
        CloseHandle(hFile1);
        CloseHandle(hFile2);
        *is_identical = true;
        return STATUS_SUCCESS;
    }

    /* Allocate streaming 64 KB buffers */
    uint8_t *buf1 = (uint8_t *)malloc(BUFFER_SIZE_64K);
    uint8_t *buf2 = (uint8_t *)malloc(BUFFER_SIZE_64K);

    if (!buf1 || !buf2) {
        free(buf1);
        free(buf2);
        CloseHandle(hFile1);
        CloseHandle(hFile2);
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    DWORD read1 = 0, read2 = 0;
    bool match = true;
    StatusCode final_status = STATUS_SUCCESS;

    while (true) {
        BOOL ok1 = ReadFile(hFile1, buf1, BUFFER_SIZE_64K, &read1, NULL);
        BOOL ok2 = ReadFile(hFile2, buf2, BUFFER_SIZE_64K, &read2, NULL);

        if (!ok1 || !ok2) {
            match = false;
            final_status = STATUS_ERROR_FILE_READ_FAILED;
            break;
        }

        if (read1 != read2) {
            match = false;
            break;
        }

        /* End of both files reached */
        if (read1 == 0) {
            break;
        }

        if (memcmp(buf1, buf2, read1) != 0) {
            match = false;
            break;
        }
    }

    /* Safety Check: Verify files did not change during byte comparison */
    BY_HANDLE_FILE_INFORMATION info1_after, info2_after;
    if (!GetFileInformationByHandle(hFile1, &info1_after) ||
        !GetFileInformationByHandle(hFile2, &info2_after)) {
        final_status = STATUS_ERROR_CONCURRENT_MODIFICATION;
        match = false;
    } else {
        bool changed1 = (info1_before.nFileSizeLow != info1_after.nFileSizeLow) ||
                        (info1_before.nFileSizeHigh != info1_after.nFileSizeHigh) ||
                        (CompareFileTime(&info1_before.ftLastWriteTime, &info1_after.ftLastWriteTime) != 0);

        bool changed2 = (info2_before.nFileSizeLow != info2_after.nFileSizeLow) ||
                        (info2_before.nFileSizeHigh != info2_after.nFileSizeHigh) ||
                        (CompareFileTime(&info2_before.ftLastWriteTime, &info2_after.ftLastWriteTime) != 0);

        if (changed1 || changed2) {
            final_status = STATUS_ERROR_CONCURRENT_MODIFICATION;
            match = false;
        }
    }

    free(buf1);
    free(buf2);
    CloseHandle(hFile1);
    CloseHandle(hFile2);

    *is_identical = match;
    return final_status;
}

StatusCode file_compare_byte_by_byte(const char *path1_utf8,
                                     const char *path2_utf8,
                                     bool *is_identical) {
    if (!path1_utf8 || !path2_utf8 || !is_identical) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    wchar_t *wpath1 = utf8_to_wchar(path1_utf8);
    wchar_t *wpath2 = utf8_to_wchar(path2_utf8);

    if (!wpath1 || !wpath2) {
        free(wpath1);
        free(wpath2);
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    StatusCode sc = file_compare_byte_by_byte_w(wpath1, wpath2, is_identical);

    free(wpath1);
    free(wpath2);
    return sc;
}
