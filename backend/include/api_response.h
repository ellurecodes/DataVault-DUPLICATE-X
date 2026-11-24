/**
 * @file api_response.h
 * @brief Dynamic HTTP response builder and JSON serializer for DUPLICATE-X.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_API_RESPONSE_H
#define DATAVAULT_API_RESPONSE_H

#include "file_record.h"
#include "duplicate_group.h"
#include "file_scanner.h"
#include "status.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dynamic string buffer for growing HTTP responses safely without buffer overflow.
 */
typedef struct {
    char *data;         /**< Pointer to dynamically allocated buffer */
    size_t length;      /**< Current string length excluding null-terminator */
    size_t capacity;    /**< Total allocated capacity */
} ApiResponseBuffer;

/**
 * @brief Initialize an ApiResponseBuffer.
 */
void api_buffer_init(ApiResponseBuffer *buf);

/**
 * @brief Free memory held by an ApiResponseBuffer.
 */
void api_buffer_free(ApiResponseBuffer *buf);

/**
 * @brief Append raw characters to buffer, reallocating dynamically if necessary.
 */
bool api_buffer_append(ApiResponseBuffer *buf, const char *str);

/**
 * @brief Append formatted printf string to buffer.
 */
bool api_buffer_append_printf(ApiResponseBuffer *buf, const char *fmt, ...);

/**
 * @brief Append a JSON-escaped string (handles quotes, backslashes, control characters).
 */
bool api_buffer_append_escaped_json(ApiResponseBuffer *buf, const char *str);

/**
 * @brief Build a full HTTP response (headers + JSON body + CORS if allowed).
 * 
 * @param buf Output response buffer.
 * @param status_code HTTP status code (200, 400, 404, 405, 500).
 * @param status_text HTTP status text ("OK", "Bad Request", etc.).
 * @param json_body JSON payload string.
 * @param origin_header Client Origin header from request, or NULL.
 */
void api_build_http_response(ApiResponseBuffer *buf,
                             int status_code,
                             const char *status_text,
                             const char *json_body,
                             const char *origin_header);

/**
 * @brief Helper to build a standardized JSON error response.
 */
void api_build_error_response(ApiResponseBuffer *buf,
                              int http_status,
                              const char *error_message,
                              const char *origin_header);

#ifdef __cplusplus
}
#endif

#endif /* DATAVAULT_API_RESPONSE_H */
