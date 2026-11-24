/**
 * @file trie.c
 * @brief Custom Prefix Tree (Trie) implementation with sparse child edges in pure C11.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_BUFFER 4096

/* Forward declarations of internal helpers */
static TrieNode *trie_node_create(void);
static void trie_node_free(TrieNode *node);
static TrieEdge *trie_find_edge(const TrieNode *node, unsigned char b);
static TrieEdge *trie_add_edge(TrieNode *node, unsigned char b, TrieNode *child);
static void trie_remove_edge(TrieNode *node, unsigned char b);
static StatusCode collect_records_iterative(const TrieNode *start_node, TrieSearchResults *results);

void trie_normalize_path(const char *input, char *output, size_t max_len) {
    if (!input || !output || max_len == 0) return;

    size_t i = 0;
    while (input[i] != '\0' && i + 1 < max_len) {
        if (input[i] == '/') {
            output[i] = '\\';
        } else {
            output[i] = input[i];
        }
        i++;
    }
    output[i] = '\0';
}

static TrieNode *trie_node_create(void) {
    TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
    if (!node) return NULL;
    node->first_edge = NULL;
    node->records_head = NULL;
    node->is_terminal = false;
    return node;
}

static void trie_node_free(TrieNode *node) {
    if (!node) return;

    /* Free all edges and recursive children */
    TrieEdge *edge = node->first_edge;
    while (edge) {
        TrieEdge *next_edge = edge->next_edge;
        trie_node_free(edge->child);
        free(edge);
        edge = next_edge;
    }

    /* Free terminal FileRecord reference nodes */
    TrieRecordNode *rec_node = node->records_head;
    while (rec_node) {
        TrieRecordNode *next_rec = rec_node->next;
        free(rec_node);
        rec_node = next_rec;
    }

    free(node);
}

static TrieEdge *trie_find_edge(const TrieNode *node, unsigned char b) {
    if (!node) return NULL;
    TrieEdge *curr = node->first_edge;
    while (curr) {
        if (curr->byte_val == b) {
            return curr;
        }
        curr = curr->next_edge;
    }
    return NULL;
}

static TrieEdge *trie_add_edge(TrieNode *node, unsigned char b, TrieNode *child) {
    if (!node || !child) return NULL;

    TrieEdge *edge = (TrieEdge *)malloc(sizeof(TrieEdge));
    if (!edge) return NULL;

    edge->byte_val = b;
    edge->child = child;
    edge->next_edge = node->first_edge;
    node->first_edge = edge;

    return edge;
}

static void trie_remove_edge(TrieNode *node, unsigned char b) {
    if (!node) return;

    TrieEdge *prev = NULL;
    TrieEdge *curr = node->first_edge;
    while (curr) {
        if (curr->byte_val == b) {
            if (prev) {
                prev->next_edge = curr->next_edge;
            } else {
                node->first_edge = curr->next_edge;
            }
            trie_node_free(curr->child);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next_edge;
    }
}

Trie *trie_init(void) {
    Trie *trie = (Trie *)malloc(sizeof(Trie));
    if (!trie) return NULL;

    trie->root = trie_node_create();
    if (!trie->root) {
        free(trie);
        return NULL;
    }

    trie->total_paths = 0;
    trie->total_nodes = 1;
    return trie;
}

StatusCode trie_insert(Trie *trie, const char *filepath, const FileRecord *record) {
    if (!trie || !filepath || !record) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    char norm_path[MAX_PATH_BUFFER];
    trie_normalize_path(filepath, norm_path, sizeof(norm_path));

    TrieNode *curr = trie->root;
    const unsigned char *p = (const unsigned char *)norm_path;

    while (*p != '\0') {
        unsigned char b = *p;
        TrieEdge *edge = trie_find_edge(curr, b);

        if (!edge) {
            TrieNode *child = trie_node_create();
            if (!child) {
                return STATUS_ERROR_OUT_OF_MEMORY;
            }
            trie->total_nodes++;

            edge = trie_add_edge(curr, b, child);
            if (!edge) {
                trie_node_free(child);
                trie->total_nodes--;
                return STATUS_ERROR_OUT_OF_MEMORY;
            }
        }

        curr = edge->child;
        p++;
    }

    /* Check if this exact record is already registered at this terminal node */
    TrieRecordNode *check = curr->records_head;
    while (check) {
        if (check->record == record) {
            /* Duplicate pointer reference already indexed */
            return STATUS_SUCCESS;
        }
        check = check->next;
    }

    TrieRecordNode *new_rec = (TrieRecordNode *)malloc(sizeof(TrieRecordNode));
    if (!new_rec) {
        return STATUS_ERROR_OUT_OF_MEMORY;
    }
    new_rec->record = record;
    new_rec->next = curr->records_head;
    curr->records_head = new_rec;

    if (!curr->is_terminal) {
        curr->is_terminal = true;
        trie->total_paths++;
    }

    return STATUS_SUCCESS;
}

StatusCode trie_search_exact(const Trie *trie, const char *filepath, const FileRecord **out_record) {
    if (!trie || !filepath || !out_record) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }
    *out_record = NULL;

    char norm_path[MAX_PATH_BUFFER];
    trie_normalize_path(filepath, norm_path, sizeof(norm_path));

    const TrieNode *curr = trie->root;
    const unsigned char *p = (const unsigned char *)norm_path;

    while (*p != '\0') {
        TrieEdge *edge = trie_find_edge(curr, *p);
        if (!edge) {
            return STATUS_ERROR_PATH_NOT_FOUND;
        }
        curr = edge->child;
        p++;
    }

    if (curr && curr->is_terminal && curr->records_head) {
        *out_record = curr->records_head->record;
        return STATUS_SUCCESS;
    }

    return STATUS_ERROR_PATH_NOT_FOUND;
}

bool trie_contains(const Trie *trie, const char *filepath) {
    const FileRecord *rec = NULL;
    return (trie_search_exact(trie, filepath, &rec) == STATUS_SUCCESS);
}

static StatusCode append_result(TrieSearchResults *results, const FileRecord *rec) {
    if (results->count >= results->capacity) {
        size_t new_cap = (results->capacity == 0) ? 16 : results->capacity * 2;
        const FileRecord **new_arr = (const FileRecord **)realloc(
            results->records, new_cap * sizeof(const FileRecord *)
        );
        if (!new_arr) {
            return STATUS_ERROR_OUT_OF_MEMORY;
        }
        results->records = new_arr;
        results->capacity = new_cap;
    }
    results->records[results->count++] = rec;
    return STATUS_SUCCESS;
}

/**
 * @brief Traverse descendant nodes iteratively using an explicit work-stack.
 * Avoids call-stack recursion to protect against stack exhaustion on deep directory trees.
 */
static StatusCode collect_records_iterative(const TrieNode *start_node, TrieSearchResults *results) {
    if (!start_node || !results) return STATUS_SUCCESS;

    /* Simple dynamic stack of TrieNode pointers */
    size_t stack_cap = 64;
    size_t stack_size = 0;
    const TrieNode **stack = (const TrieNode **)malloc(stack_cap * sizeof(const TrieNode *));
    if (!stack) return STATUS_ERROR_OUT_OF_MEMORY;

    stack[stack_size++] = start_node;

    while (stack_size > 0) {
        const TrieNode *node = stack[--stack_size];

        /* If node is terminal, collect its FileRecords */
        if (node->is_terminal) {
            TrieRecordNode *rnode = node->records_head;
            while (rnode) {
                if (rnode->record) {
                    append_result(results, rnode->record);
                }
                rnode = rnode->next;
            }
        }

        /* Push all child nodes onto stack */
        TrieEdge *edge = node->first_edge;
        while (edge) {
            if (stack_size >= stack_cap) {
                size_t new_cap = stack_cap * 2;
                const TrieNode **new_stack = (const TrieNode **)realloc(
                    stack, new_cap * sizeof(const TrieNode *)
                );
                if (!new_stack) {
                    free(stack);
                    return STATUS_ERROR_OUT_OF_MEMORY;
                }
                stack = new_stack;
                stack_cap = new_cap;
            }
            stack[stack_size++] = edge->child;
            edge = edge->next_edge;
        }
    }

    free(stack);
    return STATUS_SUCCESS;
}

StatusCode trie_search_prefix(const Trie *trie, const char *prefix, TrieSearchResults *out_results) {
    if (!out_results) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }
    out_results->records = NULL;
    out_results->count = 0;
    out_results->capacity = 0;

    if (!trie || !trie->root) {
        return STATUS_SUCCESS;
    }

    /* Empty prefix returns all indexed paths */
    if (!prefix || prefix[0] == '\0') {
        return collect_records_iterative(trie->root, out_results);
    }

    char norm_prefix[MAX_PATH_BUFFER];
    trie_normalize_path(prefix, norm_prefix, sizeof(norm_prefix));

    const TrieNode *curr = trie->root;
    const unsigned char *p = (const unsigned char *)norm_prefix;

    while (*p != '\0') {
        TrieEdge *edge = trie_find_edge(curr, *p);
        if (!edge) {
            /* Prefix does not exist in Trie */
            return STATUS_SUCCESS;
        }
        curr = edge->child;
        p++;
    }

    /* Collect all terminal records under this prefix subtree */
    return collect_records_iterative(curr, out_results);
}

/**
 * @brief Helper structure for path deletion stack.
 */
typedef struct {
    TrieNode *parent;
    unsigned char byte_val;
    TrieNode *node;
} PathFrame;

StatusCode trie_remove(Trie *trie, const char *filepath) {
    if (!trie || !filepath || !trie->root) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    char norm_path[MAX_PATH_BUFFER];
    trie_normalize_path(filepath, norm_path, sizeof(norm_path));

    PathFrame frames[MAX_PATH_BUFFER];
    size_t depth = 0;

    TrieNode *curr = trie->root;
    const unsigned char *p = (const unsigned char *)norm_path;

    while (*p != '\0') {
        TrieEdge *edge = trie_find_edge(curr, *p);
        if (!edge) {
            return STATUS_ERROR_PATH_NOT_FOUND;
        }

        frames[depth].parent = curr;
        frames[depth].byte_val = *p;
        frames[depth].node = edge->child;
        depth++;

        curr = edge->child;
        p++;
    }

    if (!curr->is_terminal) {
        return STATUS_ERROR_PATH_NOT_FOUND;
    }

    /* Clear terminal status and free reference records */
    curr->is_terminal = false;
    TrieRecordNode *rec = curr->records_head;
    while (rec) {
        TrieRecordNode *next = rec->next;
        free(rec);
        rec = next;
    }
    curr->records_head = NULL;
    trie->total_paths--;

    /* Prune upwards if node has no children and is not terminal */
    for (int i = (int)depth - 1; i >= 0; i--) {
        TrieNode *node = frames[i].node;
        TrieNode *parent = frames[i].parent;
        unsigned char b = frames[i].byte_val;

        if (node->first_edge == NULL && !node->is_terminal) {
            trie_remove_edge(parent, b);
            trie->total_nodes--;
        } else {
            /* Node still has siblings or is terminal for a shorter path */
            break;
        }
    }

    return STATUS_SUCCESS;
}

void trie_print_matches(const TrieSearchResults *results, const char *query_prefix) {
    printf("====================================================\n");
    printf(" TRIE PREFIX SEARCH RESULTS\n");
    printf(" Query Prefix: \"%s\"\n", query_prefix ? query_prefix : "(empty)");
    printf("====================================================\n\n");

    if (!results || results->count == 0) {
        printf("No files found matching the specified prefix.\n\n");
        return;
    }

    printf("Total Matches Discovered: %zu\n\n", results->count);
    printf("#   | File Name                | Size (Bytes) | Full Filepath\n");
    printf("----+--------------------------+--------------+----------------------------------------\n");

    for (size_t i = 0; i < results->count; i++) {
        const FileRecord *r = results->records[i];
        printf("%-3zu | %-24.24s | %12llu | %s\n",
               i + 1,
               r->filename ? r->filename : "(unnamed)",
               (unsigned long long)r->file_size,
               r->filepath ? r->filepath : "");
    }
    printf("====================================================\n\n");
}

void trie_search_results_free(TrieSearchResults *results) {
    if (!results) return;
    if (results->records) {
        free(results->records);
        results->records = NULL;
    }
    results->count = 0;
    results->capacity = 0;
}

void trie_free(Trie *trie) {
    if (!trie) return;
    if (trie->root) {
        trie_node_free(trie->root);
        trie->root = NULL;
    }
    trie->total_paths = 0;
    trie->total_nodes = 0;
    free(trie);
}

