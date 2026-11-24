/**
 * @file file_record.h
 * @brief File metadata record and dynamic inventory data structures.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_FILE_RECORD_H
#define DATAVAULT_FILE_RECORD_H

#include "status.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @enum HashStatus
 * @brief Lifecycle and error states for cryptographic fingerprinting.
 */
typedef enum {
    HASH_STATUS_PENDING                 = 0,
    HASH_STATUS_SUCCESS                 = 1,
    HASH_STATUS_EMPTY_FILE              = 2,
    HASH_STATUS_READ_ERROR              = 3,
    HASH_STATUS_MODIFIED_DURING_SCAN    = 4,
    HASH_STATUS_DISAPPEARED             = 5,
    HASH_STATUS_PERMISSION_DENIED       = 6
} HashStatus;

/**
 * @struct FileRecord
 * @brief Detailed metadata and content fingerprint for a discovered file.
 */
typedef struct {
    uint64_t record_id;         /**< Sequential identifier */
    char *filename;             /**< Base filename (e.g., "notes.txt") */
    char *filepath;             /**< Full absolute canonical path */
    char *extension;            /**< Extension with dot (e.g., ".txt") or "" */
    char *device_id;            /**< Origin root or volume tag (e.g., "C:", "Backup") */
    uint64_t file_size;         /**< Exact file size in bytes */
    uint64_t modified_time;     /**< Modification timestamp (Unix epoch seconds) */
    
    uint8_t sha256_binary[32];  /**< 256-bit raw binary digest */
    char sha256_hex[65];        /**< 64-character lowercase hex string + null */
    HashStatus hash_status;     /**< Result state of fingerprint computation */
} FileRecord;

/**
 * @struct FileInventory
 * @brief Dynamically resizable array holding scanned file records.
 */
typedef struct {
    FileRecord *records;        /**< Dynamic array of file records */
    size_t count;               /**< Current number of registered files */
    size_t capacity;            /**< Allocated capacity */
    uint64_t total_bytes;       /**< Aggregate size in bytes */
} FileInventory;

/**
 * @brief Initialize an empty file inventory.
 * @param inv Pointer to FileInventory.
 */
void file_inventory_init(FileInventory *inv);

/**
 * @brief Add a copy of a file record into the dynamic inventory.
 * @param inv Pointer to FileInventory.
 * @param rec Pointer to FileRecord to insert.
 * @return STATUS_SUCCESS or STATUS_ERROR_OUT_OF_MEMORY.
 */
StatusCode file_inventory_add(FileInventory *inv, const FileRecord *rec);

/**
 * @brief Free heap memory allocated for all records inside an inventory.
 * @param inv Pointer to FileInventory.
 */
void file_inventory_free(FileInventory *inv);

/**
 * @brief Clean up dynamically allocated fields in a single record.
 * @param rec Pointer to FileRecord.
 */
void file_record_cleanup(FileRecord *rec);

/**
 * @brief Return a human-readable string for a HashStatus enum.
 * @param status The hash status code.
 * @return String description.
 */
const char *hash_status_to_string(HashStatus status);

#endif /* DATAVAULT_FILE_RECORD_H */

