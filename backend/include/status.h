/**
 * @file status.h
 * @brief Standardized status codes and error reporting for DUPLICATE-X.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_STATUS_H
#define DATAVAULT_STATUS_H

/**
 * @enum StatusCode
 * @brief Unified return status codes across all engine and DSA modules.
 */
typedef enum {
    /* Success Codes (0 to 99) */
    STATUS_SUCCESS                  = 0,
    STATUS_SCAN_COMPLETED           = 1,
    STATUS_NO_DUPLICATES_FOUND      = 2,

    /* General Error Codes (100 to 199) */
    STATUS_ERROR_UNKNOWN            = 100,
    STATUS_ERROR_INVALID_ARGUMENT   = 101,
    STATUS_ERROR_OUT_OF_MEMORY      = 102,
    STATUS_ERROR_NOT_INITIALIZED    = 103,

    /* Filesystem & I/O Errors (200 to 299) */
    STATUS_ERROR_PATH_NOT_FOUND     = 200,
    STATUS_ERROR_ACCESS_DENIED      = 201,
    STATUS_ERROR_FILE_READ_FAILED   = 202,
    STATUS_ERROR_IS_REPARSE_POINT   = 203,
    STATUS_ERROR_IS_SYSTEM_FOLDER   = 204,

    /* Quarantine & Safety Violations (300 to 399) */
    STATUS_ERROR_ROOT_COPY_PROTECTED = 300,
    STATUS_ERROR_SOLE_COPY_REMAINING = 301,
    STATUS_ERROR_CONCURRENT_MODIFICATION = 302,
    STATUS_ERROR_QUARANTINE_FAILED  = 303,
    STATUS_ERROR_RESTORE_FAILED     = 304,
    STATUS_ERROR_CROSS_VOLUME_MOVE  = 305,
    STATUS_ERROR_PREVIEW_EXPIRED    = 306,
    STATUS_ERROR_PREVIEW_NOT_FOUND  = 307,
    STATUS_ERROR_FILE_TAMPERED      = 308,
    STATUS_ERROR_DESTINATION_EXISTS = 309,
    STATUS_ERROR_PREVIEW_ALREADY_USED = 310,

    /* Data Structure & Algorithmic Errors (400 to 499) */
    STATUS_ERROR_HT_COLLISION_LIMIT = 400,
    STATUS_ERROR_HEAP_EMPTY         = 401,
    STATUS_ERROR_HEAP_FULL          = 402,
    STATUS_ERROR_TRIE_INSERT_FAILED = 403

} StatusCode;

/**
 * @brief Convert a StatusCode enum value into a human-readable string.
 * @param code The status code to describe.
 * @return Constant string representation of the status code.
 */
static inline const char *status_to_string(StatusCode code) {
    switch (code) {
        case STATUS_SUCCESS:
            return "Operation completed successfully.";
        case STATUS_SCAN_COMPLETED:
            return "Filesystem scan completed.";
        case STATUS_NO_DUPLICATES_FOUND:
            return "No duplicate files were detected.";
        case STATUS_ERROR_INVALID_ARGUMENT:
            return "Invalid argument supplied.";
        case STATUS_ERROR_OUT_OF_MEMORY:
            return "Memory allocation failed (out of memory).";
        case STATUS_ERROR_NOT_INITIALIZED:
            return "Subsystem is not initialized.";
        case STATUS_ERROR_PATH_NOT_FOUND:
            return "Specified path does not exist.";
        case STATUS_ERROR_ACCESS_DENIED:
            return "Access denied (permission error).";
        case STATUS_ERROR_FILE_READ_FAILED:
            return "Failed to read file.";
        case STATUS_ERROR_IS_REPARSE_POINT:
            return "Skipping symbolic link or reparse point.";
        case STATUS_ERROR_IS_SYSTEM_FOLDER:
            return "Access blocked: path is a protected system directory.";
        case STATUS_ERROR_ROOT_COPY_PROTECTED:
            return "Safety violation: cannot delete or quarantine root copy.";
        case STATUS_ERROR_SOLE_COPY_REMAINING:
            return "Safety violation: only one remaining copy exists.";
        case STATUS_ERROR_CONCURRENT_MODIFICATION:
            return "File was modified concurrently during scanning.";
        case STATUS_ERROR_QUARANTINE_FAILED:
            return "Failed to relocate file to quarantine.";
        case STATUS_ERROR_RESTORE_FAILED:
            return "Failed to restore file from quarantine.";
        case STATUS_ERROR_CROSS_VOLUME_MOVE:
            return "Safety violation: cross-volume movement is forbidden.";
        case STATUS_ERROR_PREVIEW_EXPIRED:
            return "Quarantine preview token has expired.";
        case STATUS_ERROR_PREVIEW_NOT_FOUND:
            return "Quarantine preview token was not found.";
        case STATUS_ERROR_FILE_TAMPERED:
            return "Integrity violation: file content or hash has been tampered with.";
        case STATUS_ERROR_DESTINATION_EXISTS:
            return "Safety violation: destination file already exists; overwrite prevented.";
        case STATUS_ERROR_PREVIEW_ALREADY_USED:
            return "Quarantine preview token has already been used.";
        default:
            return "An unknown error occurred.";
    }
}

#endif /* DATAVAULT_STATUS_H */

