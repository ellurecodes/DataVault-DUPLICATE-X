/**
 * @file api_server.c
 * @brief Winsock2 HTTP REST Server implementation for DUPLICATE-X.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "api_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile bool g_server_running = false;

void api_engine_context_init(ApiEngineContext *ctx) {
    if (!ctx) return;
    file_inventory_init(&ctx->inventory);
    duplicate_group_list_init(&ctx->duplicate_groups);
    scanner_stats_init(&ctx->stats);
    ctx->trie = NULL;
    ctx->successfully_hashed = 0;
    snprintf(ctx->last_scan_status, sizeof(ctx->last_scan_status), "IDLE");
    ctx->has_completed_scan = false;
    quarantine_manager_init(&ctx->quarantine_mgr, "data/quarantine_manifest.json", "data/quarantine_journal.log");
}

void api_engine_context_cleanup(ApiEngineContext *ctx) {
    if (!ctx) return;
    quarantine_manager_cleanup(&ctx->quarantine_mgr);
    duplicate_group_list_free(&ctx->duplicate_groups);
    file_inventory_free(&ctx->inventory);
    if (ctx->trie) {
        trie_free(ctx->trie);
        ctx->trie = NULL;
    }
    ctx->successfully_hashed = 0;
    ctx->has_completed_scan = false;
}

void api_server_stop(void) {
    g_server_running = false;
}

bool api_is_origin_allowed(const char *origin) {
    if (!origin || strlen(origin) == 0) return true; /* CLI tools without origin are allowed */

    /* Local and file origin allowlist (including 'null' sent by browsers on file:// requests) */
    if (strcmp(origin, "null") == 0 ||
        strncmp(origin, "file:", 5) == 0 ||
        strncmp(origin, "http://localhost", 16) == 0 ||
        strncmp(origin, "https://localhost", 17) == 0 ||
        strncmp(origin, "http://127.0.0.1", 16) == 0 ||
        strncmp(origin, "https://127.0.0.1", 17) == 0) {
        return true;
    }
    return false;
}

bool api_parse_http_request(const char *raw_req, size_t req_len, ApiHttpRequest *out_req) {
    if (!raw_req || req_len == 0 || !out_req) return false;
    memset(out_req, 0, sizeof(ApiHttpRequest));

    /* Parse request line: METHOD PATH[?QUERY] HTTP/1.1 */
    const char *line_end = strstr(raw_req, "\r\n");
    if (!line_end) return false;

    char line_buf[2048];
    size_t line_len = (size_t)(line_end - raw_req);
    if (line_len >= sizeof(line_buf)) return false;

    memcpy(line_buf, raw_req, line_len);
    line_buf[line_len] = '\0';

    char full_path[2048] = "";
    char proto[32] = "";
    if (sscanf(line_buf, "%15s %2047s %31s", out_req->method, full_path, proto) < 2) {
        return false;
    }

    /* Split full_path into path and query */
    char *qmark = strchr(full_path, '?');
    if (qmark) {
        *qmark = '\0';
        strncpy(out_req->path, full_path, sizeof(out_req->path) - 1);
        strncpy(out_req->query, qmark + 1, sizeof(out_req->query) - 1);
    } else {
        strncpy(out_req->path, full_path, sizeof(out_req->path) - 1);
        out_req->query[0] = '\0';
    }

    /* Parse headers */
    const char *headers_start = line_end + 2;
    const char *body_marker = strstr(headers_start, "\r\n\r\n");
    if (!body_marker) {
        /* Incomplete request headers */
        return false;
    }

    /* Check Content-Length */
    const char *cl = strstr(headers_start, "Content-Length:");
    if (!cl) cl = strstr(headers_start, "content-length:");
    if (cl && cl < body_marker) {
        out_req->content_length = (size_t)strtoull(cl + 15, NULL, 10);
    }

    /* Check Origin header */
    const char *orig = strstr(headers_start, "Origin:");
    if (!orig) orig = strstr(headers_start, "origin:");
    if (orig && orig < body_marker) {
        orig += 7;
        while (*orig == ' ') orig++;
        const char *orig_end = strstr(orig, "\r\n");
        if (orig_end) {
            size_t olen = (size_t)(orig_end - orig);
            if (olen < sizeof(out_req->origin)) {
                memcpy(out_req->origin, orig, olen);
                out_req->origin[olen] = '\0';
            }
        }
    }

    /* Pointer to payload body */
    out_req->body = (char *)(body_marker + 4);
    out_req->body_length = req_len - (size_t)(out_req->body - raw_req);

    return true;
}

StatusCode api_server_run(ApiEngineContext *ctx, uint16_t port) {
    if (!ctx) return STATUS_ERROR_INVALID_ARGUMENT;

    WSADATA wsa_data;
    int wsa_res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_res != 0) {
        fprintf(stderr, "[ERROR] WSAStartup failed: %d\n", wsa_res);
        return STATUS_ERROR_UNKNOWN;
    }

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "[ERROR] Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return STATUS_ERROR_UNKNOWN;
    }

    /* Allow address reuse */
    BOOL opt = TRUE;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    /* Security: Bind strictly to loopback 127.0.0.1 (NEVER 0.0.0.0) */
    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(listen_sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "[ERROR] Socket bind to 127.0.0.1:%u failed: %d\n", port, WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return STATUS_ERROR_ACCESS_DENIED;
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "[ERROR] Socket listen failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return STATUS_ERROR_UNKNOWN;
    }

    printf("=================================\n");
    printf("        DUPLICATE-X\n");
    printf("       Powered by DataVault\n");
    printf("=================================\n\n");
    printf("Starting DUPLICATE-X Local API...\n\n");
    printf("Server address: http://127.0.0.1:%u\n\n", port);
    printf("Available endpoints:\n");
    printf("GET  /api/health\n");
    printf("POST /api/scan\n");
    printf("GET  /api/stats\n");
    printf("GET  /api/duplicates\n");
    printf("GET  /api/top-k?k=5\n");
    printf("GET  /api/search/prefix?prefix=...\n\n");
    printf("API server running. Press Ctrl+C to shutdown.\n\n");
    fflush(stdout);

    g_server_running = true;

    while (g_server_running) {
        /* Set select timeout to allow checking g_server_running */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_sock, &read_fds);

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int sel_res = select(0, &read_fds, NULL, NULL, &tv);
        if (sel_res <= 0) {
            continue; /* Timeout or signal */
        }

        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);

        if (client_sock == INVALID_SOCKET) {
            continue;
        }

        /* Set client socket receive timeout (5 seconds) */
        DWORD timeout_ms = 5000;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
        setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));

        /* Read HTTP request buffer */
        char req_buf[API_MAX_REQUEST_SIZE + 1];
        int bytes_received = recv(client_sock, req_buf, API_MAX_REQUEST_SIZE, 0);

        if (bytes_received > 0) {
            req_buf[bytes_received] = '\0';

            ApiHttpRequest req;
            ApiResponseBuffer resp;
            api_buffer_init(&resp);

            if (api_parse_http_request(req_buf, (size_t)bytes_received, &req)) {
                /* Validate Origin security allowlist */
                if (api_is_origin_allowed(req.origin)) {
                    api_route_request(ctx, &req, &resp);
                } else {
                    api_build_error_response(&resp, 403, "Origin forbidden: Cross-Origin access from non-local domains is prohibited.", NULL);
                }
            } else {
                api_build_error_response(&resp, 400, "Malformed HTTP request syntax.", NULL);
            }

            /* Send response */
            if (resp.data && resp.length > 0) {
                send(client_sock, resp.data, (int)resp.length, 0);
            }

            api_buffer_free(&resp);
        }

        closesocket(client_sock);
    }

    closesocket(listen_sock);
    WSACleanup();
    printf("\n[INFO] DUPLICATE-X API server stopped cleanly.\n");
    return STATUS_SUCCESS;
}

