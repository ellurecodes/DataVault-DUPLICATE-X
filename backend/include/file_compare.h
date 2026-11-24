/**
 * @file file_compare.h
 * @brief Byte-by-byte file content verification interface using streaming I/O.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_FILE_COMPARE_H
#define DATAVAULT_FILE_COMPARE_H

#include "status.h"
#include <stdbool.h>
#include <wchar.h>

/**
 * @brief Compare the exact binary contents of two files byte-by-byte using streaming 64 KB buffers.
 * 
 * Includes pre- and post-read metadata snapshots to detect concurrent file modifications.
 * Handles empty files and large files without loading entire payloads into RAM.
 * 
 * @param path1_utf8 UTF-8 path to the first file.
 * @param path2_utf8 UTF-8 path to the second file.
 * @param is_identical Output boolean: true if contents match 100% exactly; false otherwise.
 * @return STATUS_SUCCESS on valid comparison, or specific error code.
 */
StatusCode file_compare_byte_by_byte(const char *path1_utf8,
                                     const char *path2_utf8,
                                     bool *is_identical);

/**
 * @brief Wide-character variant for direct Win32 Unicode paths.
 */
StatusCode file_compare_byte_by_byte_w(const wchar_t *wpath1,
                                       const wchar_t *wpath2,
                                       bool *is_identical);

#endif /* DATAVAULT_FILE_COMPARE_H */

