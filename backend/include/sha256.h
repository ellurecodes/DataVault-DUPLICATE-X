/**
 * @file sha256.h
 * @brief Cryptographic SHA-256 fingerprint engine with file modification safety checks.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_SHA256_H
#define DATAVAULT_SHA256_H

#include "file_record.h"
#include "status.h"
#include <stdint.h>
#include <wchar.h>

#define SHA256_DIGEST_LENGTH  32
#define SHA256_HEX_LENGTH     65

/**
 * @brief Compute the 256-bit SHA-256 content fingerprint of a file using Windows BCrypt.
 * 
 * Streams file content in 64 KB chunks without loading the file into memory.
 * Includes concurrency safety checks: confirms file size and timestamp before and after hashing.
 * 
 * @param filepath Wide Unicode path to the target file.
 * @param binary_out Output buffer for 32-byte raw hash digest (must be >= 32 bytes).
 * @param hex_out Output buffer for 64-character lowercase hex string + null (must be >= 65 bytes).
 * @param status_out Output pointer receiving the detailed HashStatus.
 * @return STATUS_SUCCESS on valid computation, or appropriate error code.
 */
StatusCode sha256_calculate_file(const wchar_t *filepath,
                                 uint8_t binary_out[SHA256_DIGEST_LENGTH],
                                 char hex_out[SHA256_HEX_LENGTH],
                                 HashStatus *status_out);

/**
 * @brief Helper utility to convert a 32-byte binary digest into a 64-char lowercase hex string.
 * @param binary Input 32-byte binary array.
 * @param hex_out Output 65-byte char buffer.
 */
void sha256_binary_to_hex(const uint8_t binary[SHA256_DIGEST_LENGTH],
                          char hex_out[SHA256_HEX_LENGTH]);

#endif /* DATAVAULT_SHA256_H */

