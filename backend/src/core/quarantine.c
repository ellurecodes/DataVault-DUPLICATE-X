/**
 * @file quarantine.c
 * @brief Implementation of safe quarantine movement, duplicate copy protection, and recovery.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "quarantine.h"
#include "sha256.h"
#include "file_compare.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <bcrypt.h>

/* Helper to generate cryptographically secure preview or transaction tokens */
static void generate_secure_token(const char *prefix, char *out_buf, size_t buf_sz) {
    uint8_t rand_bytes[16];
    memset(rand_bytes, 0, sizeof(rand_bytes));

    NTSTATUS status = BCryptGenRandom(NULL, rand_bytes, sizeof(rand_bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        /* High-entropy fallback if BCrypt is not available */
        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);
        uint64_t seed = (uint64_t)qpc.QuadPart ^ (uint64_t)time(NULL);
        for (int i = 0; i < 16; i++) {
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            rand_bytes[i] = (uint8_t)(seed >> 32);
        }
    }

    snprintf(out_buf, buf_sz, "%s_%llx_%02x%02x%02x%02x%02x%02x",
             prefix,
             (unsigned long long)time(NULL),
             rand_bytes[0], rand_bytes[1], rand_bytes[2], rand_bytes[3], rand_bytes[4], rand_bytes[5]);
}

StatusCode quarantine_manager_init(QuarantineManager *mgr, const char *manifest_path, const char *journal_path) {
    if (!mgr) return STATUS_ERROR_INVALID_ARGUMENT;

    memset(mgr, 0, sizeof(QuarantineManager));
    mgr->preview_capacity = 32;
    mgr->preview_count = 0;
    mgr->previews = (QuarantinePreview *)calloc(mgr->preview_capacity, sizeof(QuarantinePreview));
    if (!mgr->previews) {
        return STATUS_ERROR_OUT_OF_MEMORY;
    }
    return quarantine_manifest_init(&mgr->manifest, manifest_path, journal_path);
}

void quarantine_manager_cleanup(QuarantineManager *mgr) {
    if (!mgr) return;
    quarantine_manifest_free(&mgr->manifest);
    if (mgr->previews) {
        free(mgr->previews);
        mgr->previews = NULL;
    }
    mgr->preview_count = 0;
    mgr->preview_capacity = 0;
}

static void quarantine_manager_add_preview(QuarantineManager *mgr, const QuarantinePreview *prev) {
    if (!mgr || !prev) return;
    if (mgr->preview_count >= mgr->preview_capacity) {
        size_t new_cap = (mgr->preview_capacity == 0) ? 16 : mgr->preview_capacity * 2;
        QuarantinePreview *new_arr = (QuarantinePreview *)realloc(mgr->previews, new_cap * sizeof(QuarantinePreview));
        if (!new_arr) return;
        mgr->previews = new_arr;
        mgr->preview_capacity = new_cap;
    }
    mgr->previews[mgr->preview_count++] = *prev;
}

bool quarantine_is_reparse_point(const wchar_t *path_w) {
    if (!path_w) return false;
    DWORD attr = GetFileAttributesW(path_w);
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    return (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool quarantine_is_same_volume(const wchar_t *src_w, const wchar_t *dest_w) {
    if (!src_w || !dest_w) return false;

    wchar_t src_vol[MAX_PATH] = {0};
    wchar_t dest_vol[MAX_PATH] = {0};

    if (GetVolumePathNameW(src_w, src_vol, MAX_PATH) &&
        GetVolumePathNameW(dest_w, dest_vol, MAX_PATH)) {
        return (_wcsicmp(src_vol, dest_vol) == 0);
    }

    /* Fallback: compare drive letter prefixes if full volume name query fails */
    if (src_w[1] == L':' && dest_w[1] == L':') {
        return (towupper(src_w[0]) == towupper(dest_w[0]));
    }

    return false;
}

bool quarantine_is_hardlink(const wchar_t *path1_w, const wchar_t *path2_w) {
    if (!path1_w || !path2_w) return false;

    HANDLE h1 = CreateFileW(path1_w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h1 == INVALID_HANDLE_VALUE) return false;

    HANDLE h2 = CreateFileW(path2_w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h2 == INVALID_HANDLE_VALUE) {
        CloseHandle(h1);
        return false;
    }

    BY_HANDLE_FILE_INFORMATION info1, info2;
    bool is_link = false;
    if (GetFileInformationByHandle(h1, &info1) && GetFileInformationByHandle(h2, &info2)) {
        if (info1.dwVolumeSerialNumber == info2.dwVolumeSerialNumber &&
            info1.nFileIndexHigh == info2.nFileIndexHigh &&
            info1.nFileIndexLow == info2.nFileIndexLow) {
            is_link = true;
        }
    }

    CloseHandle(h1);
    CloseHandle(h2);
    return is_link;
}

bool quarantine_generate_dest_path(const wchar_t *src_w,
                                   const char *tx_id,
                                   wchar_t *out_dest_w,
                                   size_t max_len) {
    if (!src_w || !tx_id || !out_dest_w || max_len == 0) return false;

    /* Extract parent directory of source file */
    wchar_t parent_dir[1024];
    wcsncpy(parent_dir, src_w, 1023);
    parent_dir[1023] = L'\0';

    wchar_t *last_slash = wcsrchr(parent_dir, L'\\');
    wchar_t *last_fwd = wcsrchr(parent_dir, L'/');
    if (last_fwd > last_slash) last_slash = last_fwd;

    const wchar_t *base_filename = L"file.bin";
    if (last_slash) {
        base_filename = last_slash + 1;
        *last_slash = L'\0';
    } else {
        wcscpy(parent_dir, L".");
    }

    /* Build quarantine sub-directory: <parent_dir>\.datavault_quarantine */
    wchar_t quar_dir[1024];
    _snwprintf(quar_dir, 1023, L"%ls\\%ls", parent_dir, QUARANTINE_DIR_NAME);
    quar_dir[1023] = L'\0';

    /* Create directory if missing and mark as hidden */
    CreateDirectoryW(quar_dir, NULL);
    SetFileAttributesW(quar_dir, FILE_ATTRIBUTE_HIDDEN);

    /* Convert transaction ID to wide */
    wchar_t tx_id_w[64];
    MultiByteToWideChar(CP_UTF8, 0, tx_id, -1, tx_id_w, 64);

    /* Construct unique destination: <quar_dir>\<tx_id>_<base_filename> */
    _snwprintf(out_dest_w, max_len - 1, L"%ls\\%ls_%ls", quar_dir, tx_id_w, base_filename);
    out_dest_w[max_len - 1] = L'\0';

    return true;
}

StatusCode quarantine_preview_create(QuarantineManager *mgr,
                                     const DuplicateGroupList *groups,
                                     uint32_t group_id,
                                     const uint64_t *file_ids,
                                     size_t file_count,
                                     QuarantinePreview *out_preview) {
    if (!mgr || !groups || !file_ids || file_count == 0 || !out_preview) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    /* Locate duplicate group */
    const DuplicateGroup *target_group = NULL;
    for (const DuplicateGroup *g = groups->head; g != NULL; g = g->next) {
        if (g->group_id == group_id) {
            target_group = g;
            break;
        }
    }

    if (!target_group) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    /* Validate against duplicate IDs in request array */
    for (size_t i = 0; i < file_count; i++) {
        for (size_t j = i + 1; j < file_count; j++) {
            if (file_ids[i] == file_ids[j]) {
                return STATUS_ERROR_INVALID_ARGUMENT;
            }
        }
    }

    /* Count files in group and verify requested files belong to the group */
    size_t group_file_count = target_group->file_count;
    if (file_count >= group_file_count) {
        /* CRITICAL SAFETY CHECK: At least one copy must remain! */
        return STATUS_ERROR_SOLE_COPY_REMAINING;
    }

    size_t matched_count = 0;
    const FileNode *selected_nodes[32];
    const FileNode *retained_node = NULL;

    for (const FileNode *cur = target_group->files_head; cur != NULL; cur = cur->next) {
        bool is_selected = false;
        for (size_t i = 0; i < file_count; i++) {
            if (cur->record && cur->record->record_id == file_ids[i]) {
                if (matched_count < 32) {
                    selected_nodes[matched_count++] = cur;
                }
                is_selected = true;
                break;
            }
        }
        if (!is_selected && !retained_node) {
            retained_node = cur;
        }
    }

    if (matched_count != file_count || !retained_node) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    /* Convert retained copy path to wide */
    wchar_t retained_w[1024];
    MultiByteToWideChar(CP_UTF8, 0, retained_node->record->filepath, -1, retained_w, 1024);

    /* Verify retained file exists and is readable */
    if (GetFileAttributesW(retained_w) == INVALID_FILE_ATTRIBUTES) {
        return STATUS_ERROR_PATH_NOT_FOUND;
    }

    /* Initialize preview structure */
    memset(out_preview, 0, sizeof(QuarantinePreview));
    generate_secure_token("prev", out_preview->preview_id, sizeof(out_preview->preview_id));
    out_preview->group_id = group_id;
    strncpy(out_preview->expected_sha256, target_group->sha256_hex, sizeof(out_preview->expected_sha256) - 1);
    out_preview->file_count = file_count;
    out_preview->remaining_copies = group_file_count - file_count;
    out_preview->created_at = (int64_t)time(NULL);
    out_preview->expires_at = out_preview->created_at + QUARANTINE_PREVIEW_EXPIRY_SECONDS;
    out_preview->is_used = false;
    out_preview->is_restore = false;

    uint64_t total_recoverable = 0;

    /* Revalidate each selected file before admitting into preview */
    for (size_t i = 0; i < file_count; i++) {
        const FileRecord *rec = selected_nodes[i]->record;
        out_preview->file_ids[i] = rec->record_id;
        out_preview->file_sizes[i] = rec->file_size;
        strncpy(out_preview->file_paths[i], rec->filepath, sizeof(out_preview->file_paths[i]) - 1);

        wchar_t sel_w[1024];
        MultiByteToWideChar(CP_UTF8, 0, rec->filepath, -1, sel_w, 1024);

        /* 1. Check file exists */
        DWORD attr = GetFileAttributesW(sel_w);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            return STATUS_ERROR_PATH_NOT_FOUND;
        }

        /* 2. Check not a reparse point */
        if (quarantine_is_reparse_point(sel_w)) {
            return STATUS_ERROR_IS_REPARSE_POINT;
        }

        /* 3. Re-hash SHA-256 and compare with recorded digest */
        uint8_t dummy_bin[SHA256_DIGEST_LENGTH];
        char current_hash[SHA256_HEX_LENGTH] = {0};
        HashStatus h_status = HASH_STATUS_PENDING;
        StatusCode sc_hash = sha256_calculate_file(sel_w, dummy_bin, current_hash, &h_status);
        if (sc_hash != STATUS_SUCCESS || strcmp(current_hash, target_group->sha256_hex) != 0) {
            return STATUS_ERROR_CONCURRENT_MODIFICATION;
        }

        /* 4. Byte-by-byte comparison against retained copy */
        bool identical = false;
        StatusCode sc_cmp = file_compare_byte_by_byte_w(sel_w, retained_w, &identical);
        if (sc_cmp != STATUS_SUCCESS || !identical) {
            return STATUS_ERROR_CONCURRENT_MODIFICATION;
        }

        /* 5. Hard link detection (do not claim independent recoverable bytes for hard-linked duplicate paths) */
        if (!quarantine_is_hardlink(sel_w, retained_w)) {
            total_recoverable += rec->file_size;
        }
    }

    out_preview->estimated_recoverable_bytes = total_recoverable;

    /* Store preview in manager cache */
    quarantine_manager_add_preview(mgr, out_preview);

    return STATUS_SUCCESS;
}

StatusCode quarantine_execute(QuarantineManager *mgr,
                              DuplicateGroupList *groups,
                              FileInventory *inventory,
                              const char *preview_id,
                              size_t *out_quarantined_count,
                              char out_tx_ids[32][64]) {
    if (!mgr || !preview_id || !out_quarantined_count) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }
    *out_quarantined_count = 0;

    /* Locate preview in cache */
    QuarantinePreview *preview = NULL;
    for (size_t i = 0; i < mgr->preview_count; i++) {
        if (strcmp(mgr->previews[i].preview_id, preview_id) == 0) {
            preview = &mgr->previews[i];
            break;
        }
    }

    if (!preview) {
        return STATUS_ERROR_PREVIEW_NOT_FOUND;
    }

    if (preview->is_used) {
        return STATUS_ERROR_PREVIEW_ALREADY_USED;
    }

    if (time(NULL) > preview->expires_at) {
        return STATUS_ERROR_PREVIEW_EXPIRED;
    }

    /* Invalidate preview token immediately so it cannot be reused */
    preview->is_used = true;

    /* Execute safe same-volume quarantine move for each file */
    size_t moved = 0;
    for (size_t i = 0; i < preview->file_count; i++) {
        wchar_t src_w[1024];
        MultiByteToWideChar(CP_UTF8, 0, preview->file_paths[i], -1, src_w, 1024);

        /* Ensure source file exists */
        DWORD src_attr = GetFileAttributesW(src_w);
        if (src_attr == INVALID_FILE_ATTRIBUTES) {
            return STATUS_ERROR_PATH_NOT_FOUND;
        }

        /* Generate transaction ID */
        char tx_id[64];
        generate_secure_token("tx", tx_id, sizeof(tx_id));

        /* Generate destination path in .datavault_quarantine */
        wchar_t dest_w[1024];
        if (!quarantine_generate_dest_path(src_w, tx_id, dest_w, 1024)) {
            continue;
        }

        /* Check same volume constraint */
        if (!quarantine_is_same_volume(src_w, dest_w)) {
            return STATUS_ERROR_CROSS_VOLUME_MOVE;
        }

        /* Ensure destination does not exist */
        if (GetFileAttributesW(dest_w) != INVALID_FILE_ATTRIBUTES) {
            continue;
        }

        /* Recompute SHA-256 one last time before physical move */
        uint8_t dummy_bin[SHA256_DIGEST_LENGTH];
        char final_sha[SHA256_HEX_LENGTH] = {0};
        HashStatus h_status = HASH_STATUS_PENDING;
        if (sha256_calculate_file(src_w, dummy_bin, final_sha, &h_status) != STATUS_SUCCESS) {
            return STATUS_ERROR_CONCURRENT_MODIFICATION;
        }

        if (preview->expected_sha256[0] != '\0' && strcmp(final_sha, preview->expected_sha256) != 0) {
            return STATUS_ERROR_CONCURRENT_MODIFICATION;
        }

        /* Perform physical move: MOVEFILE_WRITE_THROUGH (NO MOVEFILE_COPY_ALLOWED) */
        if (!MoveFileExW(src_w, dest_w, MOVEFILE_WRITE_THROUGH)) {
            DWORD err = GetLastError();
            (void)err;
            continue;
        }

        /* Convert destination back to UTF-8 for manifest */
        char dest_utf8[1024];
        WideCharToMultiByte(CP_UTF8, 0, dest_w, -1, dest_utf8, 1024, NULL, NULL);

        /* Extract filename */
        const char *slash = strrchr(preview->file_paths[i], '\\');
        const char *fwd = strrchr(preview->file_paths[i], '/');
        if (fwd > slash) slash = fwd;
        const char *base_name = slash ? slash + 1 : preview->file_paths[i];

        /* Build and persist quarantine transaction record */
        QuarantineRecord rec;
        memset(&rec, 0, sizeof(QuarantineRecord));
        strncpy(rec.transaction_id, tx_id, sizeof(rec.transaction_id) - 1);
        rec.group_id = preview->group_id;
        rec.file_record_id = preview->file_ids[i];
        strncpy(rec.original_path, preview->file_paths[i], sizeof(rec.original_path) - 1);
        strncpy(rec.quarantine_path, dest_utf8, sizeof(rec.quarantine_path) - 1);
        strncpy(rec.original_filename, base_name, sizeof(rec.original_filename) - 1);
        rec.file_size = preview->file_sizes[i];
        strncpy(rec.sha256_hex, final_sha, sizeof(rec.sha256_hex) - 1);
        rec.quarantine_timestamp = (int64_t)time(NULL);
        rec.restore_timestamp = 0;
        rec.status = QUARANTINE_STATUS_ACTIVE;

        quarantine_manifest_add(&mgr->manifest, &rec);

        if (out_tx_ids) {
            strncpy(out_tx_ids[moved], tx_id, 63);
            out_tx_ids[moved][63] = '\0';
        }
        moved++;

        /* Update in-memory duplicate group if available */
        if (groups) {
            for (DuplicateGroup *g = groups->head; g != NULL; g = g->next) {
                if (g->group_id == preview->group_id) {
                    duplicate_group_remove_file(g, preview->file_ids[i]);
                    break;
                }
            }
        }

        /* Mark in file inventory if available */
        if (inventory) {
            for (size_t inv_i = 0; inv_i < inventory->count; inv_i++) {
                if (inventory->records[inv_i].record_id == preview->file_ids[i]) {
                    inventory->records[inv_i].hash_status = HASH_STATUS_READ_ERROR; /* marked as inactive */
                    break;
                }
            }
        }
    }

    *out_quarantined_count = moved;
    return (moved > 0) ? STATUS_SUCCESS : STATUS_ERROR_QUARANTINE_FAILED;
}

StatusCode quarantine_restore_preview(QuarantineManager *mgr,
                                      const char *transaction_id,
                                      QuarantinePreview *out_preview) {
    if (!mgr || !transaction_id || !out_preview) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    QuarantineRecord *rec = quarantine_manifest_find(&mgr->manifest, transaction_id);
    if (!rec) {
        return STATUS_ERROR_PATH_NOT_FOUND;
    }

    if (rec->status != QUARANTINE_STATUS_ACTIVE) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    wchar_t quar_w[1024];
    wchar_t orig_w[1024];
    MultiByteToWideChar(CP_UTF8, 0, rec->quarantine_path, -1, quar_w, 1024);
    MultiByteToWideChar(CP_UTF8, 0, rec->original_path, -1, orig_w, 1024);

    /* 1. Check quarantined file exists */
    if (GetFileAttributesW(quar_w) == INVALID_FILE_ATTRIBUTES) {
        return STATUS_ERROR_PATH_NOT_FOUND;
    }

    /* 2. Check original destination does NOT already exist (prevent accidental overwrite) */
    if (GetFileAttributesW(orig_w) != INVALID_FILE_ATTRIBUTES) {
        return STATUS_ERROR_DESTINATION_EXISTS;
    }

    /* 3. Tamper verification: verify SHA-256 of quarantined file matches manifest digest */
    uint8_t dummy_bin[SHA256_DIGEST_LENGTH];
    char current_sha[SHA256_HEX_LENGTH] = {0};
    HashStatus h_status = HASH_STATUS_PENDING;
    if (sha256_calculate_file(quar_w, dummy_bin, current_sha, &h_status) != STATUS_SUCCESS ||
        strcmp(current_sha, rec->sha256_hex) != 0) {
        return STATUS_ERROR_FILE_TAMPERED;
    }

    /* Generate restore preview */
    memset(out_preview, 0, sizeof(QuarantinePreview));
    generate_secure_token("prev_restore", out_preview->preview_id, sizeof(out_preview->preview_id));
    out_preview->group_id = rec->group_id;
    out_preview->file_ids[0] = rec->file_record_id;
    out_preview->file_sizes[0] = rec->file_size;
    strncpy(out_preview->file_paths[0], rec->original_path, sizeof(out_preview->file_paths[0]) - 1);
    out_preview->file_count = 1;
    out_preview->created_at = (int64_t)time(NULL);
    out_preview->expires_at = out_preview->created_at + QUARANTINE_PREVIEW_EXPIRY_SECONDS;
    out_preview->is_used = false;
    out_preview->is_restore = true;
    strncpy(out_preview->restore_transaction_id, transaction_id, sizeof(out_preview->restore_transaction_id) - 1);

    quarantine_manager_add_preview(mgr, out_preview);

    return STATUS_SUCCESS;
}

StatusCode quarantine_restore_execute(QuarantineManager *mgr,
                                      const char *preview_id,
                                      char *out_restored_path,
                                      size_t max_path_len) {
    if (!mgr || !preview_id) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    QuarantinePreview *preview = NULL;
    for (size_t i = 0; i < mgr->preview_count; i++) {
        if (strcmp(mgr->previews[i].preview_id, preview_id) == 0) {
            preview = &mgr->previews[i];
            break;
        }
    }

    if (!preview) {
        return STATUS_ERROR_PREVIEW_NOT_FOUND;
    }

    if (preview->is_used) {
        return STATUS_ERROR_PREVIEW_ALREADY_USED;
    }

    if (time(NULL) > preview->expires_at) {
        return STATUS_ERROR_PREVIEW_EXPIRED;
    }

    if (!preview->is_restore) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    preview->is_used = true;

    QuarantineRecord *rec = quarantine_manifest_find(&mgr->manifest, preview->restore_transaction_id);
    if (!rec) {
        return STATUS_ERROR_PATH_NOT_FOUND;
    }

    wchar_t quar_w[1024];
    wchar_t orig_w[1024];
    MultiByteToWideChar(CP_UTF8, 0, rec->quarantine_path, -1, quar_w, 1024);
    MultiByteToWideChar(CP_UTF8, 0, rec->original_path, -1, orig_w, 1024);

    /* Verify quarantined file exists */
    if (GetFileAttributesW(quar_w) == INVALID_FILE_ATTRIBUTES) {
        return STATUS_ERROR_PATH_NOT_FOUND;
    }

    /* Verify destination does NOT exist */
    if (GetFileAttributesW(orig_w) != INVALID_FILE_ATTRIBUTES) {
        return STATUS_ERROR_DESTINATION_EXISTS;
    }

    /* Move file back using same-volume move with write-through */
    if (!MoveFileExW(quar_w, orig_w, MOVEFILE_WRITE_THROUGH)) {
        return STATUS_ERROR_RESTORE_FAILED;
    }

    /* Update manifest status */
    quarantine_manifest_update_status(&mgr->manifest, rec->transaction_id, QUARANTINE_STATUS_RESTORED);

    if (out_restored_path && max_path_len > 0) {
        strncpy(out_restored_path, rec->original_path, max_path_len - 1);
        out_restored_path[max_path_len - 1] = '\0';
    }

    return STATUS_SUCCESS;
}
