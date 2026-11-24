/**
 * @file quarantine.h
 * @brief Safe quarantine and file recovery engine for DUPLICATE-X.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_QUARANTINE_H
#define DATAVAULT_QUARANTINE_H

#include "status.h"
#include "quarantine_manifest.h"
#include "duplicate_group.h"
#include "file_scanner.h"
#include <wchar.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QUARANTINE_PREVIEW_EXPIRY_SECONDS 300  /* 5 minutes validity */
#define QUARANTINE_DIR_NAME L".datavault_quarantine"

/**
 * @struct QuarantinePreview
 * @brief Ephemeral preview state for two-stage user confirmation.
 */
typedef struct {
    char preview_id[64];               /**< Securely generated single-use preview token */
    uint32_t group_id;                 /**< Target duplicate group ID */
    char expected_sha256[65];          /**< Expected SHA-256 content hash */
    uint64_t file_ids[32];             /**< Selected file record IDs to quarantine */
    char file_paths[32][1024];         /**< Original paths of selected files */
    uint64_t file_sizes[32];           /**< File sizes */
    size_t file_count;                 /**< Number of files selected */
    uint64_t estimated_recoverable_bytes; /**< Storage freed upon quarantine */
    size_t remaining_copies;           /**< Valid copies that will remain after move (must be >= 1) */
    int64_t created_at;                /**< Epoch timestamp */
    int64_t expires_at;                /**< Epoch timestamp after which token is invalid */
    bool is_used;                      /**< True if preview has been executed or cancelled */
    bool is_restore;                   /**< True if this is a restore preview */
    char restore_transaction_id[64];   /**< Transaction ID if this is a restore preview */
} QuarantinePreview;

/**
 * @struct QuarantineManager
 * @brief Central coordinator for safe quarantine, verification, and recovery.
 */
typedef struct {
    QuarantineManifest manifest;       /**< Persistent transaction history */
    QuarantinePreview *previews;       /**< Active preview dynamic array */
    size_t preview_count;              /**< Number of previews in cache */
    size_t preview_capacity;           /**< Allocated capacity of preview cache */
} QuarantineManager;

/**
 * @brief Initialize quarantine manager and load existing manifest/journal.
 */
StatusCode quarantine_manager_init(QuarantineManager *mgr, const char *manifest_path, const char *journal_path);

/**
 * @brief Clean up and save quarantine manager state.
 */
void quarantine_manager_cleanup(QuarantineManager *mgr);

/**
 * @brief Stage A: Generate preview for quarantining selected redundant duplicate copies.
 * 
 * Enforces duplicate copy protection:
 * - Ensures at least one valid copy remains in the group.
 * - Revalidates SHA-256 and byte-compares selected files against the retained copy.
 * - Detects hard-links pointing to the same physical file.
 * 
 * @param mgr Quarantine manager instance.
 * @param groups Confirmed duplicate groups list.
 * @param group_id ID of group to quarantine from.
 * @param file_ids Array of file record IDs to quarantine.
 * @param file_count Count of files in file_ids array.
 * @param out_preview Populated preview structure with single-use token.
 */
StatusCode quarantine_preview_create(QuarantineManager *mgr,
                                     const DuplicateGroupList *groups,
                                     uint32_t group_id,
                                     const uint64_t *file_ids,
                                     size_t file_count,
                                     QuarantinePreview *out_preview);

/**
 * @brief Stage B: Execute confirmed quarantine move for a validated preview.
 * 
 * - Re-validates conditions before execution.
 * - Performs same-volume MoveFileExW without copying.
 * - Invalidates preview token immediately.
 * - Logs to persistent manifest and write-ahead journal.
 * - Updates in-memory duplicate groups and inventory.
 */
StatusCode quarantine_execute(QuarantineManager *mgr,
                              DuplicateGroupList *groups,
                              FileInventory *inventory,
                              const char *preview_id,
                              size_t *out_quarantined_count,
                              char out_tx_ids[32][64]);

/**
 * @brief Stage A (Restore): Generate preview for restoring a previously quarantined file.
 */
StatusCode quarantine_restore_preview(QuarantineManager *mgr,
                                      const char *transaction_id,
                                      QuarantinePreview *out_preview);

/**
 * @brief Stage B (Restore): Execute confirmed restoration of a quarantined file.
 * 
 * - Verifies quarantined file exists and matches original SHA-256 (tamper check).
 * - Ensures destination path does NOT exist (prevents accidental overwrite).
 * - Moves file back to original location via same-volume move.
 * - Updates manifest status to RESTORED.
 */
StatusCode quarantine_restore_execute(QuarantineManager *mgr,
                                      const char *preview_id,
                                      char *out_restored_path,
                                      size_t max_path_len);

/**
 * @brief Check if two filepaths reside on the same filesystem volume.
 */
bool quarantine_is_same_volume(const wchar_t *src_w, const wchar_t *dest_w);

/**
 * @brief Check if a path is a symbolic link, junction, or reparse point.
 */
bool quarantine_is_reparse_point(const wchar_t *path_w);

/**
 * @brief Check if two paths reference the exact same physical file via NTFS hard link.
 */
bool quarantine_is_hardlink(const wchar_t *path1_w, const wchar_t *path2_w);

/**
 * @brief Construct safe quarantine target path on same volume in `.datavault_quarantine`.
 */
bool quarantine_generate_dest_path(const wchar_t *src_w,
                                   const char *tx_id,
                                   wchar_t *out_dest_w,
                                   size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* DATAVAULT_QUARANTINE_H */
