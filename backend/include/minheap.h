/**
 * @file minheap.h
 * @brief Custom Binary Min Heap data structure for ranking duplicate groups by recoverable storage.
 * @author DataVault Team (Academic Year 2026-2027)
 * 
 * In DUPLICATE-X, identifying the Top K largest storage-saving opportunities is accomplished
 * using a bounded Binary Min Heap of capacity K.
 * 
 * The heap stores non-owning pointers to existing DuplicateGroup structures.
 * Time Complexity for finding Top K from G groups: O(G log K)
 * Space Complexity: O(K) auxiliary memory
 */

#ifndef DATAVAULT_MINHEAP_H
#define DATAVAULT_MINHEAP_H

#include "duplicate_group.h"
#include "status.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Binary Min Heap representation storing non-owning pointers to DuplicateGroup objects.
 */
typedef struct {
    const DuplicateGroup **items;  /**< Dynamic array of pointers to DuplicateGroup */
    size_t size;                   /**< Current number of elements stored in the heap */
    size_t capacity;               /**< Maximum capacity allocated for the items array */
} MinHeap;

/**
 * @brief Create and initialize a new Binary Min Heap.
 * 
 * @param capacity Initial capacity of the heap. If 0, a default capacity is used.
 * @return Pointer to allocated MinHeap, or NULL if memory allocation fails.
 */
MinHeap *minheap_create(size_t capacity);

/**
 * @brief Destroy the heap and release its internal array.
 * 
 * IMPORTANT: This function NEVER frees the DuplicateGroup objects themselves.
 * Memory ownership of DuplicateGroup remains exclusively with DuplicateGroupList.
 * 
 * @param heap Pointer to MinHeap instance.
 */
void minheap_free(MinHeap *heap);

/**
 * @brief Check if the heap is empty.
 * 
 * @param heap Pointer to MinHeap.
 * @return true if heap is empty or NULL, false otherwise.
 */
bool minheap_is_empty(const MinHeap *heap);

/**
 * @brief Check if the heap is full.
 * 
 * @param heap Pointer to MinHeap.
 * @return true if heap size equals its capacity, false otherwise.
 */
bool minheap_is_full(const MinHeap *heap);

/**
 * @brief Retrieve the root (minimum) element without removing it.
 * 
 * @param heap Pointer to MinHeap.
 * @return Non-owning pointer to the root DuplicateGroup, or NULL if heap is empty.
 */
const DuplicateGroup *minheap_peek(const MinHeap *heap);

/**
 * @brief Insert a DuplicateGroup pointer into the Min Heap.
 * 
 * The element is appended at the end of the array and sifted up to restore
 * the min-heap invariant.
 * 
 * @param heap Pointer to MinHeap.
 * @param group Non-owning pointer to the DuplicateGroup to insert.
 * @return STATUS_SUCCESS on success, error code on failure.
 */
StatusCode minheap_insert(MinHeap *heap, const DuplicateGroup *group);

/**
 * @brief Remove and return the root (minimum) element from the heap.
 * 
 * The last element replaces the root and is sifted down to restore the
 * min-heap invariant.
 * 
 * @param heap Pointer to MinHeap.
 * @param out_group Pointer to receive the extracted DuplicateGroup pointer.
 * @return STATUS_SUCCESS on success, STATUS_ERROR_NOT_FOUND if empty.
 */
StatusCode minheap_extract_min(MinHeap *heap, const DuplicateGroup **out_group);

/**
 * @brief Replace the root element directly with a new group and restore min-heap property.
 * 
 * This is an efficient O(log K) operation combining extract-min and insert,
 * used heavily in the bounded Top K selection algorithm.
 * 
 * @param heap Pointer to MinHeap.
 * @param new_group New group to place at root.
 * @return STATUS_SUCCESS on success, error code otherwise.
 */
StatusCode minheap_replace_root(MinHeap *heap, const DuplicateGroup *new_group);

/**
 * @brief Extract the Top K duplicate groups with the largest recoverable storage.
 * 
 * Uses a bounded Min Heap of capacity K:
 * 1. For the first K groups, insert directly into the heap.
 * 2. For subsequent groups, compare with the root (current K-th largest candidate).
 *    If recoverable storage is strictly greater than root, replace root and sift down.
 * 3. After iterating all groups, the heap contains the K largest groups.
 * 4. Extracts elements into an array in DESCENDING order (largest first) without
 *    destroying or modifying the original groups.
 * 
 * Tie-breaking: For equal recoverable storage, deterministic tie-breaking based
 * on group_id is enforced (smaller group_id has priority).
 * 
 * @param list Pointer to the DuplicateGroupList to evaluate.
 * @param k The number of top groups to extract.
 * @param out_groups Caller-allocated array of at least k `const DuplicateGroup *` pointers.
 * @param out_count Pointer to receive the actual number of groups populated (<= k).
 * @return STATUS_SUCCESS on success, error code otherwise.
 */
StatusCode minheap_get_top_k(const DuplicateGroupList *list,
                             size_t k,
                             const DuplicateGroup **out_groups,
                             size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* DATAVAULT_MINHEAP_H */

