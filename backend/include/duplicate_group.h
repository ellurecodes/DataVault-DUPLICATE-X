/**
 * @file duplicate_group.h
 * @brief Linked-list data structures for confirmed duplicate file clusters.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_DUPLICATE_GROUP_H
#define DATAVAULT_DUPLICATE_GROUP_H

#include "file_record.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @struct FileNode
 * @brief Singly linked-list node referencing an individual file instance in the inventory.
 */
typedef struct FileNode {
    const FileRecord *record;       /**< Non-owning pointer to FileRecord in FileInventory */
    struct FileNode *next;          /**< Pointer to next file in this group */
} FileNode;

/**
 * @struct DuplicateGroup
 * @brief Header for a cluster of files with identical content.
 */
typedef struct DuplicateGroup {
    uint32_t group_id;              /**< Unique sequential group identifier */
    uint8_t sha256_binary[32];      /**< 32-byte content fingerprint */
    char sha256_hex[65];            /**< 64-char lowercase hex digest string */
    uint64_t file_size;             /**< Verified size in bytes of one copy */
    uint32_t file_count;            /**< Number of verified duplicate copies in this group */
    uint64_t recoverable_bytes;     /**< Space recovered by keeping only 1 copy: (file_count - 1) * file_size */
    FileNode *files_head;           /**< Head of file instances linked list */
    FileNode *files_tail;           /**< Tail pointer for O(1) appending */
    struct DuplicateGroup *next;    /**< Chained pointer for group list */
} DuplicateGroup;

/**
 * @struct DuplicateGroupList
 * @brief Master list of confirmed duplicate groups (with file_count >= 2).
 */
typedef struct DuplicateGroupList {
    DuplicateGroup *head;           /**< Head pointer of duplicate groups */
    DuplicateGroup *tail;           /**< Tail pointer */
    size_t total_groups;            /**< Total count of duplicate groups */
    size_t total_duplicate_copies;  /**< Sum of all file copies in duplicate groups */
    uint64_t total_recoverable_bytes; /**< Sum of recoverable storage across all groups */
} DuplicateGroupList;

/**
 * @brief Create and initialize a new DuplicateGroup.
 * @param group_id Unique identifier.
 * @param first_file Initial file record starting this group.
 * @return Newly allocated DuplicateGroup pointer, or NULL on memory failure.
 */
DuplicateGroup *duplicate_group_create(uint32_t group_id, const FileRecord *first_file);

/**
 * @brief Append a verified duplicate file record to an existing DuplicateGroup.
 * @param group Pointer to DuplicateGroup.
 * @param file Pointer to FileRecord to append.
 * @return STATUS_SUCCESS or STATUS_ERROR_OUT_OF_MEMORY.
 */
StatusCode duplicate_group_add_file(DuplicateGroup *group, const FileRecord *file);

/**
 * @brief Free heap memory allocated for a DuplicateGroup and its FileNodes.
 * Note: Does NOT free the underlying FileRecord (owned by FileInventory).
 * @param group Pointer to DuplicateGroup.
 */
void duplicate_group_free(DuplicateGroup *group);

/**
 * @brief Remove a file record from a DuplicateGroup by record ID and update recoverable bytes.
 * @param group Pointer to DuplicateGroup.
 * @param file_record_id ID of FileRecord to remove.
 * @return True if found and removed, false otherwise.
 */
bool duplicate_group_remove_file(DuplicateGroup *group, uint64_t file_record_id);

/**
 * @brief Initialize an empty DuplicateGroupList.
 * @param list Pointer to DuplicateGroupList.
 */
void duplicate_group_list_init(DuplicateGroupList *list);

/**
 * @brief Add a confirmed DuplicateGroup to the master group list.
 * @param list Pointer to DuplicateGroupList.
 * @param group Pointer to DuplicateGroup.
 */
void duplicate_group_list_add(DuplicateGroupList *list, DuplicateGroup *group);

/**
 * @brief Free all groups and nodes in a DuplicateGroupList.
 * @param list Pointer to DuplicateGroupList.
 */
void duplicate_group_list_free(DuplicateGroupList *list);

#endif /* DATAVAULT_DUPLICATE_GROUP_H */

