/**
 * @file api_response.c
 * @brief Dynamic HTTP response builder and JSON serializer for DUPLICATE-X.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "api_response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void api_buffer_init(ApiResponseBuffer *buf) {
    if (!buf) return;
    buf->data = NULL;
    buf->length = 0;
    buf->capacity = 0;
}

void api_buffer_free(ApiResponseBuffer *buf) {
    if (!buf) return;
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->length = 0;
    buf->capacity = 0;
}

bool api_buffer_append(ApiResponseBuffer *buf, const char *str) {
    if (!buf || !str) return false;
    size_t str_len = strlen(str);
    if (str_len == 0) return true;

    size_t needed = buf->length + str_len + 1;
    if (needed > buf->capacity) {
        size_t new_cap = (buf->capacity == 0) ? 1024 : buf->capacity * 2;
        while (new_cap < needed) {
            new_cap *= 2;
        }

        char *new_data = (char *)realloc(buf->data, new_cap);
        if (!new_data) return false;

        buf->data = new_data;
        buf->capacity = new_cap;
    }

    memcpy(buf->data + buf->length, str, str_len);
    buf->length += str_len;
    buf->data[buf->length] = '\0';
    return true;
}

bool api_buffer_append_printf(ApiResponseBuffer *buf, const char *fmt, ...) {
    if (!buf || !fmt) return false;

    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);

    int formatted_len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (formatted_len < 0) {
        va_end(args_copy);
        return false;
    }

    size_t needed = buf->length + (size_t)formatted_len + 1;
    if (needed > buf->capacity) {
        size_t new_cap = (buf->capacity == 0) ? 1024 : buf->capacity * 2;
        while (new_cap < needed) new_cap *= 2;

        char *new_data = (char *)realloc(buf->data, new_cap);
        if (!new_data) {
            va_end(args_copy);
            return false;
        }
        buf->data = new_data;
        buf->capacity = new_cap;
    }

    vsnprintf(buf->data + buf->length, (size_t)formatted_len + 1, fmt, args_copy);
    va_end(args_copy);

    buf->length += (size_t)formatted_len;
    buf->data[buf->length] = '\0';
    return true;
}

bool api_buffer_append_escaped_json(ApiResponseBuffer *buf, const char *str) {
    if (!buf) return false;
    if (!str) return api_buffer_append(buf, "\"\"");

    if (!api_buffer_append(buf, "\"")) return false;

    const unsigned char *p = (const unsigned char *)str;
    while (*p != '\0') {
        switch (*p) {
            case '\"': if (!api_buffer_append(buf, "\\\"")) return false; break;
            case '\\': if (!api_buffer_append(buf, "\\\\")) return false; break;
            case '\b': if (!api_buffer_append(buf, "\\b"))  return false; break;
            case '\f': if (!api_buffer_append(buf, "\\f"))  return false; break;
            case '\n': if (!api_buffer_append(buf, "\\n"))  return false; break;
            case '\r': if (!api_buffer_append(buf, "\\r"))  return false; break;
            case '\t': if (!api_buffer_append(buf, "\\t"))  return false; break;
            default:
                if (*p < 0x20) {
                    /* Control characters escaped as unicode \u00XX */
                    char hex_buf[8];
                    snprintf(hex_buf, sizeof(hex_buf), "\\u%04x", *p);
                    if (!api_buffer_append(buf, hex_buf)) return false;
                } else {
                    char c_buf[2] = { (char)*p, '\0' };
                    if (!api_buffer_append(buf, c_buf)) return false;
                }
                break;
        }
        p++;
    }

    return api_buffer_append(buf, "\"");
}

void api_build_http_response(ApiResponseBuffer *buf,
                             int status_code,
                             const char *status_text,
                             const char *json_body,
                             const char *origin_header) {
    if (!buf) return;
    api_buffer_init(buf);

    size_t body_len = json_body ? strlen(json_body) : 0;

    /* Status line */
    api_buffer_append_printf(buf, "HTTP/1.1 %d %s\r\n", status_code, status_text ? status_text : "OK");

    /* Standard headers */
    api_buffer_append(buf, "Content-Type: application/json; charset=utf-8\r\n");
    api_buffer_append_printf(buf, "Content-Length: %zu\r\n", body_len);
    api_buffer_append(buf, "Connection: close\r\n");
    api_buffer_append(buf, "Server: DUPLICATE-X/1.0 (DataVault C11 Engine)\r\n");

    /* Local CORS support if origin header provided and allowed */
    if (origin_header && strlen(origin_header) > 0) {
        api_buffer_append_printf(buf, "Access-Control-Allow-Origin: %s\r\n", origin_header);
        api_buffer_append(buf, "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
        api_buffer_append(buf, "Access-Control-Allow-Headers: Content-Type\r\n");
    }

    /* End of HTTP headers */
    api_buffer_append(buf, "\r\n");

    /* Payload body */
    if (json_body && body_len > 0) {
        api_buffer_append(buf, json_body);
    }
}

void api_build_error_response(ApiResponseBuffer *buf,
                              int http_status,
                              const char *error_message,
                              const char *origin_header) {
    const char *status_text = "Bad Request";
    if (http_status == 403) status_text = "Forbidden";
    else if (http_status == 404) status_text = "Not Found";
    else if (http_status == 405) status_text = "Method Not Allowed";
    else if (http_status == 500) status_text = "Internal Server Error";

    ApiResponseBuffer json_buf;
    api_buffer_init(&json_buf);
    api_buffer_append(&json_buf, "{\n  \"status\": \"error\",\n  \"code\": ");
    api_buffer_append_printf(&json_buf, "%d,\n  \"message\": ", http_status);
    api_buffer_append_escaped_json(&json_buf, error_message ? error_message : "Unknown error");
    api_buffer_append(&json_buf, "\n}");

    api_build_http_response(buf, http_status, status_text, json_buf.data, origin_header);
    api_buffer_free(&json_buf);
}

void api_build_raw_http_response(ApiResponseBuffer *buf,
                                 int status_code,
                                 const char *status_text,
                                 const char *content_type,
                                 const char *payload_data,
                                 size_t payload_len,
                                 const char *origin_header) {
    if (!buf) return;
    api_buffer_init(buf);

    /* Status line */
    api_buffer_append_printf(buf, "HTTP/1.1 %d %s\r\n", status_code, status_text ? status_text : "OK");

    /* Standard headers */
    api_buffer_append_printf(buf, "Content-Type: %s\r\n", content_type ? content_type : "text/plain");
    api_buffer_append_printf(buf, "Content-Length: %zu\r\n", payload_len);
    api_buffer_append(buf, "Connection: close\r\n");
    api_buffer_append(buf, "Server: DUPLICATE-X/1.0 (DataVault C11 Engine)\r\n");

    /* Local CORS support if origin header provided and allowed */
    if (origin_header && strlen(origin_header) > 0) {
        api_buffer_append_printf(buf, "Access-Control-Allow-Origin: %s\r\n", origin_header);
        api_buffer_append(buf, "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
        api_buffer_append(buf, "Access-Control-Allow-Headers: Content-Type\r\n");
    }

    /* End of HTTP headers */
    api_buffer_append(buf, "\r\n");

    /* Payload data */
    if (payload_data && payload_len > 0) {
        /* Need to append exact binary/text buffer */
        size_t needed = buf->length + payload_len + 1;
        if (needed > buf->capacity) {
            size_t new_cap = (buf->capacity == 0) ? 1024 : buf->capacity * 2;
            while (new_cap < needed) new_cap *= 2;
            char *new_data = (char *)realloc(buf->data, new_cap);
            if (new_data) {
                buf->data = new_data;
                buf->capacity = new_cap;
            }
        }
        if (buf->data && buf->capacity >= needed) {
            memcpy(buf->data + buf->length, payload_data, payload_len);
            buf->length += payload_len;
            buf->data[buf->length] = '\0';
        }
    }
}

