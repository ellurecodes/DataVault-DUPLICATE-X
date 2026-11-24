/**
 * @file api_server.h
 * @brief Lightweight local HTTP REST API server using Winsock2 for DUPLICATE-X.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_API_SERVER_H
#define DATAVAULT_API_SERVER_H

#include "file_record.h"
#include "duplicate_group.h"
#include "file_scanner.h"
#include "trie.h"
#include "minheap.h"
#include "api_response.h"
#include "status.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define API_SERVER_DEFAULT_PORT 8080
#define API_MAX_REQUEST_SIZE    65536  /* 64 KB maximum HTTP request */

/**
 * @brief Global engine context held and managed during the lifecycle of the API server.
 */
typedef struct {
    FileInventory inventory;            /**< Scanned file inventory */
    DuplicateGroupList duplicate_groups;/**< Confirmed duplicate groups */
    Trie *trie;                         /**< Prefix Trie indexing all filepaths */
    ScannerStats stats;                 /**< Scanner telemetry statistics */
    size_t successfully_hashed;         /**< Count of successfully hashed files */
    char last_scan_status[64];          /**< Status description ("IDLE", "COMPLETED", "ERROR") */
    bool has_completed_scan;            /**< True if at least one scan has run */
} ApiEngineContext;

/**
 * @brief Parsed HTTP request structure.
 */
typedef struct {
    char method[16];                    /**< "GET", "POST", "OPTIONS", etc. */
    char path[1024];                    /**< URL path (e.g. "/api/health") */
    char query[2048];                   /**< Query string (e.g. "k=5", "prefix=C%3A%5C") */
    char origin[512];                   /**< Origin header value for CORS allowlist */
    size_t content_length;              /**< Value of Content-Length header */
    char *body;                         /**< Pointer into request buffer pointing to body */
    size_t body_length;                 /**< Length of body in bytes */
} ApiHttpRequest;

/**
 * @brief Initialize the API engine context.
 */
void api_engine_context_init(ApiEngineContext *ctx);

/**
 * @brief Clean up and release all memory held in the API engine context.
 */
void api_engine_context_cleanup(ApiEngineContext *ctx);

/**
 * @brief Start the local HTTP REST server loop on 127.0.0.1:port.
 * 
 * Synchronously processes requests until interrupted by Ctrl+C or stop signal.
 * 
 * @param ctx Pointer to initialized ApiEngineContext.
 * @param port Port number to listen on (typically 8080).
 * @return STATUS_SUCCESS on clean shutdown, error code otherwise.
 */
StatusCode api_server_run(ApiEngineContext *ctx, uint16_t port);

/**
 * @brief Signal the running server loop to terminate cleanly.
 */
void api_server_stop(void);

/**
 * @brief Parse an HTTP raw request buffer into an ApiHttpRequest structure.
 */
bool api_parse_http_request(const char *raw_req, size_t req_len, ApiHttpRequest *out_req);

/**
 * @brief Route an HTTP request to its respective endpoint handler and build response.
 */
void api_route_request(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *out_resp);

/**
 * @brief Perform in-place URL percent-decoding (e.g. converting "%20" to " ").
 */
void api_url_decode(char *dst, const char *src);

/**
 * @brief Check if an Origin header is strictly from a trusted local origin.
 * 
 * Enforces local origin policy (e.g. "http://localhost:...", "http://127.0.0.1:...").
 * Never allows arbitrary "*".
 */
bool api_is_origin_allowed(const char *origin);

#ifdef __cplusplus
}
#endif

#endif /* DATAVAULT_API_SERVER_H */
