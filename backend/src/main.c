/**
 * @file main.c
 * @brief Command-line interface, scanner orchestrator, and duplicate detection reporter.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "engine.h"
#include "file_scanner.h"
#include "sha256.h"
#include "duplicate_detector.h"
#include "trie.h"
#include "api_server.h"
#include "status.h"
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

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

    /* Check for --server mode */
    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"--server") == 0) {
            ApiEngineContext api_ctx;
            api_engine_context_init(&api_ctx);
            StatusCode srv_sc = api_server_run(&api_ctx, API_SERVER_DEFAULT_PORT);
            api_engine_context_cleanup(&api_ctx);
            engine_shutdown(&engine);
            return (srv_sc == STATUS_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }

    /* Step 3: Parse command-line arguments (--prefix optional flag) */
    const wchar_t *query_prefix_w = NULL;

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"--prefix") == 0) {
            if (i + 1 < argc) {
                query_prefix_w = argv[i + 1];
                i++; /* skip prefix argument value */
            }
        }
    }

    /* Collect target directory arguments (excluding flags and arguments) */
    const wchar_t *selected_dirs[64];
    size_t num_dirs = 0;

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"--prefix") == 0) {
            i++; /* skip flag value */
            continue;
        }
        if (num_dirs < 64) {
            selected_dirs[num_dirs++] = argv[i];
        }
    }

    if (num_dirs == 0) {
        printf("\n[USAGE]\n");
        printf("  duplicate-x.exe [--server] [--prefix <path_prefix>] <directory1> [directory2] ...\n\n");
        printf("[EXAMPLES]\n");
        printf("  duplicate-x.exe --server\n");
        printf("  duplicate-x.exe \"C:\\Users\\User\\Documents\" \"D:\\Backup\"\n");
        printf("  duplicate-x.exe --prefix \"C:\\Users\\User\\Documents\\Notes\" \"C:\\Users\\User\\Documents\"\n\n");
        printf("[NOTE] DUPLICATE-X will only scan directories explicitly provided above.\n");
        engine_shutdown(&engine);
        return EXIT_SUCCESS;
    }

    printf("\n[CONFIGURED STORAGE LOCATIONS]\n");
    for (size_t i = 0; i < num_dirs; i++) {
        wprintf(L"  [%zu] %ls\n", i + 1, selected_dirs[i]);
    }
    if (query_prefix_w) {
        wprintf(L"  [PREFIX FILTER] %ls\n", query_prefix_w);
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

    /* Step 11: Build Custom Prefix Trie and execute prefix search if requested */
    Trie *trie = trie_init();
    if (trie) {
        for (size_t i = 0; i < inventory.count; i++) {
            const FileRecord *rec = &inventory.records[i];
            if (rec->filepath) {
                trie_insert(trie, rec->filepath, rec);
            }
        }

        if (query_prefix_w) {
            char query_prefix_utf8[4096];
            WideCharToMultiByte(CP_UTF8, 0, query_prefix_w, -1, query_prefix_utf8, sizeof(query_prefix_utf8), NULL, NULL);

            TrieSearchResults search_results;
            trie_search_prefix(trie, query_prefix_utf8, &search_results);
            trie_print_matches(&search_results, query_prefix_utf8);
            trie_search_results_free(&search_results);
        }

        trie_free(trie);
    }

    /* Step 12: Safe cleanup and shutdown */
    duplicate_group_list_free(&duplicate_groups);
    file_inventory_free(&inventory);
    engine_shutdown(&engine);

    return EXIT_SUCCESS;
}
