/**
 * @file api_router.c
 * @brief HTTP endpoint routing and request handling integrating C engine modules.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "api_server.h"
#include "duplicate_detector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>
#include <objbase.h>

/* Forward handlers */
static void handle_health(const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_stats(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_duplicates(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_top_k(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_search_prefix(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_folder_picker(const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_scan(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_quarantine_preview(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_quarantine_execute(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_quarantine_history(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_quarantine_restore_preview(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_quarantine_restore_execute(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp);
static void handle_static_file(const ApiHttpRequest *req, ApiResponseBuffer *resp);

/* Helper to convert hex character to integer */
static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void api_url_decode(char *dst, const char *src) {
    if (!dst || !src) return;

    while (*src != '\0') {
        if (*src == '%' && src[1] != '\0' && src[2] != '\0') {
            int h1 = hex_char_to_int(src[1]);
            int h2 = hex_char_to_int(src[2]);
            if (h1 >= 0 && h2 >= 0) {
                *dst++ = (char)((h1 << 4) | h2);
                src += 3;
                continue;
            }
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
            continue;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
}

static bool get_query_param(const char *query, const char *param_name, char *out_val, size_t max_len) {
    if (!query || !param_name || !out_val || max_len == 0) return false;
    out_val[0] = '\0';

    size_t name_len = strlen(param_name);
    const char *p = query;

    while (*p != '\0') {
        if (strncmp(p, param_name, name_len) == 0 && p[name_len] == '=') {
            p += name_len + 1;
            size_t i = 0;
            while (*p != '\0' && *p != '&' && i + 1 < max_len) {
                out_val[i++] = *p++;
            }
            out_val[i] = '\0';
            return true;
        }

        const char *amp = strchr(p, '&');
        if (!amp) break;
        p = amp + 1;
    }
    return false;
}

/**
 * @brief Safe JSON parser to extract a string array property like "directories": ["path1", "path2"].
 */
static bool parse_json_directories(const char *json, char dirs[32][1024], size_t *out_count) {
    if (!json || !dirs || !out_count) return false;
    *out_count = 0;

    /* Find "directories" key */
    const char *key = strstr(json, "\"directories\"");
    if (!key) return false;

    /* Locate opening bracket '[' */
    const char *open_bracket = strchr(key, '[');
    if (!open_bracket) return false;

    const char *p = open_bracket + 1;
    size_t count = 0;

    while (*p != '\0' && *p != ']') {
        /* Skip whitespace and commas */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',')) p++;

        if (*p == ']') break;

        if (*p == '\"') {
            p++; /* Skip opening quote */
            char val[1024];
            size_t vi = 0;

            while (*p != '\0' && *p != '\"' && vi + 1 < sizeof(val)) {
                if (*p == '\\' && p[1] != '\0') {
                    p++;
                    if (*p == '\\') val[vi++] = '\\';
                    else if (*p == '\"') val[vi++] = '\"';
                    else if (*p == '/') val[vi++] = '/';
                    else if (*p == 'n') val[vi++] = '\n';
                    else if (*p == 't') val[vi++] = '\t';
                    else val[vi++] = *p;
                } else {
                    val[vi++] = *p;
                }
                p++;
            }
            val[vi] = '\0';

            if (*p == '\"') p++; /* Skip closing quote */

            /* Strip leading and trailing quotes and whitespace */
            char *s = val;
            while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' || *s == '\"' || *s == '\'') s++;
            size_t vlen = strlen(s);
            while (vlen > 0 && (s[vlen - 1] == ' ' || s[vlen - 1] == '\t' || s[vlen - 1] == '\r' || s[vlen - 1] == '\n' || s[vlen - 1] == '\"' || s[vlen - 1] == '\'')) {
                s[vlen - 1] = '\0';
                vlen--;
            }

            if (count < 32 && vlen > 0) {
                strncpy(dirs[count], s, sizeof(dirs[count]) - 1);
                dirs[count][sizeof(dirs[count]) - 1] = '\0';
                count++;
            }
        } else {
            p++;
        }
    }

    *out_count = count;
    return true;
}

void api_route_request(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *out_resp) {
    if (!ctx || !req || !out_resp) return;

    /* Handle CORS preflight */
    if (strcmp(req->method, "OPTIONS") == 0) {
        api_build_http_response(out_resp, 204, "No Content", NULL, req->origin);
        return;
    }

    /* Route REST API endpoints (/api/...) */
    if (strncmp(req->path, "/api/", 5) == 0 || strcmp(req->path, "/api") == 0) {
        /* Route: GET /api/health */
        if (strcmp(req->path, "/api/health") == 0) {
            if (strcmp(req->method, "GET") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use GET.", req->origin);
                return;
            }
            handle_health(req, out_resp);
            return;
        }

        /* Route: GET /api/stats */
        if (strcmp(req->path, "/api/stats") == 0) {
            if (strcmp(req->method, "GET") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use GET.", req->origin);
                return;
            }
            handle_stats(ctx, req, out_resp);
            return;
        }

        /* Route: GET /api/duplicates */
        if (strcmp(req->path, "/api/duplicates") == 0) {
            if (strcmp(req->method, "GET") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use GET.", req->origin);
                return;
            }
            handle_duplicates(ctx, req, out_resp);
            return;
        }

        /* Route: GET /api/top-k */
        if (strcmp(req->path, "/api/top-k") == 0) {
            if (strcmp(req->method, "GET") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use GET.", req->origin);
                return;
            }
            handle_top_k(ctx, req, out_resp);
            return;
        }

        /* Route: GET /api/search/prefix */
        if (strcmp(req->path, "/api/search/prefix") == 0) {
            if (strcmp(req->method, "GET") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use GET.", req->origin);
                return;
            }
            handle_search_prefix(ctx, req, out_resp);
            return;
        }

        /* Route: POST /api/folder-picker */
        if (strcmp(req->path, "/api/folder-picker") == 0) {
            if (strcmp(req->method, "POST") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use POST.", req->origin);
                return;
            }
            handle_folder_picker(req, out_resp);
            return;
        }

        /* Route: POST /api/scan */
        if (strcmp(req->path, "/api/scan") == 0) {
            if (strcmp(req->method, "POST") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use POST.", req->origin);
                return;
            }
            handle_scan(ctx, req, out_resp);
            return;
        }

        /* Route: POST /api/quarantine/preview */
        if (strcmp(req->path, "/api/quarantine/preview") == 0) {
            if (strcmp(req->method, "POST") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use POST.", req->origin);
                return;
            }
            handle_quarantine_preview(ctx, req, out_resp);
            return;
        }

        /* Route: POST /api/quarantine/execute */
        if (strcmp(req->path, "/api/quarantine/execute") == 0) {
            if (strcmp(req->method, "POST") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use POST.", req->origin);
                return;
            }
            handle_quarantine_execute(ctx, req, out_resp);
            return;
        }

        /* Route: GET /api/quarantine/history */
        if (strcmp(req->path, "/api/quarantine/history") == 0) {
            if (strcmp(req->method, "GET") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use GET.", req->origin);
                return;
            }
            handle_quarantine_history(ctx, req, out_resp);
            return;
        }

        /* Route: POST /api/quarantine/restore/preview */
        if (strcmp(req->path, "/api/quarantine/restore/preview") == 0) {
            if (strcmp(req->method, "POST") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use POST.", req->origin);
                return;
            }
            handle_quarantine_restore_preview(ctx, req, out_resp);
            return;
        }

        /* Route: POST /api/quarantine/restore/execute */
        if (strcmp(req->path, "/api/quarantine/restore/execute") == 0) {
            if (strcmp(req->method, "POST") != 0) {
                api_build_error_response(out_resp, 405, "Method Not Allowed. Use POST.", req->origin);
                return;
            }
            handle_quarantine_restore_execute(ctx, req, out_resp);
            return;
        }

        /* Unknown /api/ endpoint returns HTTP 404 JSON */
        api_build_error_response(out_resp, 404, "Endpoint not found on DUPLICATE-X API server.", req->origin);
        return;
    }

    /* Otherwise, serve static frontend assets */
    handle_static_file(req, out_resp);
}

static void handle_static_file(const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    if (strcmp(req->method, "GET") != 0 && strcmp(req->method, "HEAD") != 0) {
        api_build_error_response(resp, 405, "Method Not Allowed. Use GET.", req->origin);
        return;
    }

    const char *path = req->path;
    if (strcmp(path, "/") == 0) {
        path = "/index.html";
    }

    /* Path traversal security check: reject '..', ':', or '\' */
    if (strstr(path, "..") != NULL || strchr(path, ':') != NULL || strchr(path, '\\') != NULL) {
        api_build_error_response(resp, 403, "Access Denied: Path traversal detected.", req->origin);
        return;
    }

    /* Strip leading slashes */
    while (*path == '/') path++;

    /* Locate file: first try frontend/<path>, then ../frontend/<path> */
    char full_path[1200];
    snprintf(full_path, sizeof(full_path), "frontend/%s", path);

    FILE *f = fopen(full_path, "rb");
    if (!f) {
        snprintf(full_path, sizeof(full_path), "../frontend/%s", path);
        f = fopen(full_path, "rb");
    }

    if (!f) {
        api_build_error_response(resp, 404, "Static file not found.", req->origin);
        return;
    }

    /* Determine file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        api_build_error_response(resp, 500, "Error reading file length.", req->origin);
        return;
    }

    char *data = (char *)malloc((size_t)size > 0 ? (size_t)size : 1);
    if (!data) {
        fclose(f);
        api_build_error_response(resp, 500, "Memory allocation error.", req->origin);
        return;
    }

    if (size > 0) {
        size_t nread = fread(data, 1, (size_t)size, f);
        (void)nread;
    }
    fclose(f);

    /* Determine MIME content type based on extension */
    const char *mime = "text/plain; charset=utf-8";
    const char *dot = strrchr(path, '.');
    if (dot) {
        if (_stricmp(dot, ".html") == 0 || _stricmp(dot, ".htm") == 0) {
            mime = "text/html; charset=utf-8";
        } else if (_stricmp(dot, ".css") == 0) {
            mime = "text/css; charset=utf-8";
        } else if (_stricmp(dot, ".js") == 0) {
            mime = "application/javascript; charset=utf-8";
        } else if (_stricmp(dot, ".json") == 0) {
            mime = "application/json; charset=utf-8";
        } else if (_stricmp(dot, ".png") == 0) {
            mime = "image/png";
        } else if (_stricmp(dot, ".jpg") == 0 || _stricmp(dot, ".jpeg") == 0) {
            mime = "image/jpeg";
        } else if (_stricmp(dot, ".ico") == 0) {
            mime = "image/x-icon";
        } else if (_stricmp(dot, ".svg") == 0) {
            mime = "image/svg+xml";
        }
    }

    api_build_raw_http_response(resp, 200, "OK", mime, data, (size_t)size, req->origin);
    free(data);
}

static void handle_health(const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    ApiResponseBuffer json;
    api_buffer_init(&json);
    api_buffer_append(&json, "{\n");
    api_buffer_append(&json, "  \"status\": \"ok\",\n");
    api_buffer_append(&json, "  \"application\": \"DUPLICATE-X\",\n");
    api_buffer_append(&json, "  \"team\": \"DataVault\",\n");
    api_buffer_append(&json, "  \"version\": \"1.0\"\n");
    api_buffer_append(&json, "}");

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

static void handle_stats(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    ApiResponseBuffer json;
    api_buffer_init(&json);

    api_buffer_append(&json, "{\n");
    api_buffer_append_printf(&json, "  \"total_scanned_files\": %zu,\n", ctx->inventory.count);
    api_buffer_append_printf(&json, "  \"successfully_hashed_files\": %zu,\n", ctx->successfully_hashed);
    api_buffer_append_printf(&json, "  \"duplicate_groups\": %zu,\n", ctx->duplicate_groups.total_groups);
    api_buffer_append_printf(&json, "  \"duplicate_copies\": %zu,\n", ctx->duplicate_groups.total_duplicate_copies);
    api_buffer_append_printf(&json, "  \"recoverable_bytes\": %llu,\n",
                             (unsigned long long)ctx->duplicate_groups.total_recoverable_bytes);
    api_buffer_append(&json, "  \"last_scan_status\": ");
    api_buffer_append_escaped_json(&json, ctx->last_scan_status);
    api_buffer_append(&json, "\n}");

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

static void handle_duplicates(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    ApiResponseBuffer json;
    api_buffer_init(&json);

    api_buffer_append(&json, "{\n");
    api_buffer_append_printf(&json, "  \"total_groups\": %zu,\n", ctx->duplicate_groups.total_groups);
    api_buffer_append_printf(&json, "  \"total_duplicate_copies\": %zu,\n", ctx->duplicate_groups.total_duplicate_copies);
    api_buffer_append_printf(&json, "  \"total_recoverable_bytes\": %llu,\n",
                             (unsigned long long)ctx->duplicate_groups.total_recoverable_bytes);
    api_buffer_append(&json, "  \"groups\": [\n");

    const DuplicateGroup *grp = ctx->duplicate_groups.head;
    size_t grp_idx = 0;

    while (grp) {
        if (grp_idx > 0) api_buffer_append(&json, ",\n");
        api_buffer_append(&json, "    {\n");
        api_buffer_append_printf(&json, "      \"group_id\": %u,\n", grp->group_id);
        api_buffer_append_printf(&json, "      \"file_size\": %llu,\n", (unsigned long long)grp->file_size);
        api_buffer_append_printf(&json, "      \"file_count\": %u,\n", grp->file_count);
        api_buffer_append_printf(&json, "      \"recoverable_bytes\": %llu,\n", (unsigned long long)grp->recoverable_bytes);
        api_buffer_append(&json, "      \"sha256\": ");
        api_buffer_append_escaped_json(&json, grp->sha256_hex);
        api_buffer_append(&json, ",\n      \"files\": [\n");

        const FileNode *node = grp->files_head;
        size_t file_idx = 0;
        while (node) {
            if (file_idx > 0) api_buffer_append(&json, ",\n");
            const FileRecord *r = node->record;
            api_buffer_append(&json, "        {\n");
            api_buffer_append_printf(&json, "          \"file_id\": %llu,\n", (unsigned long long)r->record_id);
            api_buffer_append_printf(&json, "          \"file_size\": %llu,\n", (unsigned long long)r->file_size);
            api_buffer_append(&json, "          \"filename\": ");
            api_buffer_append_escaped_json(&json, r->filename ? r->filename : "");
            api_buffer_append(&json, ",\n          \"filepath\": ");
            api_buffer_append_escaped_json(&json, r->filepath ? r->filepath : "");
            api_buffer_append(&json, ",\n          \"device_id\": ");
            api_buffer_append_escaped_json(&json, r->device_id ? r->device_id : "Unknown");
            api_buffer_append(&json, "\n        }");
            node = node->next;
            file_idx++;
        }
        api_buffer_append(&json, "\n      ]\n    }");
        grp = grp->next;
        grp_idx++;
    }

    api_buffer_append(&json, "\n  ]\n}");
    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

static void handle_top_k(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    char k_str[32] = "5";
    get_query_param(req->query, "k", k_str, sizeof(k_str));

    int k_val = atoi(k_str);
    if (k_val <= 0 || k_val > 1000) {
        api_build_error_response(resp, 400, "Invalid parameter 'k'. Must be an integer between 1 and 1000.", req->origin);
        return;
    }

    size_t k = (size_t)k_val;
    const DuplicateGroup **top_k = (const DuplicateGroup **)malloc(k * sizeof(const DuplicateGroup *));
    if (!top_k) {
        api_build_error_response(resp, 500, "Memory allocation failed for Top K ranking.", req->origin);
        return;
    }

    size_t actual_count = 0;
    minheap_get_top_k(&ctx->duplicate_groups, k, top_k, &actual_count);

    ApiResponseBuffer json;
    api_buffer_init(&json);
    api_buffer_append(&json, "{\n");
    api_buffer_append_printf(&json, "  \"requested_k\": %zu,\n", k);
    api_buffer_append_printf(&json, "  \"returned_count\": %zu,\n", actual_count);
    api_buffer_append(&json, "  \"top_groups\": [\n");

    for (size_t i = 0; i < actual_count; i++) {
        if (i > 0) api_buffer_append(&json, ",\n");
        const DuplicateGroup *g = top_k[i];
        api_buffer_append(&json, "    {\n");
        api_buffer_append_printf(&json, "      \"rank\": %zu,\n", i + 1);
        api_buffer_append_printf(&json, "      \"group_id\": %u,\n", g->group_id);
        api_buffer_append_printf(&json, "      \"file_size\": %llu,\n", (unsigned long long)g->file_size);
        api_buffer_append_printf(&json, "      \"copies\": %u,\n", g->file_count);
        api_buffer_append_printf(&json, "      \"recoverable_bytes\": %llu,\n", (unsigned long long)g->recoverable_bytes);
        api_buffer_append(&json, "      \"sha256\": ");
        api_buffer_append_escaped_json(&json, g->sha256_hex);
        api_buffer_append(&json, "\n    }");
    }

    api_buffer_append(&json, "\n  ]\n}");
    free(top_k);

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

static void handle_search_prefix(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    char raw_prefix[2048] = "";
    get_query_param(req->query, "prefix", raw_prefix, sizeof(raw_prefix));

    char decoded_prefix[2048];
    api_url_decode(decoded_prefix, raw_prefix);

    TrieSearchResults results;
    StatusCode sc = trie_search_prefix(ctx->trie, decoded_prefix, &results);
    if (sc != STATUS_SUCCESS) {
        api_build_error_response(resp, 500, "Trie prefix search operation failed.", req->origin);
        return;
    }

    ApiResponseBuffer json;
    api_buffer_init(&json);
    api_buffer_append(&json, "{\n");
    api_buffer_append(&json, "  \"prefix\": ");
    api_buffer_append_escaped_json(&json, decoded_prefix);
    api_buffer_append_printf(&json, ",\n  \"match_count\": %zu,\n", results.count);
    api_buffer_append(&json, "  \"matches\": [\n");

    for (size_t i = 0; i < results.count; i++) {
        if (i > 0) api_buffer_append(&json, ",\n");
        const FileRecord *r = results.records[i];
        api_buffer_append(&json, "    {\n");
        api_buffer_append(&json, "      \"filename\": ");
        api_buffer_append_escaped_json(&json, r->filename ? r->filename : "");
        api_buffer_append(&json, ",\n      \"filepath\": ");
        api_buffer_append_escaped_json(&json, r->filepath ? r->filepath : "");
        api_buffer_append_printf(&json, ",\n      \"file_size\": %llu,\n", (unsigned long long)r->file_size);
        api_buffer_append(&json, "      \"sha256\": ");
        api_buffer_append_escaped_json(&json, r->sha256_hex);
        api_buffer_append(&json, "\n    }");
    }

    api_buffer_append(&json, "\n  ]\n}");
    trie_search_results_free(&results);

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

static void handle_folder_picker(const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    if (strcmp(req->method, "POST") != 0) {
        api_build_error_response(resp, 405, "Method Not Allowed. Use POST.", req->origin);
        return;
    }

    /* Initialize COM library for this thread */
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool co_initialized = SUCCEEDED(hr);
    if (!co_initialized && hr != RPC_E_CHANGED_MODE) {
        api_build_error_response(resp, 500, "COM library initialization failed.", req->origin);
        return;
    }

    IFileOpenDialog *pDlg = NULL;
    hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IFileOpenDialog, (void **)&pDlg);

    if (FAILED(hr) || !pDlg) {
        if (co_initialized) CoUninitialize();
        api_build_error_response(resp, 500, "Failed to instantiate Windows FileOpenDialog.", req->origin);
        return;
    }

    /* Configure native folder selection options */
    FILEOPENDIALOGOPTIONS opt = 0;
    if (SUCCEEDED(IFileOpenDialog_GetOptions(pDlg, &opt))) {
        IFileOpenDialog_SetOptions(pDlg, opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    }
    IFileOpenDialog_SetTitle(pDlg, L"Select Target Directory for Deduplication - DUPLICATE-X");

    /* Show native modal dialog */
    hr = IFileOpenDialog_Show(pDlg, NULL);

    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) || hr == (HRESULT)0x800704C7) {
        /* User cancelled dialog cleanly */
        IFileOpenDialog_Release(pDlg);
        if (co_initialized) CoUninitialize();

        ApiResponseBuffer json;
        api_buffer_init(&json);
        api_buffer_append(&json, "{\n  \"status\": \"cancelled\",\n  \"selected\": false,\n  \"path\": null\n}");
        api_build_http_response(resp, 200, "OK", json.data, req->origin);
        api_buffer_free(&json);
        return;
    }

    if (FAILED(hr)) {
        IFileOpenDialog_Release(pDlg);
        if (co_initialized) CoUninitialize();
        api_build_error_response(resp, 500, "Windows folder selection dialog encountered an error.", req->origin);
        return;
    }

    /* Retrieve selected folder item */
    IShellItem *pItem = NULL;
    hr = IFileOpenDialog_GetResult(pDlg, &pItem);
    if (FAILED(hr) || !pItem) {
        IFileOpenDialog_Release(pDlg);
        if (co_initialized) CoUninitialize();
        api_build_error_response(resp, 500, "Failed to retrieve selected folder item.", req->origin);
        return;
    }

    PWSTR pszFilePath = NULL;
    hr = IShellItem_GetDisplayName(pItem, SIGDN_FILESYSPATH, &pszFilePath);
    if (FAILED(hr) || !pszFilePath) {
        IShellItem_Release(pItem);
        IFileOpenDialog_Release(pDlg);
        if (co_initialized) CoUninitialize();
        api_build_error_response(resp, 500, "Failed to resolve folder filesystem path.", req->origin);
        return;
    }

    /* Convert wide path (UTF-16) to UTF-8 */
    char utf8_path[MAX_PATH * 3] = {0};
    int conv_res = WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, utf8_path, sizeof(utf8_path) - 1, NULL, NULL);

    /* Free all COM resources cleanly */
    CoTaskMemFree(pszFilePath);
    IShellItem_Release(pItem);
    IFileOpenDialog_Release(pDlg);
    if (co_initialized) CoUninitialize();

    if (conv_res <= 0) {
        api_build_error_response(resp, 500, "Failed to convert selected folder path to UTF-8.", req->origin);
        return;
    }

    /* Build JSON response with actual absolute path */
    ApiResponseBuffer json;
    api_buffer_init(&json);
    api_buffer_append(&json, "{\n  \"status\": \"selected\",\n  \"selected\": true,\n  \"path\": ");
    api_buffer_append_escaped_json(&json, utf8_path);
    api_buffer_append(&json, "\n}");

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

static void handle_scan(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    if (!req->body || req->body_length == 0) {
        api_build_error_response(resp, 400, "Missing JSON request body in POST /api/scan.", req->origin);
        return;
    }

    char dirs_utf8[32][1024];
    size_t dir_count = 0;

    if (!parse_json_directories(req->body, dirs_utf8, &dir_count)) {
        api_build_error_response(resp, 400, "Invalid JSON: 'directories' array property is required.", req->origin);
        return;
    }

    if (dir_count == 0) {
        api_build_error_response(resp, 400, "The 'directories' array must contain at least one directory path.", req->origin);
        return;
    }

    /* Convert directory paths to wide characters for Windows Unicode file scanner */
    wchar_t *wide_dirs[32];
    for (size_t i = 0; i < dir_count; i++) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, dirs_utf8[i], -1, NULL, 0);
        if (wlen <= 0) {
            api_build_error_response(resp, 400, "Failed to decode UTF-8 directory string.", req->origin);
            for (size_t j = 0; j < i; j++) free(wide_dirs[j]);
            return;
        }
        wide_dirs[i] = (wchar_t *)malloc(wlen * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, dirs_utf8[i], -1, wide_dirs[i], wlen);

        /* If path does not exist directly, try ../<path> fallback (for when running from build directory) */
        DWORD attr = GetFileAttributesW(wide_dirs[i]);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            wchar_t fallback[MAX_PATH * 2];
            swprintf(fallback, sizeof(fallback) / sizeof(wchar_t), L"../%ls", wide_dirs[i]);
            if (GetFileAttributesW(fallback) != INVALID_FILE_ATTRIBUTES) {
                free(wide_dirs[i]);
                wide_dirs[i] = _wcsdup(fallback);
            }
        }
    }

    /* Clean up any existing scan session in context before running new scan */
    duplicate_group_list_free(&ctx->duplicate_groups);
    file_inventory_free(&ctx->inventory);
    if (ctx->trie) {
        trie_free(ctx->trie);
        ctx->trie = NULL;
    }

    file_inventory_init(&ctx->inventory);
    duplicate_group_list_init(&ctx->duplicate_groups);
    scanner_stats_init(&ctx->stats);
    ctx->successfully_hashed = 0;

    /* Execute synchronous multi-directory traversal and hashing */
    StatusCode sc = scan_multiple_directories((const wchar_t **)wide_dirs, dir_count, &ctx->inventory, &ctx->stats);

    for (size_t i = 0; i < dir_count; i++) {
        free(wide_dirs[i]);
    }

    if (sc != STATUS_SUCCESS && ctx->inventory.count == 0) {
        snprintf(ctx->last_scan_status, sizeof(ctx->last_scan_status), "SCAN_FAILED");
        char err_msg[1280];
        snprintf(err_msg, sizeof(err_msg), "Filesystem scan failed for all specified directories. Please verify directory exists: '%.512s'", dirs_utf8[0]);
        api_build_error_response(resp, 500, err_msg, req->origin);
        return;
    }

    /* Count successfully hashed files */
    for (size_t i = 0; i < ctx->inventory.count; i++) {
        if (ctx->inventory.records[i].hash_status == HASH_STATUS_SUCCESS ||
            ctx->inventory.records[i].hash_status == HASH_STATUS_EMPTY_FILE) {
            ctx->successfully_hashed++;
        }
    }

    /* Run Hash Table duplicate detection & byte-by-byte comparison */
    duplicate_detector_process(&ctx->inventory, &ctx->duplicate_groups);

    /* Index all discovered files in Prefix Trie */
    ctx->trie = trie_init();
    if (ctx->trie) {
        for (size_t i = 0; i < ctx->inventory.count; i++) {
            if (ctx->inventory.records[i].filepath) {
                trie_insert(ctx->trie, ctx->inventory.records[i].filepath, &ctx->inventory.records[i]);
            }
        }
    }

    ctx->has_completed_scan = true;
    snprintf(ctx->last_scan_status, sizeof(ctx->last_scan_status), "COMPLETED");

    /* Build JSON response */
    ApiResponseBuffer json;
    api_buffer_init(&json);
    api_buffer_append(&json, "{\n");
    api_buffer_append(&json, "  \"status\": \"success\",\n");
    api_buffer_append_printf(&json, "  \"directories_scanned\": %zu,\n", dir_count);
    api_buffer_append_printf(&json, "  \"files_discovered\": %zu,\n", ctx->inventory.count);
    api_buffer_append_printf(&json, "  \"files_successfully_hashed\": %zu,\n", ctx->successfully_hashed);
    api_buffer_append_printf(&json, "  \"duplicate_groups\": %zu,\n", ctx->duplicate_groups.total_groups);
    api_buffer_append_printf(&json, "  \"duplicate_copies\": %zu,\n", ctx->duplicate_groups.total_duplicate_copies);
    api_buffer_append_printf(&json, "  \"recoverable_bytes\": %llu,\n",
                             (unsigned long long)ctx->duplicate_groups.total_recoverable_bytes);
    api_buffer_append(&json, "  \"scan_status\": \"COMPLETED\"\n");
    api_buffer_append(&json, "}");

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

/* -------------------------------------------------------------------------
 * PART 7A: Safe Quarantine & File Recovery REST Handlers
 * ------------------------------------------------------------------------- */

static bool parse_json_uint32(const char *json, const char *key, uint32_t *out_val) {
    if (!json || !key || !out_val) return false;
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    const char *pos = strstr(json, search_key);
    if (!pos) return false;
    const char *colon = strchr(pos, ':');
    if (!colon) return false;
    unsigned int val = 0;
    if (sscanf(colon + 1, " %u", &val) == 1) {
        *out_val = (uint32_t)val;
        return true;
    }
    return false;
}

static bool parse_json_string(const char *json, const char *key, char *out_val, size_t max_len) {
    if (!json || !key || !out_val || max_len == 0) return false;
    out_val[0] = '\0';
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    const char *pos = strstr(json, search_key);
    if (!pos) return false;
    const char *colon = strchr(pos, ':');
    if (!colon) return false;
    const char *quote1 = strchr(colon, '\"');
    if (!quote1) return false;
    quote1++;
    size_t i = 0;
    while (*quote1 && *quote1 != '\"' && i + 1 < max_len) {
        if (*quote1 == '\\' && quote1[1] != '\0') {
            quote1++;
            if (*quote1 == '\\') out_val[i++] = '\\';
            else if (*quote1 == '\"') out_val[i++] = '\"';
            else if (*quote1 == '/') out_val[i++] = '/';
            else out_val[i++] = *quote1;
        } else {
            out_val[i++] = *quote1;
        }
        quote1++;
    }
    out_val[i] = '\0';
    return true;
}

static bool parse_json_uint64_array(const char *json, const char *key, uint64_t *out_arr, size_t max_items, size_t *out_count) {
    if (!json || !key || !out_arr || !out_count || max_items == 0) return false;
    *out_count = 0;
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    const char *pos = strstr(json, search_key);
    if (!pos) return false;
    const char *open_b = strchr(pos, '[');
    if (!open_b) return false;
    const char *p = open_b + 1;
    size_t cnt = 0;
    while (*p && *p != ']') {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',')) p++;
        if (*p == ']') break;
        char *endptr = NULL;
        unsigned long long val = strtoull(p, &endptr, 10);
        if (endptr && endptr != p) {
            if (cnt < max_items) {
                out_arr[cnt++] = (uint64_t)val;
            }
            p = endptr;
        } else {
            p++;
        }
    }
    *out_count = cnt;
    return true;
}

static void handle_quarantine_preview(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    if (!req->body || req->body_length == 0) {
        api_build_error_response(resp, 400, "Missing JSON request body in POST /api/quarantine/preview.", req->origin);
        return;
    }

    uint32_t group_id = 0;
    if (!parse_json_uint32(req->body, "group_id", &group_id)) {
        api_build_error_response(resp, 400, "Missing or invalid 'group_id' property.", req->origin);
        return;
    }

    uint64_t file_ids[32];
    size_t file_count = 0;
    if (!parse_json_uint64_array(req->body, "file_ids", file_ids, 32, &file_count) || file_count == 0) {
        api_build_error_response(resp, 400, "Missing or empty 'file_ids' array property.", req->origin);
        return;
    }

    QuarantinePreview prev;
    StatusCode sc = quarantine_preview_create(&ctx->quarantine_mgr, &ctx->duplicate_groups, group_id, file_ids, file_count, &prev);
    if (sc != STATUS_SUCCESS) {
        if (sc == STATUS_ERROR_SOLE_COPY_REMAINING) {
            api_build_error_response(resp, 400, "Safety violation: at least one original copy must remain (cannot quarantine all copies in group).", req->origin);
            return;
        }
        api_build_error_response(resp, 400, status_to_string(sc), req->origin);
        return;
    }

    ApiResponseBuffer json;
    api_buffer_init(&json);
    api_buffer_append(&json, "{\n");
    api_buffer_append(&json, "  \"status\": \"success\",\n");
    api_buffer_append_printf(&json, "  \"preview_id\": \"%s\",\n", prev.preview_id);
    api_buffer_append_printf(&json, "  \"group_id\": %u,\n", prev.group_id);
    api_buffer_append_printf(&json, "  \"file_count\": %zu,\n", prev.file_count);
    api_buffer_append_printf(&json, "  \"remaining_copies\": %zu,\n", prev.remaining_copies);
    api_buffer_append_printf(&json, "  \"estimated_recoverable_bytes\": %llu,\n", (unsigned long long)prev.estimated_recoverable_bytes);
    api_buffer_append_printf(&json, "  \"expires_in_seconds\": %d,\n", QUARANTINE_PREVIEW_EXPIRY_SECONDS);
    api_buffer_append(&json, "  \"confirmation_required\": \"QUARANTINE\",\n");
    api_buffer_append(&json, "  \"files\": [\n");
    for (size_t i = 0; i < prev.file_count; i++) {
        api_buffer_append_printf(&json, "    {\"file_id\": %llu, \"file_size\": %llu, \"filepath\": ",
                                 (unsigned long long)prev.file_ids[i], (unsigned long long)prev.file_sizes[i]);
        api_buffer_append_escaped_json(&json, prev.file_paths[i]);
        api_buffer_append_printf(&json, "}%s\n", (i + 1 < prev.file_count) ? "," : "");
    }
    api_buffer_append(&json, "  ]\n}");

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

static void handle_quarantine_execute(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    if (!req->body || req->body_length == 0) {
        api_build_error_response(resp, 400, "Missing JSON request body in POST /api/quarantine/execute.", req->origin);
        return;
    }

    char preview_id[64] = {0};
    if (!parse_json_string(req->body, "preview_id", preview_id, sizeof(preview_id)) || strlen(preview_id) == 0) {
        api_build_error_response(resp, 400, "Missing or invalid 'preview_id' property.", req->origin);
        return;
    }

    char confirmation[64] = {0};
    parse_json_string(req->body, "confirmation", confirmation, sizeof(confirmation));
    if (strcmp(confirmation, "QUARANTINE") != 0) {
        api_build_error_response(resp, 400, "Explicit confirmation 'QUARANTINE' is required.", req->origin);
        return;
    }

    size_t quarantined_count = 0;
    char tx_ids[32][64];
    StatusCode sc = quarantine_execute(&ctx->quarantine_mgr, &ctx->duplicate_groups, &ctx->inventory, preview_id, &quarantined_count, tx_ids);
    if (sc != STATUS_SUCCESS) {
        api_build_error_response(resp, 400, status_to_string(sc), req->origin);
        return;
    }

    ApiResponseBuffer json;
    api_buffer_init(&json);
    api_buffer_append(&json, "{\n");
    api_buffer_append(&json, "  \"status\": \"success\",\n");
    api_buffer_append_printf(&json, "  \"quarantined_count\": %zu,\n", quarantined_count);
    api_buffer_append(&json, "  \"transactions\": [\n");
    for (size_t i = 0; i < quarantined_count; i++) {
        api_buffer_append_printf(&json, "    \"%s\"%s\n", tx_ids[i], (i + 1 < quarantined_count) ? "," : "");
    }
    api_buffer_append(&json, "  ]\n}");

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

static void handle_quarantine_history(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    const QuarantineManifest *m = &ctx->quarantine_mgr.manifest;

    ApiResponseBuffer json;
    api_buffer_init(&json);
    api_buffer_append(&json, "{\n");
    api_buffer_append(&json, "  \"status\": \"success\",\n");
    api_buffer_append_printf(&json, "  \"total_records\": %zu,\n", m->count);
    api_buffer_append(&json, "  \"records\": [\n");

    for (size_t i = 0; i < m->count; i++) {
        const QuarantineRecord *r = &m->records[i];
        const char *st_str = (r->status == QUARANTINE_STATUS_ACTIVE) ? "ACTIVE" :
                             (r->status == QUARANTINE_STATUS_RESTORED) ? "RESTORED" : "FAILED";

        api_buffer_append(&json, "    {\n");
        api_buffer_append_printf(&json, "      \"transaction_id\": \"%s\",\n", r->transaction_id);
        api_buffer_append_printf(&json, "      \"group_id\": %u,\n", r->group_id);
        api_buffer_append_printf(&json, "      \"file_record_id\": %llu,\n", (unsigned long long)r->file_record_id);
        api_buffer_append(&json, "      \"original_path\": ");
        api_buffer_append_escaped_json(&json, r->original_path);
        api_buffer_append(&json, ",\n      \"quarantine_path\": ");
        api_buffer_append_escaped_json(&json, r->quarantine_path);
        api_buffer_append(&json, ",\n      \"original_filename\": ");
        api_buffer_append_escaped_json(&json, r->original_filename);
        api_buffer_append(&json, ",\n");
        api_buffer_append_printf(&json, "      \"file_size\": %llu,\n", (unsigned long long)r->file_size);
        api_buffer_append_printf(&json, "      \"sha256_hex\": \"%s\",\n", r->sha256_hex);
        api_buffer_append_printf(&json, "      \"quarantine_timestamp\": %lld,\n", (long long)r->quarantine_timestamp);
        api_buffer_append_printf(&json, "      \"restore_timestamp\": %lld,\n", (long long)r->restore_timestamp);
        api_buffer_append_printf(&json, "      \"status\": \"%s\"\n", st_str);
        api_buffer_append_printf(&json, "    }%s\n", (i + 1 < m->count) ? "," : "");
    }
    api_buffer_append(&json, "  ]\n}");

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

static void handle_quarantine_restore_preview(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    if (!req->body || req->body_length == 0) {
        api_build_error_response(resp, 400, "Missing JSON request body in POST /api/quarantine/restore/preview.", req->origin);
        return;
    }

    char tx_id[64] = {0};
    if (!parse_json_string(req->body, "transaction_id", tx_id, sizeof(tx_id)) || strlen(tx_id) == 0) {
        api_build_error_response(resp, 400, "Missing or invalid 'transaction_id' property.", req->origin);
        return;
    }

    QuarantinePreview prev;
    StatusCode sc = quarantine_restore_preview(&ctx->quarantine_mgr, tx_id, &prev);
    if (sc != STATUS_SUCCESS) {
        if (sc == STATUS_ERROR_DESTINATION_EXISTS) {
            api_build_error_response(resp, 409, "Destination file already exists at original location. Overwrite prevented.", req->origin);
            return;
        }
        if (sc == STATUS_ERROR_FILE_TAMPERED) {
            api_build_error_response(resp, 400, "Integrity check failed: quarantined file content does not match recorded SHA-256 digest.", req->origin);
            return;
        }
        api_build_error_response(resp, 400, status_to_string(sc), req->origin);
        return;
    }

    ApiResponseBuffer json;
    api_buffer_init(&json);
    api_buffer_append(&json, "{\n");
    api_buffer_append(&json, "  \"status\": \"success\",\n");
    api_buffer_append_printf(&json, "  \"preview_id\": \"%s\",\n", prev.preview_id);
    api_buffer_append_printf(&json, "  \"transaction_id\": \"%s\",\n", prev.restore_transaction_id);
    api_buffer_append(&json, "  \"original_path\": ");
    api_buffer_append_escaped_json(&json, prev.file_paths[0]);
    api_buffer_append(&json, ",\n");
    api_buffer_append_printf(&json, "  \"file_size\": %llu,\n", (unsigned long long)prev.file_sizes[0]);
    api_buffer_append_printf(&json, "  \"expires_in_seconds\": %d,\n", QUARANTINE_PREVIEW_EXPIRY_SECONDS);
    api_buffer_append(&json, "  \"confirmation_required\": \"RESTORE\"\n");
    api_buffer_append(&json, "}");

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

static void handle_quarantine_restore_execute(ApiEngineContext *ctx, const ApiHttpRequest *req, ApiResponseBuffer *resp) {
    if (!req->body || req->body_length == 0) {
        api_build_error_response(resp, 400, "Missing JSON request body in POST /api/quarantine/restore/execute.", req->origin);
        return;
    }

    char preview_id[64] = {0};
    if (!parse_json_string(req->body, "preview_id", preview_id, sizeof(preview_id)) || strlen(preview_id) == 0) {
        api_build_error_response(resp, 400, "Missing or invalid 'preview_id' property.", req->origin);
        return;
    }

    char confirmation[64] = {0};
    parse_json_string(req->body, "confirmation", confirmation, sizeof(confirmation));
    if (strcmp(confirmation, "RESTORE") != 0) {
        api_build_error_response(resp, 400, "Explicit confirmation 'RESTORE' is required.", req->origin);
        return;
    }

    char restored_path[1024] = {0};
    StatusCode sc = quarantine_restore_execute(&ctx->quarantine_mgr, preview_id, restored_path, sizeof(restored_path));
    if (sc != STATUS_SUCCESS) {
        api_build_error_response(resp, 400, status_to_string(sc), req->origin);
        return;
    }

    ApiResponseBuffer json;
    api_buffer_init(&json);
    api_buffer_append(&json, "{\n");
    api_buffer_append(&json, "  \"status\": \"success\",\n");
    api_buffer_append(&json, "  \"restored_path\": ");
    api_buffer_append_escaped_json(&json, restored_path);
    api_buffer_append(&json, "\n}");

    api_build_http_response(resp, 200, "OK", json.data, req->origin);
    api_buffer_free(&json);
}

