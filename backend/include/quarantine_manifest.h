/**
 * @file quarantine_manifest.h
 * @brief Persistent transaction manifest and write-ahead journal for DUPLICATE-X quarantine.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_QUARANTINE_MANIFEST_H
#define DATAVAULT_QUARANTINE_MANIFEST_H

#include "status.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum QuarantineStatus
 * @brief Lifecycle status of a quarantined file transaction.
 */
typedef enum {
    QUARANTINE_STATUS_PREVIEWED = 0,    /**< Operation previewed but not executed */
    QUARANTINE_STATUS_ACTIVE    = 1,    /**< File is moved and actively stored in quarantine */
    QUARANTINE_STATUS_RESTORED  = 2,    /**< File has been safely restored to original path */
    QUARANTINE_STATUS_FAILED    = 3     /**< Relocation or restore failed */
} QuarantineStatus;

/**
 * @struct QuarantineRecord
 * @brief Persistent metadata for a single quarantined file transaction.
 */
typedef struct {
    char transaction_id[64];           /**< Unique transaction identifier */
    uint32_t group_id;                 /**< Source duplicate group ID */
    uint64_t file_record_id;           /**< Inventory file record ID */
    char original_path[1024];          /**< Absolute original filesystem path (UTF-8) */
    char quarantine_path[1024];        /**< Absolute quarantine destination path (UTF-8) */
    char original_filename[256];       /**< Base filename */
    uint64_t file_size;                /**< File size in bytes */
    char sha256_hex[65];               /**< SHA-256 fingerprint verified before move */
    int64_t quarantine_timestamp;      /**< Unix epoch timestamp of quarantine move */
    int64_t restore_timestamp;         /**< Unix epoch timestamp of restoration (0 if active) */
    QuarantineStatus status;           /**< ACTIVE, RESTORED, or FAILED */
} QuarantineRecord;

/**
 * @struct QuarantineManifest
 * @brief In-memory container and file persistence for all quarantine records.
 */
typedef struct {
    QuarantineRecord *records;         /**< Dynamic array of transaction records */
    size_t count;                      /**< Number of records */
    size_t capacity;                   /**< Allocated capacity */
    char manifest_filepath[1024];      /**< Path to JSON manifest file */
    char journal_filepath[1024];       /**< Path to write-ahead journal file */
} QuarantineManifest;

/**
 * @brief Initialize quarantine manifest container.
 */
StatusCode quarantine_manifest_init(QuarantineManifest *manifest, const char *manifest_path, const char *journal_path);

/**
 * @brief Clean up and release memory held by quarantine manifest.
 */
void quarantine_manifest_free(QuarantineManifest *manifest);

/**
 * @brief Add a new transaction record to the manifest.
 */
StatusCode quarantine_manifest_add(QuarantineManifest *manifest, const QuarantineRecord *rec);

/**
 * @brief Find an existing transaction record by its unique transaction ID.
 */
QuarantineRecord *quarantine_manifest_find(QuarantineManifest *manifest, const char *transaction_id);

/**
 * @brief Update the status of an existing transaction record.
 */
StatusCode quarantine_manifest_update_status(QuarantineManifest *manifest, const char *transaction_id, QuarantineStatus new_status);

/**
 * @brief Serialize manifest records to JSON disk file with atomic replace semantics.
 */
StatusCode quarantine_manifest_save(const QuarantineManifest *manifest);

/**
 * @brief Load manifest records from persistent JSON disk file.
 */
StatusCode quarantine_manifest_load(QuarantineManifest *manifest);

/**
 * @brief Append a write-ahead journal entry with immediate flush to disk.
 */
StatusCode quarantine_manifest_append_journal(const QuarantineManifest *manifest, const QuarantineRecord *rec, const char *action);

/**
 * @brief Reconcile manifest on application startup, verifying file locations and recovering interrupted moves.
 */
StatusCode quarantine_manifest_reconcile(QuarantineManifest *manifest);

#ifdef __cplusplus
}
#endif

#endif /* DATAVAULT_QUARANTINE_MANIFEST_H */

