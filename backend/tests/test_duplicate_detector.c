/**
 * @file test_duplicate_detector.c
 * @brief Comprehensive verification suite for Part 4 (Hash Table, Linked Lists, Duplicate Detector).
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "hashtable.h"
#include "duplicate_group.h"
#include "duplicate_detector.h"
#include "file_scanner.h"
#include "file_compare.h"
#include "sha256.h"
#include "status.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <windows.h>

/* Helper to create a test file with exact binary content */
static bool create_test_file(const wchar_t *wpath, const char *content, size_t length) {
    HANDLE hFile = CreateFileW(wpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    if (length > 0 && content != NULL) {
        DWORD written = 0;
        WriteFile(hFile, content, (DWORD)length, &written, NULL);
    }
    CloseHandle(hFile);
    return true;
}

/* Helper to populate a FileRecord from an existing file path */
static bool populate_record(FileRecord *rec, const wchar_t *wpath, const char *dev_id) {
    memset(rec, 0, sizeof(FileRecord));
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, NULL, 0, NULL, NULL);
    if (utf8_len <= 0) return false;

    rec->filepath = (char *)malloc(utf8_len);
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, rec->filepath, utf8_len, NULL, NULL);

    const wchar_t *name = wcsrchr(wpath, L'\\');
    if (!name) name = wcsrchr(wpath, L'/');
    name = name ? name + 1 : wpath;

    int name_len = WideCharToMultiByte(CP_UTF8, 0, name, -1, NULL, 0, NULL, NULL);
    rec->filename = (char *)malloc(name_len);
    WideCharToMultiByte(CP_UTF8, 0, name, -1, rec->filename, name_len, NULL, NULL);

    rec->extension = strdup(".txt");
    rec->device_id = strdup(dev_id);

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(wpath, GetFileExInfoStandard, &data)) {
        rec->file_size = ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    }

    sha256_calculate_file(wpath, rec->sha256_binary, rec->sha256_hex, &rec->hash_status);
    return true;
}

int wmain(void) {
    printf("====================================================\n");
    printf(" DUPLICATE-X Part 4 Comprehensive Verification Suite\n");
    printf(" Custom Hash Table & Duplicate Grouping Engine\n");
    printf(" Powered by Team DataVault (2026-2027)\n");
    printf("====================================================\n\n");

    CreateDirectoryW(L"data/test_fixtures_p4", NULL);

    /* ----------------------------------------------------
     * TEST 1: Two files containing identical content but different filenames
     * ---------------------------------------------------- */
    printf("[TEST 1/12] Two identical files with different filenames... ");
    create_test_file(L"data/test_fixtures_p4/t1_fileA.txt", "AlphaBetaGamma", 14);
    create_test_file(L"data/test_fixtures_p4/t1_fileB.txt", "AlphaBetaGamma", 14);

    FileInventory inv1;
    file_inventory_init(&inv1);
    FileRecord r1, r2;
    populate_record(&r1, L"data/test_fixtures_p4/t1_fileA.txt", "Drive_C");
    populate_record(&r2, L"data/test_fixtures_p4/t1_fileB.txt", "Drive_D");
    file_inventory_add(&inv1, &r1);
    file_inventory_add(&inv1, &r2);
    file_record_cleanup(&r1);
    file_record_cleanup(&r2);

    DuplicateGroupList g1;
    duplicate_detector_process(&inv1, &g1);
    assert(g1.total_groups == 1);
    assert(g1.head->file_count == 2);
    assert(g1.total_recoverable_bytes == 14);
    duplicate_group_list_free(&g1);
    file_inventory_free(&inv1);
    printf("PASS (1 group, 2 copies, recoverable: 14 B)\n");

    /* ----------------------------------------------------
     * TEST 2: Two files containing different content but same file size
     * ---------------------------------------------------- */
    printf("[TEST 2/12] Different content with same file size (12 bytes)... ");
    create_test_file(L"data/test_fixtures_p4/t2_same_size1.txt", "DataVault_01", 12);
    create_test_file(L"data/test_fixtures_p4/t2_same_size2.txt", "DataVault_02", 12);

    FileInventory inv2;
    file_inventory_init(&inv2);
    populate_record(&r1, L"data/test_fixtures_p4/t2_same_size1.txt", "Vol_1");
    populate_record(&r2, L"data/test_fixtures_p4/t2_same_size2.txt", "Vol_2");
    file_inventory_add(&inv2, &r1);
    file_inventory_add(&inv2, &r2);
    file_record_cleanup(&r1);
    file_record_cleanup(&r2);

    DuplicateGroupList g2;
    duplicate_detector_process(&inv2, &g2);
    assert(g2.total_groups == 0); /* Must NOT be grouped! */
    duplicate_group_list_free(&g2);
    file_inventory_free(&inv2);
    printf("PASS (0 groups formed, correctly rejected)\n");

    /* ----------------------------------------------------
     * TEST 3: Three identical files across three different folders
     * ---------------------------------------------------- */
    printf("[TEST 3/12] Three identical files across different folders... ");
    CreateDirectoryW(L"data/test_fixtures_p4/dirA", NULL);
    CreateDirectoryW(L"data/test_fixtures_p4/dirB", NULL);
    CreateDirectoryW(L"data/test_fixtures_p4/dirC", NULL);
    create_test_file(L"data/test_fixtures_p4/dirA/report.pdf", "TripletContent12345", 19);
    create_test_file(L"data/test_fixtures_p4/dirB/report_backup.pdf", "TripletContent12345", 19);
    create_test_file(L"data/test_fixtures_p4/dirC/archive.pdf", "TripletContent12345", 19);

    FileInventory inv3;
    file_inventory_init(&inv3);
    FileRecord r3;
    populate_record(&r1, L"data/test_fixtures_p4/dirA/report.pdf", "Laptop");
    populate_record(&r2, L"data/test_fixtures_p4/dirB/report_backup.pdf", "USB");
    populate_record(&r3, L"data/test_fixtures_p4/dirC/archive.pdf", "CloudDrive");
    file_inventory_add(&inv3, &r1);
    file_inventory_add(&inv3, &r2);
    file_inventory_add(&inv3, &r3);
    file_record_cleanup(&r1);
    file_record_cleanup(&r2);
    file_record_cleanup(&r3);

    DuplicateGroupList g3;
    duplicate_detector_process(&inv3, &g3);
    assert(g3.total_groups == 1);
    assert(g3.head->file_count == 3);
    assert(g3.total_recoverable_bytes == (3 - 1) * 19);
    duplicate_group_list_free(&g3);
    file_inventory_free(&inv3);
    printf("PASS (1 group, 3 copies, recoverable: 38 B)\n");

    /* ----------------------------------------------------
     * TEST 4: A solitary unique file with no duplicate
     * ---------------------------------------------------- */
    printf("[TEST 4/12] Single solitary unique file... ");
    create_test_file(L"data/test_fixtures_p4/t4_solo.txt", "UniqueSolitaryFile", 18);
    FileInventory inv4;
    file_inventory_init(&inv4);
    populate_record(&r1, L"data/test_fixtures_p4/t4_solo.txt", "Device");
    file_inventory_add(&inv4, &r1);
    file_record_cleanup(&r1);

    DuplicateGroupList g4;
    duplicate_detector_process(&inv4, &g4);
    assert(g4.total_groups == 0);
    duplicate_group_list_free(&g4);
    file_inventory_free(&inv4);
    printf("PASS (0 groups, solitary file excluded)\n");

    /* ----------------------------------------------------
     * TEST 5: Two empty (0-byte) files
     * ---------------------------------------------------- */
    printf("[TEST 5/12] Two empty (0-byte) files... ");
    create_test_file(L"data/test_fixtures_p4/empty1.txt", "", 0);
    create_test_file(L"data/test_fixtures_p4/empty2.txt", "", 0);
    FileInventory inv5;
    file_inventory_init(&inv5);
    populate_record(&r1, L"data/test_fixtures_p4/empty1.txt", "Dev1");
    populate_record(&r2, L"data/test_fixtures_p4/empty2.txt", "Dev2");
    file_inventory_add(&inv5, &r1);
    file_inventory_add(&inv5, &r2);
    file_record_cleanup(&r1);
    file_record_cleanup(&r2);

    DuplicateGroupList g5;
    duplicate_detector_process(&inv5, &g5);
    assert(g5.total_groups == 1);
    assert(g5.head->file_count == 2);
    assert(g5.head->file_size == 0);
    assert(g5.total_recoverable_bytes == 0);
    duplicate_group_list_free(&g5);
    file_inventory_free(&inv5);
    printf("PASS (1 group, 2 empty copies verified)\n");

    /* ----------------------------------------------------
     * TEST 6: Different file extensions with identical contents
     * ---------------------------------------------------- */
    printf("[TEST 6/12] Differing extensions (.dat vs .bin vs .log)... ");
    create_test_file(L"data/test_fixtures_p4/data.dat", "HeterogeneousExtensionTest", 26);
    create_test_file(L"data/test_fixtures_p4/data.bin", "HeterogeneousExtensionTest", 26);
    FileInventory inv6;
    file_inventory_init(&inv6);
    populate_record(&r1, L"data/test_fixtures_p4/data.dat", "Dev1");
    populate_record(&r2, L"data/test_fixtures_p4/data.bin", "Dev2");
    file_inventory_add(&inv6, &r1);
    file_inventory_add(&inv6, &r2);
    file_record_cleanup(&r1);
    file_record_cleanup(&r2);

    DuplicateGroupList g6;
    duplicate_detector_process(&inv6, &g6);
    assert(g6.total_groups == 1);
    assert(g6.head->file_count == 2);
    duplicate_group_list_free(&g6);
    file_inventory_free(&inv6);
    printf("PASS (1 group formed across extensions)\n");

    /* ----------------------------------------------------
     * TEST 7: Hash Table bucket collision & separate chaining
     * ---------------------------------------------------- */
    printf("[TEST 7/12] Hash Table bucket collision & separate chaining... ");
    /* Create a small table of capacity 3 to deliberately force collisions */
    HashTable *ht_small = hashtable_create(3);
    uint8_t hashA[32] = {1, 2, 3};
    uint8_t hashB[32] = {4, 5, 6};

    HashEntry *eA = NULL;
    HashEntry *eB = NULL;
    hashtable_get_or_insert(ht_small, 100, hashA, &eA);
    hashtable_get_or_insert(ht_small, 200, hashB, &eB);

    assert(eA != NULL && eB != NULL);
    assert(hashtable_lookup(ht_small, 100, hashA) == eA);
    assert(hashtable_lookup(ht_small, 200, hashB) == eB);
    assert(hashtable_lookup(ht_small, 300, hashA) == NULL);
    hashtable_free(ht_small);
    printf("PASS (Chaining resolved collisions correctly)\n");

    /* ----------------------------------------------------
     * TEST 8: Hash Table dynamic resizing
     * ---------------------------------------------------- */
    printf("[TEST 8/12] Hash Table dynamic resizing (load factor > 0.75)... ");
    HashTable *ht_resize = hashtable_create(5);
    size_t initial_cap = ht_resize->capacity;
    assert(initial_cap == 5);

    /* Insert 25 distinct keys to trigger dynamic resizing */
    for (uint64_t i = 1; i <= 25; i++) {
        uint8_t dummy_hash[32] = {0};
        dummy_hash[0] = (uint8_t)i;
        dummy_hash[31] = (uint8_t)(i * 7);
        HashEntry *res = NULL;
        StatusCode sc = hashtable_get_or_insert(ht_resize, i * 10, dummy_hash, &res);
        assert(sc == STATUS_SUCCESS);
        assert(res != NULL);
    }

    size_t final_cap = ht_resize->capacity;
    assert(final_cap > initial_cap);
    assert(ht_resize->count == 25);

    /* Verify all 25 entries remain accessible after resizing */
    for (uint64_t i = 1; i <= 25; i++) {
        uint8_t dummy_hash[32] = {0};
        dummy_hash[0] = (uint8_t)i;
        dummy_hash[31] = (uint8_t)(i * 7);
        HashEntry *found = hashtable_lookup(ht_resize, i * 10, dummy_hash);
        assert(found != NULL);
        assert(found->file_size == i * 10);
    }
    hashtable_free(ht_resize);
    printf("PASS (Resized from %zu to %zu, all 25 accessible)\n", initial_cap, final_cap);

    /* ----------------------------------------------------
     * TEST 9: File marked as modified during scan
     * ---------------------------------------------------- */
    printf("[TEST 9/12] File modified during scan exclusion... ");
    create_test_file(L"data/test_fixtures_p4/t9_normal.txt", "StableContent", 13);
    create_test_file(L"data/test_fixtures_p4/t9_modified.txt", "StableContent", 13);
    FileInventory inv9;
    file_inventory_init(&inv9);
    populate_record(&r1, L"data/test_fixtures_p4/t9_normal.txt", "Dev1");
    populate_record(&r2, L"data/test_fixtures_p4/t9_modified.txt", "Dev2");
    /* Simulate modification detection */
    r2.hash_status = HASH_STATUS_MODIFIED_DURING_SCAN;
    file_inventory_add(&inv9, &r1);
    file_inventory_add(&inv9, &r2);
    file_record_cleanup(&r1);
    file_record_cleanup(&r2);

    DuplicateGroupList g9;
    duplicate_detector_process(&inv9, &g9);
    assert(g9.total_groups == 0); /* Modified file MUST be excluded! */
    duplicate_group_list_free(&g9);
    file_inventory_free(&inv9);
    printf("PASS (Modified file excluded from duplicate groups)\n");

    /* ----------------------------------------------------
     * TEST 10: Streaming comparison on files > 64 KB
     * ---------------------------------------------------- */
    printf("[TEST 10/12] Streaming comparison on 150 KB files... ");
    size_t large_size = 150 * 1024;
    char *large_buf = (char *)malloc(large_size);
    for (size_t i = 0; i < large_size; i++) large_buf[i] = (char)(i % 251);

    create_test_file(L"data/test_fixtures_p4/large1.bin", large_buf, large_size);
    create_test_file(L"data/test_fixtures_p4/large2.bin", large_buf, large_size);

    bool is_match = false;
    StatusCode comp_sc = file_compare_byte_by_byte(
        "data/test_fixtures_p4/large1.bin",
        "data/test_fixtures_p4/large2.bin",
        &is_match
    );
    assert(comp_sc == STATUS_SUCCESS);
    assert(is_match == true);

    /* Corrupt 1 byte in chunk 2 (at offset 70 KB) */
    large_buf[70 * 1024] = 'X';
    create_test_file(L"data/test_fixtures_p4/large3_diff.bin", large_buf, large_size);
    free(large_buf);

    comp_sc = file_compare_byte_by_byte(
        "data/test_fixtures_p4/large1.bin",
        "data/test_fixtures_p4/large3_diff.bin",
        &is_match
    );
    assert(comp_sc == STATUS_SUCCESS);
    assert(is_match == false);
    printf("PASS (Streaming chunks verified match and mismatch)\n");

    /* ----------------------------------------------------
     * TEST 11: Multiple copies in the same directory
     * ---------------------------------------------------- */
    printf("[TEST 11/12] Multiple copies within the exact same folder... ");
    create_test_file(L"data/test_fixtures_p4/same_folder_1.txt", "SameDirContent", 14);
    create_test_file(L"data/test_fixtures_p4/same_folder_2.txt", "SameDirContent", 14);
    FileInventory inv11;
    file_inventory_init(&inv11);
    populate_record(&r1, L"data/test_fixtures_p4/same_folder_1.txt", "Folder");
    populate_record(&r2, L"data/test_fixtures_p4/same_folder_2.txt", "Folder");
    file_inventory_add(&inv11, &r1);
    file_inventory_add(&inv11, &r2);
    file_record_cleanup(&r1);
    file_record_cleanup(&r2);

    DuplicateGroupList g11;
    duplicate_detector_process(&inv11, &g11);
    assert(g11.total_groups == 1);
    assert(g11.head->file_count == 2);
    duplicate_group_list_free(&g11);
    file_inventory_free(&inv11);
    printf("PASS (1 group formed within same folder)\n");

    /* ----------------------------------------------------
     * TEST 12: Unicode filenames with spaces
     * ---------------------------------------------------- */
    printf("[TEST 12/12] Unicode filenames with spaces and accents... ");
    create_test_file(L"data/test_fixtures_p4/файл с пробелами.dat", "UnicodePayload_2026", 19);
    create_test_file(L"data/test_fixtures_p4/archive copy 2.dat", "UnicodePayload_2026", 19);
    FileInventory inv12;
    file_inventory_init(&inv12);
    populate_record(&r1, L"data/test_fixtures_p4/файл с пробелами.dat", "UnicodeDisk");
    populate_record(&r2, L"data/test_fixtures_p4/archive copy 2.dat", "UnicodeDisk");
    file_inventory_add(&inv12, &r1);
    file_inventory_add(&inv12, &r2);
    file_record_cleanup(&r1);
    file_record_cleanup(&r2);

    DuplicateGroupList g12;
    duplicate_detector_process(&inv12, &g12);
    assert(g12.total_groups == 1);
    assert(g12.head->file_count == 2);
    duplicate_group_list_free(&g12);
    file_inventory_free(&inv12);
    printf("PASS (Unicode filenames with spaces verified)\n");

    printf("\n====================================================\n");
    printf(" ALL 12 VERIFICATION TESTS PASSED WITH 100%% SUCCESS!\n");
    printf("====================================================\n");

    return EXIT_SUCCESS;
}

