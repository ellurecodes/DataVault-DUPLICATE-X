/**
 * @file test_scanner_sha256.c
 * @brief Comprehensive verification suite for File Scanner & SHA-256 Engine.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "file_scanner.h"
#include "sha256.h"
#include "status.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <wchar.h>

#define KNOWN_ABC_HASH   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
#define KNOWN_EMPTY_HASH "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

int wmain(void) {
    printf("==========================================\n");
    printf(" DUPLICATE-X Part 3 Unit & Verification Suite\n");
    printf(" Powered by Team DataVault (2026-2027)\n");
    printf("==========================================\n\n");

    /* TEST 1 & 3: SHA-256 Known Test Vector ("abc") */
    printf("[TEST 1/8] Verifying SHA-256 of 'abc' (notes.txt)... ");
    uint8_t bin[32];
    char hex[65];
    HashStatus status;
    StatusCode sc = sha256_calculate_file(L"data/test_files/device_laptop/notes.txt", bin, hex, &status);
    assert(sc == STATUS_SUCCESS);
    assert(status == HASH_STATUS_SUCCESS);
    assert(strcmp(hex, KNOWN_ABC_HASH) == 0);
    printf("PASS\n      Hash: %s\n", hex);

    /* TEST 4: Verifying renamed file content match */
    printf("[TEST 2/8] Verifying renamed_notes.txt has identical hash to notes.txt... ");
    uint8_t bin2[32];
    char hex2[65];
    sc = sha256_calculate_file(L"data/test_files/device_usb/renamed_notes.txt", bin2, hex2, &status);
    assert(sc == STATUS_SUCCESS);
    assert(strcmp(hex, hex2) == 0);
    printf("PASS\n      Hash: %s\n", hex2);

    /* TEST 5: Verifying unique.txt content differs */
    printf("[TEST 3/8] Verifying unique.txt produces distinct hash... ");
    uint8_t bin3[32];
    char hex3[65];
    sc = sha256_calculate_file(L"data/test_files/device_laptop/unique.txt", bin3, hex3, &status);
    assert(sc == STATUS_SUCCESS);
    assert(strcmp(hex, hex3) != 0);
    printf("PASS\n      Hash: %s\n", hex3);

    /* TEST 7: Empty file hashing */
    printf("[TEST 4/8] Verifying 0-byte file hashing (empty.txt)... ");
    uint8_t bin_empty[32];
    char hex_empty[65];
    sc = sha256_calculate_file(L"data/test_files/empty.txt", bin_empty, hex_empty, &status);
    assert(sc == STATUS_SUCCESS);
    assert(status == HASH_STATUS_EMPTY_FILE);
    assert(strcmp(hex_empty, KNOWN_EMPTY_HASH) == 0);
    printf("PASS\n      Hash: %s\n", hex_empty);

    /* TEST 2 & 6: Multi-directory recursive scanning */
    printf("[TEST 5/8] Scanning two directories together (device_laptop & device_usb)... ");
    FileInventory inv;
    file_inventory_init(&inv);
    ScannerStats stats;
    scanner_stats_init(&stats);

    const wchar_t *roots[] = {
        L"data/test_files/device_laptop",
        L"data/test_files/device_usb"
    };

    sc = scan_multiple_directories(roots, 2, &inv, &stats);
    assert(sc == STATUS_SUCCESS);
    assert(inv.count == 3);
    assert(stats.files_scanned == 3);
    assert(stats.directories_scanned == 2);
    assert(inv.total_bytes == 9);
    printf("PASS\n      Discovered: %zu files across %zu directories (Total bytes: %llu)\n",
           inv.count, stats.directories_scanned, (unsigned long long)inv.total_bytes);

    /* TEST 8: Missing directory error handling */
    printf("[TEST 6/8] Testing missing directory handling (data/test_files/non_existent)... ");
    sc = scan_directory(L"data/test_files/non_existent_folder", &inv, &stats);
    assert(sc == STATUS_ERROR_PATH_NOT_FOUND);
    printf("PASS (Gracefully caught STATUS_ERROR_PATH_NOT_FOUND)\n");

    /* TEST 9: Memory deallocation */
    printf("[TEST 7/8] Verifying memory cleanup of FileInventory... ");
    file_inventory_free(&inv);
    assert(inv.count == 0);
    assert(inv.records == NULL);
    printf("PASS\n");

    /* TEST 10: Concurrency safety check simulation (non-existent file) */
    printf("[TEST 8/8] Verifying disappearing file safety check... ");
    sc = sha256_calculate_file(L"data/test_files/missing_file.tmp", bin, hex, &status);
    assert(sc == STATUS_ERROR_PATH_NOT_FOUND);
    assert(status == HASH_STATUS_DISAPPEARED);
    printf("PASS (Correctly flagged HASH_STATUS_DISAPPEARED)\n");

    printf("\n==========================================\n");
    printf(" ALL 8 UNIT & INTEGRATION TESTS PASSED!\n");
    printf("==========================================\n");

    return EXIT_SUCCESS;
}

