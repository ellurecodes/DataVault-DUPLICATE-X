/**
 * @file main.c
 * @brief Command-line interface, scanner orchestrator, and duplicate detection reporter.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "engine.h"
#include "file_scanner.h"
#include "sha256.h"
#include "duplicate_detector.h"
#include "status.h"
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

int wmain(int argc, wchar_t *argv[]) {
    /* Step 1: Render project and team identity banner */
    engine_display_banner();

    /* Step 2: Initialize engine state */
    EngineState engine;
    StatusCode status = engine_init(&engine);
    if (status != STATUS_SUCCESS) {
        fprintf(stderr, "[ERROR] Engine initialization failed (%d): %s\n",
                status, status_to_string(status));
        return EXIT_FAILURE;
    }

    /* Step 3: Validate command-line arguments */
    if (argc < 2) {
        printf("\n[USAGE]\n");
        printf("  duplicate-x.exe <directory1> [directory2] ...\n\n");
        printf("[EXAMPLE]\n");
        printf("  duplicate-x.exe \"C:\\Users\\User\\Documents\" \"D:\\Backup\"\n\n");
        printf("[NOTE] DUPLICATE-X will only scan directories explicitly provided above.\n");
        engine_shutdown(&engine);
        return EXIT_SUCCESS;
    }

    size_t num_dirs = (size_t)(argc - 1);
    const wchar_t **selected_dirs = (const wchar_t **)&argv[1];

    printf("\n[CONFIGURED STORAGE LOCATIONS]\n");
    for (size_t i = 0; i < num_dirs; i++) {
        wprintf(L"  [%zu] %ls\n", i + 1, selected_dirs[i]);
    }
    printf("\n");

    /* Step 4: Initialize file inventory and scanner telemetry */
    FileInventory inventory;
    file_inventory_init(&inventory);

    ScannerStats stats;
    scanner_stats_init(&stats);

    printf("[SCAN] Starting recursive filesystem traversal & SHA-256 fingerprinting...\n\n");

    /* Step 5: Execute scan across all targeted directories */
    StatusCode scan_status = scan_multiple_directories(selected_dirs, num_dirs, &inventory, &stats);

    if (scan_status != STATUS_SUCCESS && inventory.count == 0) {
        fprintf(stderr, "[ERROR] Scan encountered errors (%d): %s\n",
                scan_status, status_to_string(scan_status));
    }

    /* Step 6: Render detailed file records and fingerprints */
    printf("========================================================================================================\n");
    printf("%-4s | %-24s | %10s | %-64s\n", "ID", "FILE NAME", "SIZE (B)", "SHA-256 FINGERPRINT");
    printf("========================================================================================================\n");

    size_t successfully_hashed = 0;

    if (inventory.count == 0) {
        printf("  (No files found or accessible in the specified storage locations)\n");
    } else {
        for (size_t i = 0; i < inventory.count; i++) {
            FileRecord *rec = &inventory.records[i];
            bool is_valid_hash = (rec->hash_status == HASH_STATUS_SUCCESS || rec->hash_status == HASH_STATUS_EMPTY_FILE);
            if (is_valid_hash) {
                successfully_hashed++;
            }

            const char *display_hash = is_valid_hash ? rec->sha256_hex : hash_status_to_string(rec->hash_status);

            printf("%-4llu | %-24.24s | %10llu | %-64s\n",
                   (unsigned long long)rec->record_id,
                   rec->filename ? rec->filename : "(unnamed)",
                   (unsigned long long)rec->file_size,
                   display_hash);
        }
    }
    printf("========================================================================================================\n\n");

    /* Step 7: Display scanner summary */
    printf("[SCAN SUMMARY]\n");
    printf("  Target Directories Scanned : %zu\n", stats.directories_scanned);
    printf("  Total Files Discovered     : %zu\n", inventory.count);
    printf("  Successfully Hashed Files  : %zu\n", successfully_hashed);
    printf("  Total Content Processed    : %llu bytes\n", (unsigned long long)inventory.total_bytes);
    printf("  Reparse Points Skipped     : %zu\n", stats.reparse_points_skipped);
    printf("  Errors / Inaccessible Paths: %zu\n\n", stats.errors_encountered);

    /* Step 8: Execute Duplicate Detection using Hash Table & Byte Verification */
    printf("[DEDUPLICATION] Running separate-chaining candidate indexing & byte-by-byte verification...\n\n");

    DuplicateGroupList duplicate_groups;
    duplicate_detector_process(&inventory, &duplicate_groups);

    /* Step 9: Render the official Duplicate Detection Report */
    duplicate_report_print(&duplicate_groups, inventory.count, successfully_hashed);

    /* Step 10: Display Top K storage recovery opportunities via Binary Min Heap (K = 5) */
    duplicate_report_print_top_k(&duplicate_groups, 5);

    /* Step 11: Safe cleanup and shutdown */
    duplicate_group_list_free(&duplicate_groups);
    file_inventory_free(&inventory);
    engine_shutdown(&engine);

    return EXIT_SUCCESS;
}
