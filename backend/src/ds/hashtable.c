/**
 * @file hashtable.c
 * @brief Custom Hash Table implementation with separate chaining and dynamic resizing.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "hashtable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Prime numbers table for uniform bucket distributions */
static const size_t PRIME_CAPACITIES[] = {
    61, 127, 251, 509, 1021, 2053, 4093, 8191,
    16381, 32771, 65537, 131071, 262147, 524287, 1048573
};
#define NUM_PRIMES (sizeof(PRIME_CAPACITIES) / sizeof(PRIME_CAPACITIES[0]))

static size_t get_next_prime_capacity(size_t current) {
    for (size_t i = 0; i < NUM_PRIMES; i++) {
        if (PRIME_CAPACITIES[i] > current) {
            return PRIME_CAPACITIES[i];
        }
    }
    return current * 2 + 1;
}

uint64_t fnv1a_hash_candidate(uint64_t file_size, const uint8_t sha256[32]) {
    const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    const uint64_t FNV_PRIME        = 1099511628211ULL;
    uint64_t hash = FNV_OFFSET_BASIS;

    /* Hash the 8 bytes of file_size */
    const uint8_t *size_bytes = (const uint8_t *)&file_size;
    for (size_t i = 0; i < sizeof(uint64_t); i++) {
        hash ^= size_bytes[i];
        hash *= FNV_PRIME;
    }

    /* Hash the 32 bytes of cryptographic SHA-256 digest */
    if (sha256) {
        for (size_t i = 0; i < 32; i++) {
            hash ^= sha256[i];
            hash *= FNV_PRIME;
        }
    }

    return hash;
}

HashTable *hashtable_create(size_t initial_capacity) {
    size_t cap = (initial_capacity > 0) ? initial_capacity : DEFAULT_HASHTABLE_CAPACITY;

    HashTable *ht = (HashTable *)malloc(sizeof(HashTable));
    if (!ht) return NULL;

    ht->buckets = (HashEntry **)calloc(cap, sizeof(HashEntry *));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }

    ht->capacity = cap;
    ht->count = 0;
    ht->max_load_factor = DEFAULT_MAX_LOAD_FACTOR;

    return ht;
}

HashEntry *hashtable_lookup(const HashTable *ht, uint64_t file_size, const uint8_t sha256[32]) {
    if (!ht || !ht->buckets || ht->capacity == 0 || !sha256) {
        return NULL;
    }

    uint64_t hash = fnv1a_hash_candidate(file_size, sha256);
    size_t bucket_idx = (size_t)(hash % ht->capacity);

    HashEntry *entry = ht->buckets[bucket_idx];
    while (entry) {
        if (entry->file_size == file_size &&
            memcmp(entry->sha256_binary, sha256, 32) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static StatusCode hashtable_resize(HashTable *ht) {
    size_t new_capacity = get_next_prime_capacity(ht->capacity);
    HashEntry **new_buckets = (HashEntry **)calloc(new_capacity, sizeof(HashEntry *));
    if (!new_buckets) {
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    /* Re-hash and redistribute all existing entries across the new bucket array */
    for (size_t i = 0; i < ht->capacity; i++) {
        HashEntry *entry = ht->buckets[i];
        while (entry) {
            HashEntry *next = entry->next;

            uint64_t hash = fnv1a_hash_candidate(entry->file_size, entry->sha256_binary);
            size_t new_idx = (size_t)(hash % new_capacity);

            /* Insert at head of new bucket */
            entry->next = new_buckets[new_idx];
            new_buckets[new_idx] = entry;

            entry = next;
        }
    }

    free(ht->buckets);
    ht->buckets = new_buckets;
    ht->capacity = new_capacity;

    return STATUS_SUCCESS;
}

StatusCode hashtable_get_or_insert(HashTable *ht,
                                   uint64_t file_size,
                                   const uint8_t sha256[32],
                                   HashEntry **out_entry) {
    if (!ht || !sha256 || !out_entry) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    /* Check if candidate key already exists */
    HashEntry *existing = hashtable_lookup(ht, file_size, sha256);
    if (existing) {
        *out_entry = existing;
        return STATUS_SUCCESS;
    }

    /* Check load factor threshold and resize if necessary */
    double current_load = (double)(ht->count + 1) / (double)ht->capacity;
    if (current_load > ht->max_load_factor) {
        hashtable_resize(ht);
    }

    /* Allocate new hash entry */
    HashEntry *new_entry = (HashEntry *)malloc(sizeof(HashEntry));
    if (!new_entry) {
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    new_entry->file_size = file_size;
    memcpy(new_entry->sha256_binary, sha256, 32);
    new_entry->groups_head = NULL;

    /* Compute bucket and insert at head of bucket chain */
    uint64_t hash = fnv1a_hash_candidate(file_size, sha256);
    size_t bucket_idx = (size_t)(hash % ht->capacity);

    new_entry->next = ht->buckets[bucket_idx];
    ht->buckets[bucket_idx] = new_entry;

    ht->count++;
    *out_entry = new_entry;

    return STATUS_SUCCESS;
}

void hashtable_free(HashTable *ht) {
    if (!ht) return;

    if (ht->buckets) {
        for (size_t i = 0; i < ht->capacity; i++) {
            HashEntry *entry = ht->buckets[i];
            while (entry) {
                HashEntry *next_entry = entry->next;

                /* Free any duplicate groups registered under this entry */
                DuplicateGroup *group = entry->groups_head;
                while (group) {
                    DuplicateGroup *next_group = group->next;
                    duplicate_group_free(group);
                    group = next_group;
                }

                free(entry);
                entry = next_entry;
            }
        }
        free(ht->buckets);
        ht->buckets = NULL;
    }

    free(ht);
}

