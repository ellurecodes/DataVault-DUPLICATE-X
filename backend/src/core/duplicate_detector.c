/**
 * @file duplicate_detector.c
 * @brief Duplicate detection engine integrating custom Hash Table and byte-by-byte verification.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "duplicate_detector.h"
#include "hashtable.h"
#include "file_compare.h"
#include "minheap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void duplicate_detector_format_size(uint64_t bytes, char *buffer, size_t buf_size) {
    if (!buffer || buf_size == 0) return;

    if (bytes < 1024) {
        snprintf(buffer, buf_size, "%llu bytes", (unsigned long long)bytes);
    } else if (bytes < 1024 * 1024) {
        double kb = (double)bytes / 1024.0;
        snprintf(buffer, buf_size, "%.2f KB (%llu bytes)", kb, (unsigned long long)bytes);
    } else if (bytes < 1024ULL * 1024 * 1024) {
        double mb = (double)bytes / (1024.0 * 1024.0);
        snprintf(buffer, buf_size, "%.2f MB (%llu bytes)", mb, (unsigned long long)bytes);
    } else {
        double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
        snprintf(buffer, buf_size, "%.2f GB (%llu bytes)", gb, (unsigned long long)bytes);
    }
}

StatusCode duplicate_detector_process(const FileInventory *inventory,
                                      DuplicateGroupList *out_groups) {
    if (!inventory || !out_groups) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    duplicate_group_list_init(out_groups);

    if (inventory->count == 0) {
        return STATUS_SUCCESS;
    }

    /* Initialize custom Hash Table with separate chaining */
    HashTable *ht = hashtable_create(0);
    if (!ht) {
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    uint32_t next_group_id = 1;

    /* Step 1 & 2: Iterate inventory and filter valid files */
    for (size_t i = 0; i < inventory->count; i++) {
        const FileRecord *rec = &inventory->records[i];

        /* Only successfully hashed files enter duplicate detection */
        if (rec->hash_status != HASH_STATUS_SUCCESS &&
            rec->hash_status != HASH_STATUS_EMPTY_FILE) {
            continue;
        }

        /* Step 3 & 4: Lookup or insert candidate bucket entry using (size, sha256) */
        HashEntry *entry = NULL;
        StatusCode sc = hashtable_get_or_insert(ht, rec->file_size, rec->sha256_binary, &entry);
        if (sc != STATUS_SUCCESS || !entry) {
            continue;
        }

        /* Step 5: If no existing candidate group exists, create first group */
        if (!entry->groups_head) {
            DuplicateGroup *new_grp = duplicate_group_create(next_group_id++, rec);
            if (new_grp) {
                entry->groups_head = new_grp;
            }
            continue;
        }

        /* Step 6: Candidate exists - perform byte-by-byte content verification */
        bool added_to_existing_group = false;
        DuplicateGroup *grp = entry->groups_head;

        while (grp) {
            /* Representative file from this candidate group */
            const FileRecord *rep_file = grp->files_head->record;

            bool is_identical = false;
            StatusCode comp_sc = file_compare_byte_by_byte(rec->filepath,
                                                           rep_file->filepath,
                                                           &is_identical);

            /* Step 7: Only when contents match byte-for-byte, add to duplicate group */
            if (comp_sc == STATUS_SUCCESS && is_identical) {
                duplicate_group_add_file(grp, rec);
                added_to_existing_group = true;
                break;
            }

            grp = grp->next;
        }

        /* Step 8: If content differed despite identical hash (cryptographic collision),
         * handle as a separate verified-content group under the same candidate entry */
        if (!added_to_existing_group) {
            DuplicateGroup *new_grp = duplicate_group_create(next_group_id++, rec);
            if (new_grp) {
                new_grp->next = entry->groups_head;
                entry->groups_head = new_grp;
            }
        }
    }

    /* Step 9 & 10: Collect confirmed duplicate groups (file_count >= 2) */
    uint32_t final_group_id = 1;
    for (size_t i = 0; i < ht->capacity; i++) {
        HashEntry *entry = ht->buckets[i];
        while (entry) {
            DuplicateGroup *prev = NULL;
            DuplicateGroup *grp = entry->groups_head;

            while (grp) {
                DuplicateGroup *next_grp = grp->next;

                if (grp->file_count >= 2) {
                    /* Unlink from entry so hashtable_free won't destroy it */
                    if (prev) {
                        prev->next = next_grp;
                    } else {
                        entry->groups_head = next_grp;
                    }

                    grp->group_id = final_group_id++;
                    duplicate_group_list_add(out_groups, grp);
                } else {
                    prev = grp;
                }

                grp = next_grp;
            }

            entry = entry->next;
        }
    }

    /* Free hash table and any solitary (non-duplicate) candidate entries */
    hashtable_free(ht);

    return STATUS_SUCCESS;
}

void duplicate_report_print(const DuplicateGroupList *groups,
                            size_t total_scanned,
                            size_t total_hashed) {
    printf("=========================================\n");
    printf(" DUPLICATE-X - DATAVAULT\n");
    printf(" DUPLICATE DETECTION REPORT\n");
    printf("=========================================\n\n");

    if (!groups || groups->total_groups == 0) {
        printf("No duplicate files detected across the scanned storage locations.\n\n");
    } else {
        DuplicateGroup *grp = groups->head;
        while (grp) {
            printf("Duplicate Group %u:\n\n", grp->group_id);

            FileNode *node = grp->files_head;
            size_t copy_idx = 1;
            while (node) {
                const FileRecord *r = node->record;
                printf("File %zu: %s\n", copy_idx++, r->filename ? r->filename : "(unnamed)");
                printf("Location: %s\n", r->device_id ? r->device_id : "Unknown");
                printf("Path:     %s\n\n", r->filepath ? r->filepath : "");
                node = node->next;
            }

            printf("Verification: MATCHED (Byte-by-Byte Confirmed)\n");
            printf("SHA-256:      %s\n", grp->sha256_hex);
            printf("File Size:    %llu bytes\n", (unsigned long long)grp->file_size);
            printf("Duplicate Copies: %u\n", grp->file_count);
            printf("Recoverable Storage: %llu bytes\n", (unsigned long long)grp->recoverable_bytes);
            printf("\n-----------------------------------------\n\n");

            grp = grp->next;
        }
    }

    /* Summary section */
    char rec_formatted[64];
    duplicate_detector_format_size(groups ? groups->total_recoverable_bytes : 0,
                                   rec_formatted, sizeof(rec_formatted));

    double kb = (double)(groups ? groups->total_recoverable_bytes : 0) / 1024.0;
    double mb = kb / 1024.0;

    printf("Summary:\n\n");
    printf("Files Scanned:        %zu\n", total_scanned);
    printf("Successfully Hashed:  %zu\n", total_hashed);
    printf("Duplicate Groups:     %zu\n", groups ? groups->total_groups : 0);
    printf("Total Duplicate Files: %zu\n", groups ? groups->total_duplicate_copies : 0);
    printf("Recoverable Storage:  %llu bytes (%.2f KB, %.2f MB)\n",
           (unsigned long long)(groups ? groups->total_recoverable_bytes : 0), kb, mb);
    printf("=========================================\n\n");
}

void duplicate_report_print_top_k(const DuplicateGroupList *groups, size_t k) {
    printf("====================================================\n");
    printf(" DUPLICATE-X: TOP %zu STORAGE RECOVERY OPPORTUNITIES\n", k);
    printf(" Powered by Custom Binary Min Heap Ranking Engine\n");
    printf("====================================================\n\n");

    if (!groups || groups->total_groups == 0 || k == 0) {
        printf("No duplicate groups available for ranking.\n\n");
        return;
    }

    /* Allocate pointer buffer for up to k groups */
    const DuplicateGroup **top_k = (const DuplicateGroup **)malloc(k * sizeof(const DuplicateGroup *));
    if (!top_k) {
        printf("[ERROR] Failed to allocate Top K results buffer.\n\n");
        return;
    }

    size_t actual_count = 0;
    StatusCode sc = minheap_get_top_k(groups, k, top_k, &actual_count);
    if (sc != STATUS_SUCCESS || actual_count == 0) {
        printf("No duplicate groups ranked.\n\n");
        free(top_k);
        return;
    }

    printf("Rank | Group ID | Copies | File Size (Bytes) | Recoverable Storage\n");
    printf("-----+----------+--------+-------------------+--------------------\n");

    for (size_t i = 0; i < actual_count; i++) {
        const DuplicateGroup *g = top_k[i];
        char rec_str[64];
        duplicate_detector_format_size(g->recoverable_bytes, rec_str, sizeof(rec_str));

        printf("#%-3zu | Group %-2u | %-6u | %17llu | %s\n",
               i + 1,
               g->group_id,
               g->file_count,
               (unsigned long long)g->file_size,
               rec_str);
    }
    printf("--------------------------------------------------------------------\n");
    if (actual_count < k) {
        printf("(Showing all %zu available duplicate group(s) since total < %zu)\n", actual_count, k);
    }

    char total_rec_str[64];
    duplicate_detector_format_size(groups->total_recoverable_bytes, total_rec_str, sizeof(total_rec_str));
    printf("Aggregate Recoverable Across All Groups: %s\n", total_rec_str);
    printf("====================================================================\n\n");

    free(top_k);
}


