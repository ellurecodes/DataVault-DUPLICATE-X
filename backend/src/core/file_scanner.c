/**
 * @file file_scanner.c
 * @brief Dynamic file inventory management and scanner coordinator.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "file_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_INVENTORY_CAPACITY 64

void scanner_stats_init(ScannerStats *stats) {
    if (!stats) return;
    stats->directories_scanned = 0;
    stats->files_scanned = 0;
    stats->reparse_points_skipped = 0;
    stats->errors_encountered = 0;
    stats->total_bytes_scanned = 0;
}

void file_inventory_init(FileInventory *inv) {
    if (!inv) return;
    inv->records = NULL;
    inv->count = 0;
    inv->capacity = 0;
    inv->total_bytes = 0;
}

void file_record_cleanup(FileRecord *rec) {
    if (!rec) return;
    if (rec->filename) {
        free(rec->filename);
        rec->filename = NULL;
    }
    if (rec->filepath) {
        free(rec->filepath);
        rec->filepath = NULL;
    }
    if (rec->extension) {
        free(rec->extension);
        rec->extension = NULL;
    }
    if (rec->device_id) {
        free(rec->device_id);
        rec->device_id = NULL;
    }
}

void file_inventory_free(FileInventory *inv) {
    if (!inv) return;
    if (inv->records) {
        for (size_t i = 0; i < inv->count; i++) {
            file_record_cleanup(&inv->records[i]);
        }
        free(inv->records);
        inv->records = NULL;
    }
    inv->count = 0;
    inv->capacity = 0;
    inv->total_bytes = 0;
}

StatusCode file_inventory_add(FileInventory *inv, const FileRecord *rec) {
    if (!inv || !rec) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    /* Grow dynamically if capacity is reached (growth factor 2x) */
    if (inv->count >= inv->capacity) {
        size_t new_cap = (inv->capacity == 0) ? INITIAL_INVENTORY_CAPACITY : inv->capacity * 2;
        FileRecord *new_records = (FileRecord *)realloc(inv->records, new_cap * sizeof(FileRecord));
        if (!new_records) {
            return STATUS_ERROR_OUT_OF_MEMORY;
        }
        inv->records = new_records;
        inv->capacity = new_cap;
    }

    FileRecord *dest = &inv->records[inv->count];
    dest->record_id = (uint64_t)(inv->count + 1);
    dest->file_size = rec->file_size;
    dest->modified_time = rec->modified_time;
    dest->hash_status = rec->hash_status;

    memcpy(dest->sha256_binary, rec->sha256_binary, sizeof(dest->sha256_binary));
    memcpy(dest->sha256_hex, rec->sha256_hex, sizeof(dest->sha256_hex));

    /* Duplicate dynamic strings */
    dest->filename = rec->filename ? strdup(rec->filename) : NULL;
    dest->filepath = rec->filepath ? strdup(rec->filepath) : NULL;
    dest->extension = rec->extension ? strdup(rec->extension) : NULL;
    dest->device_id = rec->device_id ? strdup(rec->device_id) : NULL;

    inv->count++;
    inv->total_bytes += rec->file_size;

    return STATUS_SUCCESS;
}

const char *hash_status_to_string(HashStatus status) {
    switch (status) {
        case HASH_STATUS_PENDING:
            return "Pending";
        case HASH_STATUS_SUCCESS:
            return "Valid";
        case HASH_STATUS_EMPTY_FILE:
            return "Empty File";
        case HASH_STATUS_READ_ERROR:
            return "Read Error";
        case HASH_STATUS_MODIFIED_DURING_SCAN:
            return "Modified During Scan";
        case HASH_STATUS_DISAPPEARED:
            return "File Disappeared";
        case HASH_STATUS_PERMISSION_DENIED:
            return "Permission Denied";
        default:
            return "Unknown";
    }
}

StatusCode scan_multiple_directories(const wchar_t **root_paths, size_t num_paths,
                                     FileInventory *inventory, ScannerStats *stats) {
    if (!root_paths || num_paths == 0 || !inventory) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    StatusCode overall_status = STATUS_SUCCESS;

    for (size_t i = 0; i < num_paths; i++) {
        if (!root_paths[i]) continue;
        StatusCode sc = scan_directory(root_paths[i], inventory, stats);
        if (sc != STATUS_SUCCESS && overall_status == STATUS_SUCCESS) {
            overall_status = sc;
        }
    }

    return overall_status;
}

