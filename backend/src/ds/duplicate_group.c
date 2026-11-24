/**
 * @file duplicate_group.c
 * @brief Custom linked-list implementation for duplicate file groups and file nodes.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "duplicate_group.h"
#include <stdlib.h>
#include <string.h>

DuplicateGroup *duplicate_group_create(uint32_t group_id, const FileRecord *first_file) {
    if (!first_file) return NULL;

    DuplicateGroup *group = (DuplicateGroup *)malloc(sizeof(DuplicateGroup));
    if (!group) return NULL;

    FileNode *node = (FileNode *)malloc(sizeof(FileNode));
    if (!node) {
        free(group);
        return NULL;
    }

    node->record = first_file;
    node->next = NULL;

    group->group_id = group_id;
    group->file_size = first_file->file_size;
    group->file_count = 1;
    group->recoverable_bytes = 0;
    group->files_head = node;
    group->files_tail = node;
    group->next = NULL;

    memcpy(group->sha256_binary, first_file->sha256_binary, sizeof(group->sha256_binary));
    memcpy(group->sha256_hex, first_file->sha256_hex, sizeof(group->sha256_hex));

    return group;
}

StatusCode duplicate_group_add_file(DuplicateGroup *group, const FileRecord *file) {
    if (!group || !file) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    FileNode *new_node = (FileNode *)malloc(sizeof(FileNode));
    if (!new_node) {
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    new_node->record = file;
    new_node->next = NULL;

    if (!group->files_head) {
        group->files_head = new_node;
        group->files_tail = new_node;
    } else {
        group->files_tail->next = new_node;
        group->files_tail = new_node;
    }

    group->file_count++;
    group->recoverable_bytes = (uint64_t)(group->file_count - 1) * group->file_size;

    return STATUS_SUCCESS;
}

void duplicate_group_free(DuplicateGroup *group) {
    if (!group) return;

    FileNode *curr = group->files_head;
    while (curr) {
        FileNode *next = curr->next;
        free(curr);
        curr = next;
    }

    free(group);
}

bool duplicate_group_remove_file(DuplicateGroup *group, uint64_t file_record_id) {
    if (!group || !group->files_head) return false;

    FileNode *prev = NULL;
    FileNode *curr = group->files_head;

    while (curr) {
        if (curr->record && curr->record->record_id == file_record_id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                group->files_head = curr->next;
            }

            if (curr == group->files_tail) {
                group->files_tail = prev;
            }

            free(curr);
            if (group->file_count > 0) group->file_count--;
            group->recoverable_bytes = (group->file_count > 1) ? (uint64_t)(group->file_count - 1) * group->file_size : 0;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

void duplicate_group_list_init(DuplicateGroupList *list) {
    if (!list) return;
    list->head = NULL;
    list->tail = NULL;
    list->total_groups = 0;
    list->total_duplicate_copies = 0;
    list->total_recoverable_bytes = 0;
}

void duplicate_group_list_add(DuplicateGroupList *list, DuplicateGroup *group) {
    if (!list || !group) return;

    group->next = NULL;

    if (!list->head) {
        list->head = group;
        list->tail = group;
    } else {
        list->tail->next = group;
        list->tail = group;
    }

    list->total_groups++;
    list->total_duplicate_copies += group->file_count;
    list->total_recoverable_bytes += group->recoverable_bytes;
}

void duplicate_group_list_free(DuplicateGroupList *list) {
    if (!list) return;

    DuplicateGroup *curr = list->head;
    while (curr) {
        DuplicateGroup *next = curr->next;
        duplicate_group_free(curr);
        curr = next;
    }

    list->head = NULL;
    list->tail = NULL;
    list->total_groups = 0;
    list->total_duplicate_copies = 0;
    list->total_recoverable_bytes = 0;
}

