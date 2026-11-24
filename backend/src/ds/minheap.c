/**
 * @file minheap.c
 * @brief Custom Binary Min Heap implementation in pure C11.
 * @author DataVault Team (Academic Year 2026-2027)
 * 
 * Implements a bounded Binary Min Heap with sift-up and sift-down algorithms.
 * Used to identify Top K storage-saving duplicate groups with O(G log K) time complexity.
 */

#include "minheap.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Compare two DuplicateGroup pointers according to Min-Heap criteria.
 * 
 * Returns < 0 if a should be placed above b (i.e. a < b in recoverable storage).
 * Returns > 0 if a should be placed below b (i.e. a > b in recoverable storage).
 * 
 * Priority Metric: Recoverable Bytes (uint64_t).
 * Tie-Breaker: Group ID (smaller group ID takes precedence for determinism).
 */
static int compare_groups(const DuplicateGroup *a, const DuplicateGroup *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;

    if (a->recoverable_bytes < b->recoverable_bytes) {
        return -1;
    } else if (a->recoverable_bytes > b->recoverable_bytes) {
        return 1;
    }

    /* Tie-breaker: for equal recoverable storage, compare group_id */
    if (a->group_id < b->group_id) {
        return -1;
    } else if (a->group_id > b->group_id) {
        return 1;
    }

    return 0;
}

/**
 * @brief Swap two element pointers in the heap items array.
 */
static void swap_items(const DuplicateGroup **items, size_t i, size_t j) {
    const DuplicateGroup *temp = items[i];
    items[i] = items[j];
    items[j] = temp;
}

/**
 * @brief Sift up the element at index `idx` to restore the min-heap property.
 * 
 * Parent index formula: parent = (idx - 1) / 2
 * Time Complexity: O(log N)
 */
static void sift_up(MinHeap *heap, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        /* If child is smaller than parent, swap with parent */
        if (compare_groups(heap->items[idx], heap->items[parent]) < 0) {
            swap_items(heap->items, idx, parent);
            idx = parent;
        } else {
            break;
        }
    }
}

/**
 * @brief Sift down the element at index `idx` to restore the min-heap property.
 * 
 * Left child formula:  left  = 2 * idx + 1
 * Right child formula: right = 2 * idx + 2
 * Time Complexity: O(log N)
 */
static void sift_down(MinHeap *heap, size_t idx) {
    size_t size = heap->size;
    while (true) {
        size_t smallest = idx;
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;

        if (left < size && compare_groups(heap->items[left], heap->items[smallest]) < 0) {
            smallest = left;
        }

        if (right < size && compare_groups(heap->items[right], heap->items[smallest]) < 0) {
            smallest = right;
        }

        if (smallest != idx) {
            swap_items(heap->items, idx, smallest);
            idx = smallest;
        } else {
            break;
        }
    }
}

MinHeap *minheap_create(size_t capacity) {
    size_t cap = (capacity > 0) ? capacity : 16;

    MinHeap *heap = (MinHeap *)malloc(sizeof(MinHeap));
    if (!heap) return NULL;

    heap->items = (const DuplicateGroup **)malloc(cap * sizeof(const DuplicateGroup *));
    if (!heap->items) {
        free(heap);
        return NULL;
    }

    heap->size = 0;
    heap->capacity = cap;
    return heap;
}

void minheap_free(MinHeap *heap) {
    if (!heap) return;
    if (heap->items) {
        free(heap->items);
        heap->items = NULL;
    }
    heap->size = 0;
    heap->capacity = 0;
    free(heap);
}

bool minheap_is_empty(const MinHeap *heap) {
    return (!heap || heap->size == 0);
}

bool minheap_is_full(const MinHeap *heap) {
    if (!heap) return false;
    return (heap->size >= heap->capacity);
}

const DuplicateGroup *minheap_peek(const MinHeap *heap) {
    if (!heap || heap->size == 0 || !heap->items) {
        return NULL;
    }
    return heap->items[0];
}

StatusCode minheap_insert(MinHeap *heap, const DuplicateGroup *group) {
    if (!heap || !group) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    /* Dynamically grow if unbounded or reached initial capacity */
    if (heap->size >= heap->capacity) {
        size_t new_cap = heap->capacity * 2;
        const DuplicateGroup **new_items = (const DuplicateGroup **)realloc(
            heap->items, new_cap * sizeof(const DuplicateGroup *)
        );
        if (!new_items) {
            return STATUS_ERROR_OUT_OF_MEMORY;
        }
        heap->items = new_items;
        heap->capacity = new_cap;
    }

    /* Place at next available leaf position */
    size_t idx = heap->size;
    heap->items[idx] = group;
    heap->size++;

    /* Restore min-heap property by bubbling up */
    sift_up(heap, idx);

    return STATUS_SUCCESS;
}

StatusCode minheap_extract_min(MinHeap *heap, const DuplicateGroup **out_group) {
    if (!heap || heap->size == 0 || !out_group) {
        return STATUS_ERROR_HEAP_EMPTY;
    }

    /* Root is the minimum element */
    *out_group = heap->items[0];

    /* Move last leaf element to root */
    heap->size--;
    if (heap->size > 0) {
        heap->items[0] = heap->items[heap->size];
        sift_down(heap, 0);
    }

    return STATUS_SUCCESS;
}

StatusCode minheap_replace_root(MinHeap *heap, const DuplicateGroup *new_group) {
    if (!heap || heap->size == 0 || !new_group) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    heap->items[0] = new_group;
    sift_down(heap, 0);

    return STATUS_SUCCESS;
}

StatusCode minheap_get_top_k(const DuplicateGroupList *list,
                             size_t k,
                             const DuplicateGroup **out_groups,
                             size_t *out_count) {
    if (!out_count) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }
    *out_count = 0;

    if (!list || k == 0 || !out_groups || list->total_groups == 0) {
        return STATUS_SUCCESS;
    }

    /* Allocate a bounded Min Heap of capacity k */
    MinHeap *heap = minheap_create(k);
    if (!heap) {
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    /* Step 1 & 2: Process all duplicate groups in the list */
    const DuplicateGroup *curr = list->head;
    while (curr) {
        if (heap->size < k) {
            /* If heap has room, insert directly */
            minheap_insert(heap, curr);
        } else {
            /* Heap is full: compare with root (current k-th largest candidate) */
            if (compare_groups(curr, heap->items[0]) > 0) {
                /* Current group has larger recoverable storage than the smallest in Top K */
                minheap_replace_root(heap, curr);
            }
        }
        curr = curr->next;
    }

    size_t count = heap->size;
    *out_count = count;

    /* Step 3: Extract from heap to produce DESCENDING order.
     * Extracting from Min Heap yields elements in ascending order (smallest first).
     * Therefore, place extracted elements from right to left (index count - 1 down to 0).
     */
    for (size_t i = 0; i < count; i++) {
        const DuplicateGroup *min_item = NULL;
        minheap_extract_min(heap, &min_item);
        out_groups[count - 1 - i] = min_item;
    }

    minheap_free(heap);
    return STATUS_SUCCESS;
}
