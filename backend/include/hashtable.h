/**
 * @file hashtable.h
 * @brief Custom Hash Table with separate chaining and dynamic resizing.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_HASHTABLE_H
#define DATAVAULT_HASHTABLE_H

#include "duplicate_group.h"
#include "status.h"
#include <stdint.h>
#include <stddef.h>

#define DEFAULT_HASHTABLE_CAPACITY  61
#define DEFAULT_MAX_LOAD_FACTOR     0.75

/**
 * @struct HashEntry
 * @brief Hash Table bucket node representing candidate duplicates sharing a (size, sha256) key.
 */
typedef struct HashEntry {
    uint64_t file_size;             /**< Candidate file size */
    uint8_t sha256_binary[32];      /**< 32-byte SHA-256 content fingerprint */
    DuplicateGroup *groups_head;    /**< Linked list of verified duplicate groups under this key */
    struct HashEntry *next;         /**< Collision chain pointer (separate chaining) */
} HashEntry;

/**
 * @struct HashTable
 * @brief Master Hash Table structure with dynamic resizing.
 */
typedef struct HashTable {
    HashEntry **buckets;            /**< Dynamically allocated array of bucket pointers */
    size_t capacity;                /**< Total number of buckets */
    size_t count;                   /**< Total number of candidate keys stored */
    double max_load_factor;         /**< Threshold for resizing (0.75) */
} HashTable;

/**
 * @brief Compute FNV-1a 64-bit hash over file size and 32-byte SHA-256 digest for bucket mapping.
 * Note: FNV-1a is solely used for internal in-memory bucket indexing, never for file content identity.
 * @param file_size Exact file size in bytes.
 * @param sha256 32-byte SHA-256 binary digest.
 * @return 64-bit FNV-1a hash value.
 */
uint64_t fnv1a_hash_candidate(uint64_t file_size, const uint8_t sha256[32]);

/**
 * @brief Allocate and initialize a new custom Hash Table.
 * @param initial_capacity Initial number of buckets (0 uses default 61).
 * @return Pointer to new HashTable, or NULL on allocation failure.
 */
HashTable *hashtable_create(size_t initial_capacity);

/**
 * @brief Find an existing HashEntry matching the exact file size and 32-byte SHA-256 digest.
 * @param ht Pointer to HashTable.
 * @param file_size Candidate file size.
 * @param sha256 32-byte SHA-256 binary digest.
 * @return Pointer to matching HashEntry, or NULL if not found.
 */
HashEntry *hashtable_lookup(const HashTable *ht, uint64_t file_size, const uint8_t sha256[32]);

/**
 * @brief Insert or retrieve a candidate key into the Hash Table.
 * Triggers dynamic resizing if the load factor exceeds 0.75.
 * @param ht Pointer to HashTable.
 * @param file_size Candidate file size.
 * @param sha256 32-byte SHA-256 binary digest.
 * @param out_entry Output pointer receiving the new or existing HashEntry.
 * @return STATUS_SUCCESS or error code.
 */
StatusCode hashtable_get_or_insert(HashTable *ht,
                                   uint64_t file_size,
                                   const uint8_t sha256[32],
                                   HashEntry **out_entry);

/**
 * @brief Deallocate the Hash Table, its buckets, and internal HashEntries.
 * @param ht Pointer to HashTable.
 */
void hashtable_free(HashTable *ht);

#endif /* DATAVAULT_HASHTABLE_H */

