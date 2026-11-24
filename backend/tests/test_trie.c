/**
 * @file test_trie.c
 * @brief Comprehensive 18-scenario verification test suite for Part 5B Prefix Trie.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "trie.h"
#include "file_record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Helper to allocate an isolated FileRecord for testing */
static FileRecord *create_test_record(uint64_t id, const char *path, const char *name, uint64_t size) {
    FileRecord *rec = (FileRecord *)malloc(sizeof(FileRecord));
    assert(rec != NULL);
    memset(rec, 0, sizeof(FileRecord));

    rec->record_id = id;
    rec->filepath = strdup(path);
    rec->filename = strdup(name);
    rec->extension = strdup(".txt");
    rec->device_id = strdup("Device_Test");
    rec->file_size = size;
    rec->hash_status = HASH_STATUS_SUCCESS;
    return rec;
}

static void free_test_record(FileRecord *rec) {
    if (!rec) return;
    if (rec->filepath) free(rec->filepath);
    if (rec->filename) free(rec->filename);
    if (rec->extension) free(rec->extension);
    if (rec->device_id) free(rec->device_id);
    free(rec);
}

int main(void) {
    printf("====================================================\n");
    printf(" DUPLICATE-X Part 5B Comprehensive Verification Suite\n");
    printf(" Custom Prefix Tree (Trie) & File Path Indexing\n");
    printf(" Powered by Team DataVault (2026-2027)\n");
    printf("====================================================\n\n");

    /* ----------------------------------------------------
     * TEST 1: Initialize empty Trie
     * ---------------------------------------------------- */
    printf("[TEST 1/18] Initialize empty Trie... ");
    Trie *t1 = trie_init();
    assert(t1 != NULL);
    assert(t1->root != NULL);
    assert(t1->total_paths == 0);
    assert(t1->total_nodes == 1);
    trie_free(t1);
    printf("PASS (root allocated, total_paths = 0, total_nodes = 1)\n");

    /* ----------------------------------------------------
     * TEST 2: Insert one file path
     * ---------------------------------------------------- */
    printf("[TEST 2/18] Insert one file path... ");
    Trie *t2 = trie_init();
    FileRecord *r2 = create_test_record(1, "C:\\Data\\Notes.txt", "Notes.txt", 120);
    StatusCode sc = trie_insert(t2, r2->filepath, r2);
    assert(sc == STATUS_SUCCESS);
    assert(t2->total_paths == 1);
    assert(t2->total_nodes > 1);
    trie_free(t2);
    free_test_record(r2);
    printf("PASS (1 path inserted, node count dynamically expanded)\n");

    /* ----------------------------------------------------
     * TEST 3: Exact search for existing path
     * ---------------------------------------------------- */
    printf("[TEST 3/18] Exact search for existing path... ");
    Trie *t3 = trie_init();
    FileRecord *r3 = create_test_record(10, "C:\\Vault\\Report.pdf", "Report.pdf", 450);
    trie_insert(t3, r3->filepath, r3);

    const FileRecord *found3 = NULL;
    assert(trie_search_exact(t3, "C:\\Vault\\Report.pdf", &found3) == STATUS_SUCCESS);
    assert(found3 == r3);
    assert(found3->file_size == 450);
    /* Test separator normalization: '/' should match indexed '\\' */
    const FileRecord *found3_norm = NULL;
    assert(trie_search_exact(t3, "C:/Vault/Report.pdf", &found3_norm) == STATUS_SUCCESS);
    assert(found3_norm == r3);

    trie_free(t3);
    free_test_record(r3);
    printf("PASS (exact path found, slashes normalized)\n");

    /* ----------------------------------------------------
     * TEST 4: Exact search for non-existing path
     * ---------------------------------------------------- */
    printf("[TEST 4/18] Exact search for non-existing path... ");
    Trie *t4 = trie_init();
    FileRecord *r4 = create_test_record(20, "C:\\Vault\\Photo.png", "Photo.png", 800);
    trie_insert(t4, r4->filepath, r4);

    const FileRecord *found4 = NULL;
    assert(trie_search_exact(t4, "C:\\Vault\\Video.mp4", &found4) == STATUS_ERROR_PATH_NOT_FOUND);
    assert(found4 == NULL);
    assert(trie_contains(t4, "C:\\Vault\\Video.mp4") == false);

    trie_free(t4);
    free_test_record(r4);
    printf("PASS (correctly returned STATUS_ERROR_PATH_NOT_FOUND)\n");

    /* ----------------------------------------------------
     * TEST 5: Prefix search with one result
     * ---------------------------------------------------- */
    printf("[TEST 5/18] Prefix search with one result... ");
    Trie *t5 = trie_init();
    FileRecord *r5_a = create_test_record(1, "C:\\Projects\\App\\main.c", "main.c", 2048);
    FileRecord *r5_b = create_test_record(2, "C:\\Users\\Guest\\temp.log", "temp.log", 64);
    trie_insert(t5, r5_a->filepath, r5_a);
    trie_insert(t5, r5_b->filepath, r5_b);

    TrieSearchResults res5;
    assert(trie_search_prefix(t5, "C:\\Projects", &res5) == STATUS_SUCCESS);
    assert(res5.count == 1);
    assert(res5.records[0] == r5_a);
    trie_search_results_free(&res5);

    trie_free(t5);
    free_test_record(r5_a);
    free_test_record(r5_b);
    printf("PASS (single prefix match found)\n");

    /* ----------------------------------------------------
     * TEST 6: Prefix search with multiple results
     * ---------------------------------------------------- */
    printf("[TEST 6/18] Prefix search with multiple results... ");
    Trie *t6 = trie_init();
    FileRecord *r6_1 = create_test_record(1, "C:\\College\\Notes\\Maths.pdf", "Maths.pdf", 100);
    FileRecord *r6_2 = create_test_record(2, "C:\\College\\Notes\\Physics.pdf", "Physics.pdf", 200);
    FileRecord *r6_3 = create_test_record(3, "C:\\College\\Projects\\App.c", "App.c", 300);
    trie_insert(t6, r6_1->filepath, r6_1);
    trie_insert(t6, r6_2->filepath, r6_2);
    trie_insert(t6, r6_3->filepath, r6_3);

    TrieSearchResults res6;
    assert(trie_search_prefix(t6, "C:\\College\\Notes", &res6) == STATUS_SUCCESS);
    assert(res6.count == 2);
    /* Both Maths and Physics must be present */
    bool found_math = false, found_phys = false;
    for (size_t i = 0; i < res6.count; i++) {
        if (strcmp(res6.records[i]->filename, "Maths.pdf") == 0) found_math = true;
        if (strcmp(res6.records[i]->filename, "Physics.pdf") == 0) found_phys = true;
    }
    assert(found_math && found_phys);
    trie_search_results_free(&res6);

    trie_free(t6);
    free_test_record(r6_1);
    free_test_record(r6_2);
    free_test_record(r6_3);
    printf("PASS (both Notes files retrieved under C:\\College\\Notes)\n");

    /* ----------------------------------------------------
     * TEST 7: Prefix with no matches
     * ---------------------------------------------------- */
    printf("[TEST 7/18] Prefix with no matches... ");
    Trie *t7 = trie_init();
    FileRecord *r7 = create_test_record(1, "C:\\FolderA\\File1.txt", "File1.txt", 10);
    trie_insert(t7, r7->filepath, r7);

    TrieSearchResults res7;
    assert(trie_search_prefix(t7, "D:\\OtherDrive", &res7) == STATUS_SUCCESS);
    assert(res7.count == 0);
    assert(res7.records == NULL);
    trie_search_results_free(&res7);

    trie_free(t7);
    free_test_record(r7);
    printf("PASS (zero matches, returns empty results cleanly)\n");

    /* ----------------------------------------------------
     * TEST 8: Multiple paths sharing a common prefix
     * ---------------------------------------------------- */
    printf("[TEST 8/18] Multiple paths sharing long prefix... ");
    Trie *t8 = trie_init();
    FileRecord *r8_a = create_test_record(1, "C:\\Shared\\Common\\Subdir\\FileA.bin", "FileA.bin", 50);
    FileRecord *r8_b = create_test_record(2, "C:\\Shared\\Common\\Subdir\\FileB.bin", "FileB.bin", 60);
    FileRecord *r8_c = create_test_record(3, "C:\\Shared\\Common\\Subdir\\FileC.bin", "FileC.bin", 70);
    trie_insert(t8, r8_a->filepath, r8_a);
    trie_insert(t8, r8_b->filepath, r8_b);
    trie_insert(t8, r8_c->filepath, r8_c);

    TrieSearchResults res8;
    trie_search_prefix(t8, "C:\\Shared\\Common\\Subdir", &res8);
    assert(res8.count == 3);
    trie_search_results_free(&res8);

    trie_free(t8);
    free_test_record(r8_a);
    free_test_record(r8_b);
    free_test_record(r8_c);
    printf("PASS (all 3 shared-prefix siblings retrieved)\n");

    /* ----------------------------------------------------
     * TEST 9: Different directories with similar names
     * ---------------------------------------------------- */
    printf("[TEST 9/18] Directories with similar name prefixes... ");
    Trie *t9 = trie_init();
    FileRecord *r9_1 = create_test_record(1, "C:\\TestDir\\file.txt", "file.txt", 10);
    FileRecord *r9_2 = create_test_record(2, "C:\\TestDirExtended\\file.txt", "file.txt", 20);
    trie_insert(t9, r9_1->filepath, r9_1);
    trie_insert(t9, r9_2->filepath, r9_2);

    TrieSearchResults res9;
    /* Searching for TestDir\\ should not match TestDirExtended */
    trie_search_prefix(t9, "C:\\TestDir\\", &res9);
    assert(res9.count == 1);
    assert(res9.records[0] == r9_1);
    trie_search_results_free(&res9);

    trie_free(t9);
    free_test_record(r9_1);
    free_test_record(r9_2);
    printf("PASS (boundary slash isolates distinct subdirectories)\n");

    /* ----------------------------------------------------
     * TEST 10: Unicode filename support (UTF-8)
     * ---------------------------------------------------- */
    printf("[TEST 10/18] Unicode UTF-8 path support... ");
    Trie *t10 = trie_init();
    FileRecord *r10 = create_test_record(1, "C:\\Документы\\файл_тест.dat", "файл_тест.dat", 999);
    trie_insert(t10, r10->filepath, r10);

    const FileRecord *found10 = NULL;
    assert(trie_search_exact(t10, "C:\\Документы\\файл_тест.dat", &found10) == STATUS_SUCCESS);
    assert(found10 == r10);

    TrieSearchResults res10;
    trie_search_prefix(t10, "C:\\Документы", &res10);
    assert(res10.count == 1);
    trie_search_results_free(&res10);

    trie_free(t10);
    free_test_record(r10);
    printf("PASS (Cyrillic UTF-8 multi-byte characters indexed and queried)\n");

    /* ----------------------------------------------------
     * TEST 11: Filenames containing spaces
     * ---------------------------------------------------- */
    printf("[TEST 11/18] Paths and filenames with spaces... ");
    Trie *t11 = trie_init();
    FileRecord *r11 = create_test_record(1, "C:\\My Documents\\Project Proposal 2026.docx", "Project Proposal 2026.docx", 550);
    trie_insert(t11, r11->filepath, r11);

    const FileRecord *found11 = NULL;
    assert(trie_search_exact(t11, "C:\\My Documents\\Project Proposal 2026.docx", &found11) == STATUS_SUCCESS);
    assert(found11 == r11);

    trie_free(t11);
    free_test_record(r11);
    printf("PASS (spaces preserved and matched accurately)\n");

    /* ----------------------------------------------------
     * TEST 12: Long file paths
     * ---------------------------------------------------- */
    printf("[TEST 12/18] Deeply nested long file paths... ");
    Trie *t12 = trie_init();
    char long_path[600];
    strcpy(long_path, "C:\\Level0");
    for (int lvl = 1; lvl <= 15; lvl++) {
        strcat(long_path, "\\NestedFolderDirectory");
    }
    strcat(long_path, "\\target_file.dat");

    FileRecord *r12 = create_test_record(1, long_path, "target_file.dat", 4096);
    assert(trie_insert(t12, long_path, r12) == STATUS_SUCCESS);
    assert(trie_contains(t12, long_path) == true);

    trie_free(t12);
    free_test_record(r12);
    printf("PASS (nested path with depth 16 traversed safely)\n");

    /* ----------------------------------------------------
     * TEST 13: Duplicate path insertion handling
     * ---------------------------------------------------- */
    printf("[TEST 13/18] Duplicate path insertion idempotency... ");
    Trie *t13 = trie_init();
    FileRecord *r13 = create_test_record(1, "C:\\Data\\DuplicateEntry.txt", "DuplicateEntry.txt", 100);
    assert(trie_insert(t13, r13->filepath, r13) == STATUS_SUCCESS);
    size_t paths_before = t13->total_paths;
    assert(trie_insert(t13, r13->filepath, r13) == STATUS_SUCCESS);
    assert(t13->total_paths == paths_before); /* Total paths must not duplicate */

    trie_free(t13);
    free_test_record(r13);
    printf("PASS (duplicate insert handled idempotently)\n");

    /* ----------------------------------------------------
     * TEST 14: Empty prefix behavior
     * ---------------------------------------------------- */
    printf("[TEST 14/18] Empty prefix query (returns all indexed paths)... ");
    Trie *t14 = trie_init();
    FileRecord *r14_1 = create_test_record(1, "C:\\A.txt", "A.txt", 1);
    FileRecord *r14_2 = create_test_record(2, "C:\\B.txt", "B.txt", 2);
    FileRecord *r14_3 = create_test_record(3, "C:\\C.txt", "C.txt", 3);
    trie_insert(t14, r14_1->filepath, r14_1);
    trie_insert(t14, r14_2->filepath, r14_2);
    trie_insert(t14, r14_3->filepath, r14_3);

    TrieSearchResults res14;
    assert(trie_search_prefix(t14, "", &res14) == STATUS_SUCCESS);
    assert(res14.count == 3);
    trie_search_results_free(&res14);

    trie_free(t14);
    free_test_record(r14_1);
    free_test_record(r14_2);
    free_test_record(r14_3);
    printf("PASS (empty prefix correctly returns all 3 indexed records)\n");

    /* ----------------------------------------------------
     * TEST 15: Invalid NULL input handling
     * ---------------------------------------------------- */
    printf("[TEST 15/18] Invalid NULL argument guards... ");
    assert(trie_insert(NULL, NULL, NULL) == STATUS_ERROR_INVALID_ARGUMENT);
    assert(trie_search_exact(NULL, NULL, NULL) == STATUS_ERROR_INVALID_ARGUMENT);
    assert(trie_contains(NULL, NULL) == false);
    assert(trie_search_prefix(NULL, NULL, NULL) == STATUS_ERROR_INVALID_ARGUMENT);
    assert(trie_remove(NULL, NULL) == STATUS_ERROR_INVALID_ARGUMENT);
    trie_free(NULL);
    trie_search_results_free(NULL);
    printf("PASS (all functions safely guarded against NULL)\n");

    /* ----------------------------------------------------
     * TEST 16: Trie removal of an existing path
     * ---------------------------------------------------- */
    printf("[TEST 16/18] Trie removal of existing path... ");
    Trie *t16 = trie_init();
    FileRecord *r16_a = create_test_record(1, "C:\\Dir\\Keep.txt", "Keep.txt", 10);
    FileRecord *r16_b = create_test_record(2, "C:\\Dir\\RemoveMe.txt", "RemoveMe.txt", 20);
    trie_insert(t16, r16_a->filepath, r16_a);
    trie_insert(t16, r16_b->filepath, r16_b);
    assert(t16->total_paths == 2);

    assert(trie_remove(t16, "C:\\Dir\\RemoveMe.txt") == STATUS_SUCCESS);
    assert(t16->total_paths == 1);
    assert(trie_contains(t16, "C:\\Dir\\RemoveMe.txt") == false);
    assert(trie_contains(t16, "C:\\Dir\\Keep.txt") == true);

    trie_free(t16);
    free_test_record(r16_a);
    free_test_record(r16_b);
    printf("PASS (removed path pruned, sibling preserved)\n");

    /* ----------------------------------------------------
     * TEST 17: Removal of a non-existing path
     * ---------------------------------------------------- */
    printf("[TEST 17/18] Removal of non-existing path... ");
    Trie *t17 = trie_init();
    FileRecord *r17 = create_test_record(1, "C:\\Exist.txt", "Exist.txt", 100);
    trie_insert(t17, r17->filepath, r17);

    assert(trie_remove(t17, "C:\\NoSuchPath.txt") == STATUS_ERROR_PATH_NOT_FOUND);
    assert(t17->total_paths == 1);

    trie_free(t17);
    free_test_record(r17);
    printf("PASS (STATUS_ERROR_PATH_NOT_FOUND returned without mutations)\n");

    /* ----------------------------------------------------
     * TEST 18: Memory cleanup without affecting FileInventory ownership
     * ---------------------------------------------------- */
    printf("[TEST 18/18] Trie memory cleanup without affecting records... ");
    Trie *t18 = trie_init();
    FileRecord *r18 = create_test_record(99, "C:\\Safe\\File.txt", "File.txt", 1234);
    trie_insert(t18, r18->filepath, r18);

    /* Free the entire Trie */
    trie_free(t18);

    /* The original FileRecord and its dynamically allocated strings must still be 100% valid */
    assert(r18->record_id == 99);
    assert(strcmp(r18->filepath, "C:\\Safe\\File.txt") == 0);
    assert(strcmp(r18->filename, "File.txt") == 0);
    assert(r18->file_size == 1234);

    free_test_record(r18);
    printf("PASS (FileRecord memory ownership completely unmolested)\n");

    printf("\n====================================================\n");
    printf(" ALL 18 TRIE VERIFICATION TESTS PASSED WITH 100%% SUCCESS!\n");
    printf("====================================================\n");

    return EXIT_SUCCESS;
}

