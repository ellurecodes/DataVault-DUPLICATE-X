/**
 * @file trie.h
 * @brief Custom Prefix Tree (Trie) data structure for indexing and searching file paths.
 * @author DataVault Team (Academic Year 2026-2027)
 * 
 * Implements a memory-efficient sparse child Trie using an edge linked-list representation.
 * Supports exact path lookup, fast prefix filtering, path normalization, and non-owning
 * FileRecord references.
 */

#ifndef DATAVAULT_TRIE_H
#define DATAVAULT_TRIE_H

#include "file_record.h"
#include "status.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct TrieNode TrieNode;
typedef struct TrieEdge TrieEdge;
typedef struct TrieRecordNode TrieRecordNode;

/**
 * @brief Linked list node referencing FileRecord instances at a terminal Trie node.
 * 
 * Non-owning: never frees the underlying FileRecord or its strings.
 */
struct TrieRecordNode {
    const FileRecord *record;       /**< Non-owning pointer to FileRecord */
    TrieRecordNode *next;           /**< Next FileRecord reference */
};

/**
 * @brief Sparse edge linking a parent node to a child node for a specific byte.
 */
struct TrieEdge {
    unsigned char byte_val;         /**< Character / UTF-8 byte label */
    TrieNode *child;                /**< Target child node */
    TrieEdge *next_edge;            /**< Next sibling edge in linked list */
};

/**
 * @brief Trie Node structure with sparse children edges.
 */
struct TrieNode {
    TrieEdge *first_edge;           /**< Head of edge linked list (sparse children) */
    TrieRecordNode *records_head;   /**< Head of linked list of FileRecord references (if terminal) */
    bool is_terminal;               /**< True if this node completes at least one indexed path */
};

/**
 * @brief Trie container structure.
 */
typedef struct {
    TrieNode *root;                 /**< Root node of the Trie */
    size_t total_paths;             /**< Total number of paths indexed */
    size_t total_nodes;             /**< Total number of allocated Trie nodes */
} Trie;

/**
 * @brief Container for prefix search query results.
 */
typedef struct {
    const FileRecord **records;     /**< Array of non-owning FileRecord pointers */
    size_t count;                   /**< Number of matching file records */
    size_t capacity;                /**< Allocated capacity of array */
} TrieSearchResults;

/**
 * @brief Initialize a new, empty Trie.
 * @return Pointer to allocated Trie, or NULL if memory allocation fails.
 */
Trie *trie_init(void);

/**
 * @brief Insert a file path and its associated FileRecord into the Trie.
 * 
 * Path normalization (converting '/' to '\\') is performed automatically.
 * 
 * @param trie Pointer to Trie.
 * @param filepath Full file path to index (UTF-8).
 * @param record Non-owning pointer to FileRecord.
 * @return STATUS_SUCCESS on success, error code otherwise.
 */
StatusCode trie_insert(Trie *trie, const char *filepath, const FileRecord *record);

/**
 * @brief Perform exact lookup for an indexed file path.
 * 
 * @param trie Pointer to Trie.
 * @param filepath Exact file path to find.
 * @param out_record Pointer to receive the non-owning FileRecord pointer.
 * @return STATUS_SUCCESS if found, STATUS_ERROR_PATH_NOT_FOUND if not found.
 */
StatusCode trie_search_exact(const Trie *trie, const char *filepath, const FileRecord **out_record);

/**
 * @brief Check whether an exact file path exists in the Trie.
 * 
 * @param trie Pointer to Trie.
 * @param filepath File path to check.
 * @return true if path is indexed, false otherwise.
 */
bool trie_contains(const Trie *trie, const char *filepath);

/**
 * @brief Search for all indexed files whose paths begin with the specified prefix.
 * 
 * If prefix is NULL or empty string (""), all indexed files in the Trie are returned.
 * 
 * @param trie Pointer to Trie.
 * @param prefix Prefix string to search (UTF-8).
 * @param out_results Pointer to TrieSearchResults to receive matching FileRecords.
 * @return STATUS_SUCCESS on success, error code otherwise.
 */
StatusCode trie_search_prefix(const Trie *trie, const char *prefix, TrieSearchResults *out_results);

/**
 * @brief Remove an indexed file path from the Trie.
 * 
 * Prunes unused child nodes and edges while preserving siblings.
 * 
 * @param trie Pointer to Trie.
 * @param filepath File path to remove.
 * @return STATUS_SUCCESS on success, STATUS_ERROR_PATH_NOT_FOUND if path did not exist.
 */
StatusCode trie_remove(Trie *trie, const char *filepath);

/**
 * @brief Print search results to standard output in a clean, tabular format.
 * 
 * @param results Pointer to TrieSearchResults.
 * @param query_prefix The prefix string that was queried.
 */
void trie_print_matches(const TrieSearchResults *results, const char *query_prefix);

/**
 * @brief Free the Trie, its nodes, edges, and record reference nodes.
 * 
 * IMPORTANT: Never frees the underlying FileRecord objects.
 * 
 * @param trie Pointer to Trie.
 */
void trie_free(Trie *trie);

/**
 * @brief Free the dynamically allocated search results container.
 * 
 * Does not free the FileRecord objects referenced within.
 * 
 * @param results Pointer to TrieSearchResults.
 */
void trie_search_results_free(TrieSearchResults *results);

/**
 * @brief Normalize a file path string into an output buffer.
 * 
 * Converts '/' forward slashes to '\\' backslashes uniformly.
 * 
 * @param input Raw input path.
 * @param output Destination buffer.
 * @param max_len Maximum length of destination buffer.
 */
void trie_normalize_path(const char *input, char *output, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* DATAVAULT_TRIE_H */

