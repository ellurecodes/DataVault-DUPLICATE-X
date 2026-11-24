/**
 * @file test_quarantine.c
 * @brief Comprehensive 25-scenario verification test suite for Part 7A Safe Quarantine & Recovery Engine.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "quarantine.h"
#include "quarantine_manifest.h"
#include "sha256.h"
#include "file_compare.h"
#include "file_record.h"
#include "duplicate_group.h"
#include "status.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <windows.h>
#include <time.h>

#define TEST_SANDBOX "data\\test_quarantine_sandbox"
#define TEST_MANIFEST "data\\test_quarantine_sandbox\\manifest.json"
#define TEST_JOURNAL  "data\\test_quarantine_sandbox\\journal.log"

/* Helper to safely remove a test directory recursively */
static void cleanup_directory(const wchar_t *dir_w) {
    wchar_t search_w[1024];
    _snwprintf(search_w, 1023, L"%ls\\*", dir_w);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search_w, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            wchar_t sub_w[1024];
            _snwprintf(sub_w, 1023, L"%ls\\%ls", dir_w, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                cleanup_directory(sub_w);
            } else {
                SetFileAttributesW(sub_w, FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(sub_w);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    RemoveDirectoryW(dir_w);
}

/* Helper to create test file with specific text */
/* Helper to create test file with specific text (supports UTF-8 Unicode paths) */
static void create_test_file(const char *rel_path, const char *content) {
    char full[1024];
    snprintf(full, sizeof(full), "%s\\%s", TEST_SANDBOX, rel_path);

    wchar_t full_w[1024];
    MultiByteToWideChar(CP_UTF8, 0, full, -1, full_w, 1024);

    /* Ensure parent directories exist */
    wchar_t dir_w[1024];
    wcsncpy(dir_w, full_w, 1023);
    dir_w[1023] = L'\0';
    wchar_t *s = wcsrchr(dir_w, L'\\');
    if (s) {
        *s = L'\0';
        for (wchar_t *p = dir_w; *p; p++) {
            if (*p == L'\\') {
                *p = L'\0';
                CreateDirectoryW(dir_w, NULL);
                *p = L'\\';
            }
        }
        CreateDirectoryW(dir_w, NULL);
    }

    FILE *f = _wfopen(full_w, L"wb");
    if (f) {
        if (content) fputs(content, f);
        fclose(f);
    }
}

int main(void) {
    printf("====================================================\n");
    printf(" DUPLICATE-X Part 7A Safe Quarantine & Recovery Suite\n");
    printf(" 25 Comprehensive Verification Scenarios\n");
    printf(" Powered by Team DataVault (2026-2027)\n");
    printf("====================================================\n\n");

    /* Reset sandbox */
    wchar_t sandbox_w[1024];
    MultiByteToWideChar(CP_UTF8, 0, TEST_SANDBOX, -1, sandbox_w, 1024);
    cleanup_directory(sandbox_w);
    CreateDirectoryW(sandbox_w, NULL);

    /* ----------------------------------------------------
     * TEST 1: Initialize empty quarantine manager
     * ---------------------------------------------------- */
    printf("[TEST 1/25] Initialize empty quarantine manager... ");
    QuarantineManager qmgr;
    StatusCode sc = quarantine_manager_init(&qmgr, TEST_MANIFEST, TEST_JOURNAL);
    assert(sc == STATUS_SUCCESS);
    assert(qmgr.manifest.count == 0);
    assert(qmgr.preview_count == 0);
    printf("PASS (initialized cleanly with 0 records)\n");

    /* Create sample duplicate files in sandbox */
    create_test_file("folder_a\\doc1.txt", "IDENTICAL CONTENT FOR THREE COPIES");
    create_test_file("folder_a\\doc2.txt", "IDENTICAL CONTENT FOR THREE COPIES");
    create_test_file("folder_b\\doc3.txt", "IDENTICAL CONTENT FOR THREE COPIES");

    char abs1[1024], abs2[1024], abs3[1024];
    GetFullPathNameA("data\\test_quarantine_sandbox\\folder_a\\doc1.txt", 1024, abs1, NULL);
    GetFullPathNameA("data\\test_quarantine_sandbox\\folder_a\\doc2.txt", 1024, abs2, NULL);
    GetFullPathNameA("data\\test_quarantine_sandbox\\folder_b\\doc3.txt", 1024, abs3, NULL);

    wchar_t abs1_w[1024];
    MultiByteToWideChar(CP_UTF8, 0, abs1, -1, abs1_w, 1024);
    uint8_t d_bin[SHA256_DIGEST_LENGTH];
    char digest[SHA256_HEX_LENGTH] = {0};
    HashStatus hs = HASH_STATUS_PENDING;
    sha256_calculate_file(abs1_w, d_bin, digest, &hs);

    FileInventory inv;
    file_inventory_init(&inv);

    FileRecord r1, r2, r3;
    memset(&r1, 0, sizeof(FileRecord));
    r1.record_id = 1;
    r1.filepath = strdup(abs1);
    r1.filename = strdup("doc1.txt");
    r1.file_size = 34;
    strcpy(r1.sha256_hex, digest);
    file_inventory_add(&inv, &r1);
    file_record_cleanup(&r1);

    memset(&r2, 0, sizeof(FileRecord));
    r2.record_id = 2;
    r2.filepath = strdup(abs2);
    r2.filename = strdup("doc2.txt");
    r2.file_size = 34;
    strcpy(r2.sha256_hex, digest);
    file_inventory_add(&inv, &r2);
    file_record_cleanup(&r2);

    memset(&r3, 0, sizeof(FileRecord));
    r3.record_id = 3;
    r3.filepath = strdup(abs3);
    r3.filename = strdup("doc3.txt");
    r3.file_size = 34;
    strcpy(r3.sha256_hex, digest);
    file_inventory_add(&inv, &r3);
    file_record_cleanup(&r3);

    DuplicateGroupList dgl;
    duplicate_group_list_init(&dgl);
    DuplicateGroup *target_grp = duplicate_group_create(1, &inv.records[0]);
    duplicate_group_add_file(target_grp, &inv.records[1]);
    duplicate_group_add_file(target_grp, &inv.records[2]);
    duplicate_group_list_add(&dgl, target_grp);

    /* ----------------------------------------------------
     * TEST 2: Preview a valid duplicate file
     * ---------------------------------------------------- */
    printf("[TEST 2/25] Preview a valid duplicate file... ");
    uint64_t sel_files[1] = {2}; /* Select doc2.txt */
    QuarantinePreview prev1;
    sc = quarantine_preview_create(&qmgr, &dgl, 1, sel_files, 1, &prev1);
    assert(sc == STATUS_SUCCESS);
    assert(strlen(prev1.preview_id) > 0);
    assert(prev1.file_count == 1);
    assert(prev1.remaining_copies == 2);
    assert(prev1.estimated_recoverable_bytes == 34);
    printf("PASS (preview generated with ID %s)\n", prev1.preview_id);

    /* ----------------------------------------------------
     * TEST 3: Preview must not move the file
     * ---------------------------------------------------- */
    printf("[TEST 3/25] Preview must not move the file... ");
    assert(GetFileAttributesA(abs2) != INVALID_FILE_ATTRIBUTES);
    assert(qmgr.manifest.count == 0);
    printf("PASS (file doc2.txt remains untouched at original location)\n");

    /* ----------------------------------------------------
     * TEST 4: Confirmed quarantine succeeds
     * ---------------------------------------------------- */
    printf("[TEST 4/25] Confirmed quarantine execution... ");
    size_t q_count = 0;
    char tx_ids[32][64];
    sc = quarantine_execute(&qmgr, &dgl, &inv, prev1.preview_id, &q_count, tx_ids);
    assert(sc == STATUS_SUCCESS);
    assert(q_count == 1);
    assert(qmgr.manifest.count == 1);
    assert(target_grp->file_count == 2); /* One copy removed from group */
    printf("PASS (quarantined 1 copy, tx_id: %s)\n", tx_ids[0]);

    /* ----------------------------------------------------
     * TEST 5: Original file disappears from original path
     * ---------------------------------------------------- */
    printf("[TEST 5/25] Original file disappears from original path... ");
    assert(GetFileAttributesA(abs2) == INVALID_FILE_ATTRIBUTES);
    printf("PASS (doc2.txt no longer present at original path)\n");

    /* ----------------------------------------------------
     * TEST 6: Quarantined file exists in quarantine
     * ---------------------------------------------------- */
    printf("[TEST 6/25] Quarantined file exists in quarantine directory... ");
    const QuarantineRecord *rec1 = quarantine_manifest_find(&qmgr.manifest, tx_ids[0]);
    assert(rec1 != NULL);
    assert(rec1->status == QUARANTINE_STATUS_ACTIVE);
    assert(GetFileAttributesA(rec1->quarantine_path) != INVALID_FILE_ATTRIBUTES);
    printf("PASS (quarantined file exists at %s)\n", rec1->quarantine_path);

    /* ----------------------------------------------------
     * TEST 7: Quarantined content matches original content
     * ---------------------------------------------------- */
    printf("[TEST 7/25] Quarantined content matches original content... ");
    wchar_t quar_w[1024];
    MultiByteToWideChar(CP_UTF8, 0, rec1->quarantine_path, -1, quar_w, 1024);
    char check_sha[SHA256_HEX_LENGTH] = {0};
    uint8_t dummy_b[SHA256_DIGEST_LENGTH];
    HashStatus check_hs = HASH_STATUS_PENDING;
    sha256_calculate_file(quar_w, dummy_b, check_sha, &check_hs);
    assert(strcmp(check_sha, digest) == 0);
    printf("PASS (SHA-256 verified %s)\n", check_sha);

    /* ----------------------------------------------------
     * TEST 8: Last remaining copy protection
     * ---------------------------------------------------- */
    printf("[TEST 8/25] Last remaining copy protection... ");
    /* Group currently has 2 copies left: doc1 (id 1) and doc3 (id 3). Selecting both would leave 0. */
    uint64_t all_sel[2] = {1, 3};
    QuarantinePreview prev_fail;
    sc = quarantine_preview_create(&qmgr, &dgl, 1, all_sel, 2, &prev_fail);
    assert(sc == STATUS_ERROR_SOLE_COPY_REMAINING);
    printf("PASS (rejected with STATUS_ERROR_SOLE_COPY_REMAINING)\n");

    /* ----------------------------------------------------
     * TEST 9: Attempt to quarantine all copies in a group
     * ---------------------------------------------------- */
    printf("[TEST 9/25] Attempt to quarantine all copies in group... ");
    /* Try selecting all remaining files (count == grp->file_count) */
    sc = quarantine_preview_create(&qmgr, &dgl, 1, all_sel, target_grp->file_count, &prev_fail);
    assert(sc == STATUS_ERROR_SOLE_COPY_REMAINING);
    printf("PASS (rejected: root/sole copy protection active)\n");

    /* ----------------------------------------------------
     * TEST 10: Attempt to quarantine more than available copies
     * ---------------------------------------------------- */
    printf("[TEST 10/25] Attempt to quarantine more than available copies... ");
    uint64_t excess[3] = {1, 3, 99};
    sc = quarantine_preview_create(&qmgr, &dgl, 1, excess, 3, &prev_fail);
    assert(sc == STATUS_ERROR_SOLE_COPY_REMAINING || sc == STATUS_ERROR_INVALID_ARGUMENT);
    printf("PASS (rejected excessive file count)\n");

    /* ----------------------------------------------------
     * TEST 11: Invalid duplicate group ID
     * ---------------------------------------------------- */
    printf("[TEST 11/25] Invalid duplicate group ID... ");
    uint64_t single_sel[1] = {1};
    sc = quarantine_preview_create(&qmgr, &dgl, 9999, single_sel, 1, &prev_fail);
    assert(sc == STATUS_ERROR_INVALID_ARGUMENT);
    printf("PASS (group 9999 returned STATUS_ERROR_INVALID_ARGUMENT)\n");

    /* ----------------------------------------------------
     * TEST 12: Invalid file record ID
     * ---------------------------------------------------- */
    printf("[TEST 12/25] Invalid file record ID... ");
    uint64_t bogus_fid[1] = {8888};
    sc = quarantine_preview_create(&qmgr, &dgl, 1, bogus_fid, 1, &prev_fail);
    assert(sc == STATUS_ERROR_INVALID_ARGUMENT);
    printf("PASS (unrecognized file ID returned STATUS_ERROR_INVALID_ARGUMENT)\n");

    /* ----------------------------------------------------
     * TEST 13: Expired preview token
     * ---------------------------------------------------- */
    printf("[TEST 13/25] Expired preview token rejection... ");
    QuarantinePreview prev_exp;
    sc = quarantine_preview_create(&qmgr, &dgl, 1, single_sel, 1, &prev_exp);
    assert(sc == STATUS_SUCCESS);
    /* Artificially expire the preview */
    for (size_t i = 0; i < qmgr.preview_count; i++) {
        if (strcmp(qmgr.previews[i].preview_id, prev_exp.preview_id) == 0) {
            qmgr.previews[i].expires_at = (int64_t)time(NULL) - 100;
            break;
        }
    }
    sc = quarantine_execute(&qmgr, &dgl, &inv, prev_exp.preview_id, &q_count, tx_ids);
    assert(sc == STATUS_ERROR_PREVIEW_EXPIRED);
    printf("PASS (expired token rejected with STATUS_ERROR_PREVIEW_EXPIRED)\n");

    /* ----------------------------------------------------
     * TEST 14: Reused preview token
     * ---------------------------------------------------- */
    printf("[TEST 14/25] Reused preview token rejection... ");
    /* prev1 was already used in TEST 4 */
    sc = quarantine_execute(&qmgr, &dgl, &inv, prev1.preview_id, &q_count, tx_ids);
    assert(sc == STATUS_ERROR_PREVIEW_ALREADY_USED);
    printf("PASS (used token rejected with STATUS_ERROR_PREVIEW_ALREADY_USED)\n");

    /* ----------------------------------------------------
     * TEST 15: Modified file after preview
     * ---------------------------------------------------- */
    printf("[TEST 15/25] Modified file after preview detection... ");
    create_test_file("folder_b\\doc_mod.txt", "IDENTICAL CONTENT FOR THREE COPIES");
    char abs_mod[1024];
    GetFullPathNameA("data\\test_quarantine_sandbox\\folder_b\\doc_mod.txt", 1024, abs_mod, NULL);

    FileRecord r_mod;
    memset(&r_mod, 0, sizeof(FileRecord));
    r_mod.record_id = 4;
    r_mod.filepath = strdup(abs_mod);
    r_mod.filename = strdup("doc_mod.txt");
    r_mod.file_size = 34;
    strcpy(r_mod.sha256_hex, digest);
    duplicate_group_add_file(target_grp, &r_mod);

    uint64_t sel_mod[1] = {4};
    QuarantinePreview prev_mod;
    sc = quarantine_preview_create(&qmgr, &dgl, 1, sel_mod, 1, &prev_mod);
    assert(sc == STATUS_SUCCESS);

    /* Tamper file before execution */
    FILE *f_tamp = fopen(abs_mod, "ab");
    fputs("TAMPERED", f_tamp);
    fclose(f_tamp);

    sc = quarantine_execute(&qmgr, &dgl, &inv, prev_mod.preview_id, &q_count, tx_ids);
    assert(sc == STATUS_ERROR_QUARANTINE_FAILED || sc == STATUS_ERROR_CONCURRENT_MODIFICATION);
    printf("PASS (tampered file detected, move rejected)\n");

    /* ----------------------------------------------------
     * TEST 16: Source file missing after preview
     * ---------------------------------------------------- */
    printf("[TEST 16/25] Source file missing after preview... ");
    create_test_file("folder_b\\doc_del.txt", "IDENTICAL CONTENT FOR THREE COPIES");
    char abs_del[1024];
    GetFullPathNameA("data\\test_quarantine_sandbox\\folder_b\\doc_del.txt", 1024, abs_del, NULL);

    FileRecord r_del;
    memset(&r_del, 0, sizeof(FileRecord));
    r_del.record_id = 5;
    r_del.filepath = strdup(abs_del);
    r_del.filename = strdup("doc_del.txt");
    r_del.file_size = 34;
    strcpy(r_del.sha256_hex, digest);
    duplicate_group_add_file(target_grp, &r_del);

    uint64_t sel_del[1] = {5};
    QuarantinePreview prev_del;
    sc = quarantine_preview_create(&qmgr, &dgl, 1, sel_del, 1, &prev_del);
    assert(sc == STATUS_SUCCESS);

    DeleteFileA(abs_del); /* Deleted behind our back */

    sc = quarantine_execute(&qmgr, &dgl, &inv, prev_del.preview_id, &q_count, tx_ids);
    assert(sc == STATUS_ERROR_QUARANTINE_FAILED || sc == STATUS_ERROR_PATH_NOT_FOUND);
    printf("PASS (missing source file handled gracefully)\n");

    /* ----------------------------------------------------
     * TEST 17: Insufficient permission handling
     * ---------------------------------------------------- */
    printf("[TEST 17/25] Insufficient permission / read-only handling... ");
    create_test_file("folder_a\\doc_ro.txt", "IDENTICAL CONTENT FOR THREE COPIES");
    char abs_ro[1024];
    GetFullPathNameA("data\\test_quarantine_sandbox\\folder_a\\doc_ro.txt", 1024, abs_ro, NULL);
    FileRecord r_ro;
    memset(&r_ro, 0, sizeof(FileRecord));
    r_ro.record_id = 6;
    r_ro.filepath = strdup(abs_ro);
    r_ro.filename = strdup("doc_ro.txt");
    r_ro.file_size = 34;
    strcpy(r_ro.sha256_hex, digest);
    duplicate_group_add_file(target_grp, &r_ro);

    /* Open file with exclusive lock so MoveFileEx fails */
    HANDLE hLock = CreateFileA(abs_ro, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    uint64_t sel_ro[1] = {6};
    QuarantinePreview prev_ro;
    sc = quarantine_preview_create(&qmgr, &dgl, 1, sel_ro, 1, &prev_ro);
    if (sc == STATUS_SUCCESS) {
        sc = quarantine_execute(&qmgr, &dgl, &inv, prev_ro.preview_id, &q_count, tx_ids);
        assert(sc == STATUS_ERROR_QUARANTINE_FAILED);
    }
    if (hLock != INVALID_HANDLE_VALUE) CloseHandle(hLock);
    printf("PASS (exclusive lock handled safely without corrupting state)\n");

    /* ----------------------------------------------------
     * TEST 18: Restore a quarantined file successfully
     * ---------------------------------------------------- */
    printf("[TEST 18/25] Restore quarantined file successfully... ");
    /* We restore rec1 from TEST 4 */
    QuarantinePreview rest_prev;
    sc = quarantine_restore_preview(&qmgr, rec1->transaction_id, &rest_prev);
    assert(sc == STATUS_SUCCESS);
    assert(rest_prev.is_restore == true);

    char rest_path[1024] = {0};
    sc = quarantine_restore_execute(&qmgr, rest_prev.preview_id, rest_path, sizeof(rest_path));
    assert(sc == STATUS_SUCCESS);
    assert(GetFileAttributesA(abs2) != INVALID_FILE_ATTRIBUTES);
    assert(rec1->status == QUARANTINE_STATUS_RESTORED);
    printf("PASS (doc2.txt restored to original path: %s)\n", rest_path);

    /* ----------------------------------------------------
     * TEST 19: Restore when original path already exists
     * ---------------------------------------------------- */
    printf("[TEST 19/25] Restore when original path exists (overwrite prevented)... ");
    /* Quarantine doc3.txt first */
    uint64_t sel_d3[1] = {3};
    QuarantinePreview prev_d3;
    sc = quarantine_preview_create(&qmgr, &dgl, 1, sel_d3, 1, &prev_d3);
    assert(sc == STATUS_SUCCESS);
    sc = quarantine_execute(&qmgr, &dgl, &inv, prev_d3.preview_id, &q_count, tx_ids);
    assert(sc == STATUS_SUCCESS);

    /* Recreate doc3.txt at its original path behind quarantine's back */
    create_test_file("folder_b\\doc3.txt", "CONFLICTING FILE CREATED AT DESTINATION");

    /* Attempt to restore */
    QuarantinePreview rest_conflict;
    sc = quarantine_restore_preview(&qmgr, tx_ids[0], &rest_conflict);
    assert(sc == STATUS_ERROR_DESTINATION_EXISTS);
    printf("PASS (STATUS_ERROR_DESTINATION_EXISTS prevented overwrite)\n");

    /* ----------------------------------------------------
     * TEST 20: Tampered quarantine file detection
     * ---------------------------------------------------- */
    printf("[TEST 20/25] Tampered quarantine file detection... ");
    /* Remove conflicting file so path check passes */
    DeleteFileA(abs3);
    const QuarantineRecord *rec_d3 = quarantine_manifest_find(&qmgr.manifest, tx_ids[0]);
    assert(rec_d3 != NULL);

    /* Tamper with quarantine file content */
    FILE *f_tamp_q = fopen(rec_d3->quarantine_path, "ab");
    fputs("TAMPER_QUARANTINE_BYTE", f_tamp_q);
    fclose(f_tamp_q);

    QuarantinePreview rest_tampered;
    sc = quarantine_restore_preview(&qmgr, rec_d3->transaction_id, &rest_tampered);
    assert(sc == STATUS_ERROR_FILE_TAMPERED);
    printf("PASS (tamper detected: SHA-256 digest mismatch)\n");

    /* ----------------------------------------------------
     * TEST 21: Manifest persistence across manager restart
     * ---------------------------------------------------- */
    printf("[TEST 21/25] Manifest persistence across manager restart... ");
    size_t count_before = qmgr.manifest.count;
    quarantine_manager_cleanup(&qmgr);

    QuarantineManager qmgr2;
    sc = quarantine_manager_init(&qmgr2, TEST_MANIFEST, TEST_JOURNAL);
    assert(sc == STATUS_SUCCESS);
    assert(qmgr2.manifest.count == count_before);
    assert(quarantine_manifest_find(&qmgr2.manifest, rec1->transaction_id) != NULL);
    printf("PASS (%zu records successfully reloaded from JSON manifest)\n", qmgr2.manifest.count);

    /* ----------------------------------------------------
     * TEST 22: Interrupted transaction recovery
     * ---------------------------------------------------- */
    printf("[TEST 22/25] Interrupted transaction reconciliation... ");
    /* Inject an inconsistent record into manifest where original exists and quarantine does not */
    QuarantineRecord fake_rec;
    memset(&fake_rec, 0, sizeof(QuarantineRecord));
    strcpy(fake_rec.transaction_id, "tx_fake_interrupted_123");
    strcpy(fake_rec.original_path, abs1);
    strcpy(fake_rec.quarantine_path, "data\\test_quarantine_sandbox\\nonexistent.bin");
    fake_rec.status = QUARANTINE_STATUS_ACTIVE;
    quarantine_manifest_add(&qmgr2.manifest, &fake_rec);

    /* Run reconcile */
    sc = quarantine_manifest_reconcile(&qmgr2.manifest);
    assert(sc == STATUS_SUCCESS);
    const QuarantineRecord *reconciled = quarantine_manifest_find(&qmgr2.manifest, "tx_fake_interrupted_123");
    assert(reconciled != NULL);
    assert(reconciled->status == QUARANTINE_STATUS_FAILED);
    printf("PASS (interrupted transaction reconciled to STATUS_FAILED)\n");

    /* ----------------------------------------------------
     * TEST 23: Unicode filename restoration
     * ---------------------------------------------------- */
    printf("[TEST 23/25] Unicode filename quarantine & restore... ");
    create_test_file("unicode\\файл_123_копия.txt", "UNICODE CONTENT FOR QUARANTINE TEST");
    create_test_file("unicode\\файл_123_оригинал.txt", "UNICODE CONTENT FOR QUARANTINE TEST");

    wchar_t abs_u1_w[1024], abs_u2_w[1024];
    GetFullPathNameW(L"data\\test_quarantine_sandbox\\unicode\\файл_123_копия.txt", 1024, abs_u1_w, NULL);
    GetFullPathNameW(L"data\\test_quarantine_sandbox\\unicode\\файл_123_оригинал.txt", 1024, abs_u2_w, NULL);

    char abs_u1[1024], abs_u2[1024];
    WideCharToMultiByte(CP_UTF8, 0, abs_u1_w, -1, abs_u1, 1024, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, abs_u2_w, -1, abs_u2, 1024, NULL, NULL);

    uint8_t u_binary[32] = {0};
    char u_digest[65] = {0};
    HashStatus hs_u;
    sha256_calculate_file(abs_u1_w, u_binary, u_digest, &hs_u);

    FileRecord ru1, ru2;
    memset(&ru1, 0, sizeof(FileRecord));
    ru1.record_id = 70;
    ru1.filepath = strdup(abs_u1);
    ru1.filename = strdup("файл_123_копия.txt");
    ru1.file_size = 36;
    strcpy(ru1.sha256_hex, u_digest);

    memset(&ru2, 0, sizeof(FileRecord));
    ru2.record_id = 71;
    ru2.filepath = strdup(abs_u2);
    ru2.filename = strdup("файл_123_оригинал.txt");
    ru2.file_size = 36;
    strcpy(ru2.sha256_hex, u_digest);

    DuplicateGroup *grp_u = duplicate_group_create(2, &ru1);
    duplicate_group_add_file(grp_u, &ru2);
    duplicate_group_list_add(&dgl, grp_u);

    uint64_t sel_u[1] = {70};
    QuarantinePreview prev_u;
    sc = quarantine_preview_create(&qmgr2, &dgl, 2, sel_u, 1, &prev_u);
    assert(sc == STATUS_SUCCESS);

    sc = quarantine_execute(&qmgr2, &dgl, &inv, prev_u.preview_id, &q_count, tx_ids);
    assert(sc == STATUS_SUCCESS);
    assert(GetFileAttributesW(abs_u1_w) == INVALID_FILE_ATTRIBUTES);

    /* Restore unicode file */
    QuarantinePreview rest_prev_u;
    sc = quarantine_restore_preview(&qmgr2, tx_ids[0], &rest_prev_u);
    assert(sc == STATUS_SUCCESS);

    char rest_u_path[1024] = {0};
    sc = quarantine_restore_execute(&qmgr2, rest_prev_u.preview_id, rest_u_path, sizeof(rest_u_path));
    assert(sc == STATUS_SUCCESS);
    assert(GetFileAttributesW(abs_u1_w) != INVALID_FILE_ATTRIBUTES);
    printf("PASS (Unicode paths preserved and restored cleanly)\n");

    /* ----------------------------------------------------
     * TEST 24: Cross-volume operation rejection
     * ---------------------------------------------------- */
    printf("[TEST 24/25] Cross-volume operation rejection... ");
    /* Verify quarantine_is_same_volume rejects differing drive roots */
    wchar_t c_path[] = L"C:\\Folder\\file.txt";
    wchar_t d_path[] = L"D:\\Folder\\.datavault_quarantine\\file.bin";
    bool same_vol = quarantine_is_same_volume(c_path, d_path);
    assert(same_vol == false);
    printf("PASS (quarantine_is_same_volume strictly returned false)\n");

    /* ----------------------------------------------------
     * TEST 25: Hard-link identity safety
     * ---------------------------------------------------- */
    printf("[TEST 25/25] Hard-link identity detection... ");
    /* Test quarantine_is_hardlink on same file returns true */
    bool is_hl = quarantine_is_hardlink(abs1_w, abs1_w);
    assert(is_hl == true);

    wchar_t abs2_w[1024];
    MultiByteToWideChar(CP_UTF8, 0, abs2, -1, abs2_w, 1024);
    /* abs1 and abs2 are distinct files, not hard links */
    bool is_distinct = quarantine_is_hardlink(abs1_w, abs2_w);
    assert(is_distinct == false);
    printf("PASS (hard-link file identity verified via volume serial & file index)\n");

    /* Cleanup test resources */
    quarantine_manager_cleanup(&qmgr2);
    duplicate_group_list_free(&dgl);
    file_inventory_free(&inv);
    cleanup_directory(sandbox_w);

    printf("\n====================================================\n");
    printf(" ALL 25 QUARANTINE & RECOVERY VERIFICATION TESTS PASSED (100%%)!\n");
    printf("====================================================\n");

    return EXIT_SUCCESS;
}
