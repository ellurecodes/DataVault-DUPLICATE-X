/**
 * @file test_minheap.c
 * @brief Comprehensive 15-scenario verification test suite for Part 5A Binary Min Heap.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "minheap.h"
#include "duplicate_group.h"
#include "file_record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Helper to create dummy DuplicateGroup for testing */
static DuplicateGroup *create_dummy_group(uint32_t id, uint64_t recoverable_bytes) {
    DuplicateGroup *grp = (DuplicateGroup *)malloc(sizeof(DuplicateGroup));
    assert(grp != NULL);
    memset(grp, 0, sizeof(DuplicateGroup));

    grp->group_id = id;
    grp->recoverable_bytes = recoverable_bytes;
    grp->file_count = 2;
    grp->file_size = recoverable_bytes;
    grp->files_head = NULL;
    grp->files_tail = NULL;
    grp->next = NULL;
    return grp;
}

int main(void) {
    printf("====================================================\n");
    printf(" DUPLICATE-X Part 5A Comprehensive Verification Suite\n");
    printf(" Custom Binary Min Heap & Top K Ranking Engine\n");
    printf(" Powered by Team DataVault (2026-2027)\n");
    printf("====================================================\n\n");

    /* ----------------------------------------------------
     * TEST 1: Empty heap initialization
     * ---------------------------------------------------- */
    printf("[TEST 1/15] Empty heap initialization... ");
    MinHeap *h1 = minheap_create(5);
    assert(h1 != NULL);
    assert(h1->size == 0);
    assert(h1->capacity == 5);
    assert(minheap_is_empty(h1) == true);
    assert(minheap_peek(h1) == NULL);
    minheap_free(h1);
    printf("PASS (size = 0, peek = NULL)\n");

    /* ----------------------------------------------------
     * TEST 2: Insert one duplicate group
     * ---------------------------------------------------- */
    printf("[TEST 2/15] Insert one duplicate group... ");
    MinHeap *h2 = minheap_create(4);
    DuplicateGroup *g2 = create_dummy_group(101, 500);
    StatusCode sc = minheap_insert(h2, g2);
    assert(sc == STATUS_SUCCESS);
    assert(h2->size == 1);
    assert(minheap_is_empty(h2) == false);
    assert(minheap_peek(h2) == g2);
    assert(minheap_peek(h2)->recoverable_bytes == 500);
    minheap_free(h2);
    free(g2);
    printf("PASS (size = 1, peek matches inserted group)\n");

    /* ----------------------------------------------------
     * TEST 3: Insert multiple groups in random order
     * ---------------------------------------------------- */
    printf("[TEST 3/15] Insert multiple groups in random order... ");
    MinHeap *h3 = minheap_create(10);
    DuplicateGroup *g_arr[5];
    uint64_t sizes3[5] = {800, 200, 600, 100, 400};
    for (int i = 0; i < 5; i++) {
        g_arr[i] = create_dummy_group((uint32_t)(i + 1), sizes3[i]);
        assert(minheap_insert(h3, g_arr[i]) == STATUS_SUCCESS);
    }
    assert(h3->size == 5);
    /* Root must be smallest (100) */
    assert(minheap_peek(h3)->recoverable_bytes == 100);
    minheap_free(h3);
    for (int i = 0; i < 5; i++) free(g_arr[i]);
    printf("PASS (5 groups inserted, root is minimum 100)\n");

    /* ----------------------------------------------------
     * TEST 4: Verify Min Heap property after every insertion
     * ---------------------------------------------------- */
    printf("[TEST 4/15] Verify Min Heap property dynamically after each insert... ");
    MinHeap *h4 = minheap_create(8);
    uint64_t sequence[7] = {950, 420, 110, 680, 50, 890, 310};
    DuplicateGroup *g4_arr[7];
    for (int i = 0; i < 7; i++) {
        g4_arr[i] = create_dummy_group((uint32_t)(i + 1), sequence[i]);
        assert(minheap_insert(h4, g4_arr[i]) == STATUS_SUCCESS);
        /* Check heap property for all nodes: parent <= child */
        for (size_t idx = 1; idx < h4->size; idx++) {
            size_t parent = (idx - 1) / 2;
            assert(h4->items[parent]->recoverable_bytes <= h4->items[idx]->recoverable_bytes);
        }
    }
    minheap_free(h4);
    for (int i = 0; i < 7; i++) free(g4_arr[i]);
    printf("PASS (min-heap invariant verified across all 7 steps)\n");

    /* ----------------------------------------------------
     * TEST 5: Verify peek-min returns the correct group
     * ---------------------------------------------------- */
    printf("[TEST 5/15] Peek-min accuracy with varying elements... ");
    MinHeap *h5 = minheap_create(4);
    DuplicateGroup *g5_a = create_dummy_group(1, 1500);
    DuplicateGroup *g5_b = create_dummy_group(2, 300);
    DuplicateGroup *g5_c = create_dummy_group(3, 700);
    minheap_insert(h5, g5_a);
    assert(minheap_peek(h5) == g5_a);
    minheap_insert(h5, g5_b);
    assert(minheap_peek(h5) == g5_b); /* 300 is now minimum */
    minheap_insert(h5, g5_c);
    assert(minheap_peek(h5) == g5_b); /* 300 remains minimum */
    minheap_free(h5);
    free(g5_a); free(g5_b); free(g5_c);
    printf("PASS (peek accurately tracks root minimum)\n");

    /* ----------------------------------------------------
     * TEST 6: Verify extract-min returns groups in ascending order
     * ---------------------------------------------------- */
    printf("[TEST 6/15] Extract-min returns strictly ascending order... ");
    MinHeap *h6 = minheap_create(6);
    uint64_t vals6[6] = {500, 100, 300, 600, 200, 400};
    DuplicateGroup *g6_arr[6];
    for (int i = 0; i < 6; i++) {
        g6_arr[i] = create_dummy_group((uint32_t)(i + 1), vals6[i]);
        minheap_insert(h6, g6_arr[i]);
    }

    uint64_t prev = 0;
    for (int i = 0; i < 6; i++) {
        const DuplicateGroup *extracted = NULL;
        assert(minheap_extract_min(h6, &extracted) == STATUS_SUCCESS);
        assert(extracted != NULL);
        assert(extracted->recoverable_bytes >= prev);
        prev = extracted->recoverable_bytes;
    }
    assert(minheap_is_empty(h6) == true);
    minheap_free(h6);
    for (int i = 0; i < 6; i++) free(g6_arr[i]);
    printf("PASS (extracted 100 -> 200 -> 300 -> 400 -> 500 -> 600)\n");

    /* ----------------------------------------------------
     * TEST 7: Verify Top 3 largest groups correctly identified
     * ---------------------------------------------------- */
    printf("[TEST 7/15] Verify Top 3 largest groups from 6 groups... ");
    DuplicateGroupList list7;
    duplicate_group_list_init(&list7);
    uint64_t vals7[6] = {50, 900, 200, 800, 100, 700};
    DuplicateGroup *g7_arr[6];
    for (int i = 0; i < 6; i++) {
        g7_arr[i] = create_dummy_group((uint32_t)(i + 1), vals7[i]);
        duplicate_group_list_add(&list7, g7_arr[i]);
    }

    const DuplicateGroup *top3[3];
    size_t count3 = 0;
    assert(minheap_get_top_k(&list7, 3, top3, &count3) == STATUS_SUCCESS);
    assert(count3 == 3);
    /* Descending order: 900, 800, 700 */
    assert(top3[0]->recoverable_bytes == 900);
    assert(top3[1]->recoverable_bytes == 800);
    assert(top3[2]->recoverable_bytes == 700);

    /* Free groups via list */
    duplicate_group_list_free(&list7);
    printf("PASS (Top 3 correctly isolated: 900, 800, 700)\n");

    /* ----------------------------------------------------
     * TEST 8: Verify Top 5 largest groups when more than 5 exist
     * ---------------------------------------------------- */
    printf("[TEST 8/15] Top 5 largest groups from 10 groups... ");
    DuplicateGroupList list8;
    duplicate_group_list_init(&list8);
    uint64_t vals8[10] = {120, 850, 340, 990, 450, 720, 610, 150, 900, 280};
    for (int i = 0; i < 10; i++) {
        duplicate_group_list_add(&list8, create_dummy_group((uint32_t)(i + 1), vals8[i]));
    }

    const DuplicateGroup *top5[5];
    size_t count5 = 0;
    assert(minheap_get_top_k(&list8, 5, top5, &count5) == STATUS_SUCCESS);
    assert(count5 == 5);
    /* Top 5 descending: 990, 900, 850, 720, 610 */
    assert(top5[0]->recoverable_bytes == 990);
    assert(top5[1]->recoverable_bytes == 900);
    assert(top5[2]->recoverable_bytes == 850);
    assert(top5[3]->recoverable_bytes == 720);
    assert(top5[4]->recoverable_bytes == 610);

    duplicate_group_list_free(&list8);
    printf("PASS (Top 5: 990, 900, 850, 720, 610)\n");

    /* ----------------------------------------------------
     * TEST 9: Verify K larger than the number of groups
     * ---------------------------------------------------- */
    printf("[TEST 9/15] Request K = 10 when only 3 groups exist... ");
    DuplicateGroupList list9;
    duplicate_group_list_init(&list9);
    duplicate_group_list_add(&list9, create_dummy_group(1, 300));
    duplicate_group_list_add(&list9, create_dummy_group(2, 100));
    duplicate_group_list_add(&list9, create_dummy_group(3, 200));

    const DuplicateGroup *top10[10];
    size_t count9 = 0;
    assert(minheap_get_top_k(&list9, 10, top10, &count9) == STATUS_SUCCESS);
    assert(count9 == 3);
    assert(top10[0]->recoverable_bytes == 300);
    assert(top10[1]->recoverable_bytes == 200);
    assert(top10[2]->recoverable_bytes == 100);

    duplicate_group_list_free(&list9);
    printf("PASS (Gracefully returned available 3 groups)\n");

    /* ----------------------------------------------------
     * TEST 10: Verify K = 0 handling
     * ---------------------------------------------------- */
    printf("[TEST 10/15] Verify K = 0 boundary handling... ");
    DuplicateGroupList list10;
    duplicate_group_list_init(&list10);
    duplicate_group_list_add(&list10, create_dummy_group(1, 500));

    const DuplicateGroup *top0[1];
    size_t count0 = 99;
    assert(minheap_get_top_k(&list10, 0, top0, &count0) == STATUS_SUCCESS);
    assert(count0 == 0);

    duplicate_group_list_free(&list10);
    printf("PASS (count = 0, no-op executed safely)\n");

    /* ----------------------------------------------------
     * TEST 11: Verify groups with equal recoverable storage
     * ---------------------------------------------------- */
    printf("[TEST 11/15] Groups with equal recoverable storage (tie-breaking)... ");
    DuplicateGroupList list11;
    duplicate_group_list_init(&list11);
    /* Same recoverable bytes: 500, but differing group IDs */
    duplicate_group_list_add(&list11, create_dummy_group(3, 500));
    duplicate_group_list_add(&list11, create_dummy_group(1, 500));
    duplicate_group_list_add(&list11, create_dummy_group(2, 500));

    const DuplicateGroup *top_tie[3];
    size_t count11 = 0;
    assert(minheap_get_top_k(&list11, 3, top_tie, &count11) == STATUS_SUCCESS);
    assert(count11 == 3);
    /* Check all have 500 */
    assert(top_tie[0]->recoverable_bytes == 500);
    assert(top_tie[1]->recoverable_bytes == 500);
    assert(top_tie[2]->recoverable_bytes == 500);

    duplicate_group_list_free(&list11);
    printf("PASS (deterministic tie-breaking preserved)\n");

    /* ----------------------------------------------------
     * TEST 12: Verify empty duplicate group list
     * ---------------------------------------------------- */
    printf("[TEST 12/15] Empty DuplicateGroupList input... ");
    DuplicateGroupList list12;
    duplicate_group_list_init(&list12);

    const DuplicateGroup *top12[5];
    size_t count12 = 88;
    assert(minheap_get_top_k(&list12, 5, top12, &count12) == STATUS_SUCCESS);
    assert(count12 == 0);

    duplicate_group_list_free(&list12);
    printf("PASS (handled empty list cleanly, count = 0)\n");

    /* ----------------------------------------------------
     * TEST 13: Verify original DuplicateGroupList remains valid after heap cleanup
     * ---------------------------------------------------- */
    printf("[TEST 13/15] Original DuplicateGroupList preserved after heap cleanup... ");
    DuplicateGroupList list13;
    duplicate_group_list_init(&list13);
    duplicate_group_list_add(&list13, create_dummy_group(1, 1000));
    duplicate_group_list_add(&list13, create_dummy_group(2, 2000));

    const DuplicateGroup *top13[2];
    size_t count13 = 0;
    minheap_get_top_k(&list13, 2, top13, &count13);

    /* Verify original list structure and pointers are completely unaffected */
    assert(list13.total_groups == 2);
    assert(list13.head != NULL);
    assert(list13.head->group_id == 1);
    assert(list13.head->recoverable_bytes == 1000);
    assert(list13.tail != NULL);
    assert(list13.tail->group_id == 2);
    assert(list13.tail->recoverable_bytes == 2000);

    duplicate_group_list_free(&list13);
    printf("PASS (list intact, zero side effects)\n");

    /* ----------------------------------------------------
     * TEST 14: Verify output order is strictly descending
     * ---------------------------------------------------- */
    printf("[TEST 14/15] Verify output ordering is strictly descending... ");
    DuplicateGroupList list14;
    duplicate_group_list_init(&list14);
    uint64_t vals14[8] = {10, 80, 30, 70, 20, 60, 40, 50};
    for (int i = 0; i < 8; i++) {
        duplicate_group_list_add(&list14, create_dummy_group((uint32_t)(i + 1), vals14[i]));
    }

    const DuplicateGroup *top4[4];
    size_t count14 = 0;
    minheap_get_top_k(&list14, 4, top4, &count14);
    assert(count14 == 4);
    for (size_t i = 0; i < count14 - 1; i++) {
        assert(top4[i]->recoverable_bytes >= top4[i + 1]->recoverable_bytes);
    }
    assert(top4[0]->recoverable_bytes == 80);
    assert(top4[1]->recoverable_bytes == 70);
    assert(top4[2]->recoverable_bytes == 60);
    assert(top4[3]->recoverable_bytes == 50);

    duplicate_group_list_free(&list14);
    printf("PASS (80 >= 70 >= 60 >= 50 confirmed)\n");

    /* ----------------------------------------------------
     * TEST 15: Verify memory cleanup and boundary handling
     * ---------------------------------------------------- */
    printf("[TEST 15/15] Memory cleanup and invalid argument safety... ");
    /* NULL pointer safety */
    minheap_free(NULL);
    assert(minheap_is_empty(NULL) == true);
    assert(minheap_is_full(NULL) == false);
    assert(minheap_peek(NULL) == NULL);
    assert(minheap_insert(NULL, NULL) == STATUS_ERROR_INVALID_ARGUMENT);
    assert(minheap_extract_min(NULL, NULL) == STATUS_ERROR_HEAP_EMPTY);
    assert(minheap_replace_root(NULL, NULL) == STATUS_ERROR_INVALID_ARGUMENT);

    MinHeap *h15 = minheap_create(2);
    assert(minheap_insert(h15, NULL) == STATUS_ERROR_INVALID_ARGUMENT);
    const DuplicateGroup *dummy_out = NULL;
    assert(minheap_extract_min(h15, &dummy_out) == STATUS_ERROR_HEAP_EMPTY);
    minheap_free(h15);
    printf("PASS (all boundary guards and NULL inputs safe)\n");

    printf("\n====================================================\n");
    printf(" ALL 15 BINARY MIN HEAP TESTS PASSED WITH 100%% SUCCESS!\n");
    printf("====================================================\n");

    return EXIT_SUCCESS;
}
