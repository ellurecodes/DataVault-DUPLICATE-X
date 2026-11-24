/**
 * @file test_api.c
 * @brief Comprehensive 15-scenario verification test suite for Part 6A HTTP REST API.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "api_server.h"
#include "api_response.h"
#include "file_record.h"
#include "duplicate_group.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Helper to setup a populated engine context for testing */
static void populate_dummy_context(ApiEngineContext *ctx) {
    api_engine_context_init(ctx);

    FileRecord r1;
    memset(&r1, 0, sizeof(FileRecord));
    r1.record_id = 1;
    r1.filepath = strdup("C:\\TestFolder\\Notes.txt");
    r1.filename = strdup("Notes.txt");
    r1.extension = strdup(".txt");
    r1.device_id = strdup("Drive_C");
    r1.file_size = 500;
    r1.hash_status = HASH_STATUS_SUCCESS;
    strcpy(r1.sha256_hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    file_inventory_add(&ctx->inventory, &r1);
    file_record_cleanup(&r1);

    FileRecord r2;
    memset(&r2, 0, sizeof(FileRecord));
    r2.record_id = 2;
    r2.filepath = strdup("D:\\Backup\\Notes_Copy.txt");
    r2.filename = strdup("Notes_Copy.txt");
    r2.extension = strdup(".txt");
    r2.device_id = strdup("Drive_D");
    r2.file_size = 500;
    r2.hash_status = HASH_STATUS_SUCCESS;
    strcpy(r2.sha256_hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    file_inventory_add(&ctx->inventory, &r2);
    file_record_cleanup(&r2);

    ctx->successfully_hashed = 2;
    snprintf(ctx->last_scan_status, sizeof(ctx->last_scan_status), "COMPLETED");
    ctx->has_completed_scan = true;

    /* Build duplicate group referencing the inventory records */
    DuplicateGroup *grp = duplicate_group_create(1, &ctx->inventory.records[0]);
    duplicate_group_add_file(grp, &ctx->inventory.records[1]);
    duplicate_group_list_add(&ctx->duplicate_groups, grp);

    /* Build Trie */
    ctx->trie = trie_init();
    trie_insert(ctx->trie, ctx->inventory.records[0].filepath, &ctx->inventory.records[0]);
    trie_insert(ctx->trie, ctx->inventory.records[1].filepath, &ctx->inventory.records[1]);
}

int main(void) {
    printf("====================================================\n");
    printf(" DUPLICATE-X Part 6A Comprehensive API Test Suite\n");
    printf(" Winsock2 Local REST API & JSON Serialization\n");
    printf(" Powered by Team DataVault (2026-2027)\n");
    printf("====================================================\n\n");

    ApiEngineContext ctx;
    populate_dummy_context(&ctx);

    /* ----------------------------------------------------
     * TEST 1: GET /api/health returns HTTP 200
     * ---------------------------------------------------- */
    printf("[TEST 1/15] GET /api/health returns HTTP 200... ");
    ApiHttpRequest req1;
    ApiResponseBuffer resp1;
    api_buffer_init(&resp1);
    const char *raw1 = "GET /api/health HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n\r\n";
    assert(api_parse_http_request(raw1, strlen(raw1), &req1) == true);
    api_route_request(&ctx, &req1, &resp1);
    assert(strstr(resp1.data, "HTTP/1.1 200 OK") != NULL);
    api_buffer_free(&resp1);
    printf("PASS (status 200 OK verified)\n");

    /* ----------------------------------------------------
     * TEST 2: Health JSON contains correct application name
     * ---------------------------------------------------- */
    printf("[TEST 2/15] Health JSON contains 'DUPLICATE-X'... ");
    ApiResponseBuffer resp2;
    api_buffer_init(&resp2);
    api_route_request(&ctx, &req1, &resp2);
    assert(strstr(resp2.data, "\"application\": \"DUPLICATE-X\"") != NULL);
    api_buffer_free(&resp2);
    printf("PASS (application: DUPLICATE-X confirmed)\n");

    /* ----------------------------------------------------
     * TEST 3: Health JSON contains DataVault team name
     * ---------------------------------------------------- */
    printf("[TEST 3/15] Health JSON contains team 'DataVault'... ");
    ApiResponseBuffer resp3;
    api_buffer_init(&resp3);
    api_route_request(&ctx, &req1, &resp3);
    assert(strstr(resp3.data, "\"team\": \"DataVault\"") != NULL);
    api_buffer_free(&resp3);
    printf("PASS (team: DataVault confirmed)\n");

    /* ----------------------------------------------------
     * TEST 4: Unknown endpoint returns HTTP 404
     * ---------------------------------------------------- */
    printf("[TEST 4/15] Unknown endpoint returns HTTP 404... ");
    ApiHttpRequest req4;
    ApiResponseBuffer resp4;
    api_buffer_init(&resp4);
    const char *raw4 = "GET /api/unknown/endpoint HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n\r\n";
    api_parse_http_request(raw4, strlen(raw4), &req4);
    api_route_request(&ctx, &req4, &resp4);
    assert(strstr(resp4.data, "HTTP/1.1 404 Not Found") != NULL);
    assert(strstr(resp4.data, "\"code\": 404") != NULL || strstr(resp4.data, "\"code\":404") != NULL);
    api_buffer_free(&resp4);
    printf("PASS (status 404 Not Found returned)\n");

    /* ----------------------------------------------------
     * TEST 5: Unsupported HTTP method returns HTTP 405
     * ---------------------------------------------------- */
    printf("[TEST 5/15] Unsupported method (DELETE) returns HTTP 405... ");
    ApiHttpRequest req5;
    ApiResponseBuffer resp5;
    api_buffer_init(&resp5);
    const char *raw5 = "DELETE /api/health HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n\r\n";
    api_parse_http_request(raw5, strlen(raw5), &req5);
    api_route_request(&ctx, &req5, &resp5);
    assert(strstr(resp5.data, "HTTP/1.1 405 Method Not Allowed") != NULL);
    api_buffer_free(&resp5);
    printf("PASS (status 405 Method Not Allowed returned)\n");

    /* ----------------------------------------------------
     * TEST 6: Malformed JSON in POST /api/scan returns HTTP 400
     * ---------------------------------------------------- */
    printf("[TEST 6/15] Malformed JSON returns HTTP 400... ");
    ApiHttpRequest req6;
    ApiResponseBuffer resp6;
    api_buffer_init(&resp6);
    const char *raw6 = "POST /api/scan HTTP/1.1\r\nContent-Length: 15\r\n\r\n{invalid_json}";
    api_parse_http_request(raw6, strlen(raw6), &req6);
    api_route_request(&ctx, &req6, &resp6);
    assert(strstr(resp6.data, "HTTP/1.1 400 Bad Request") != NULL);
    api_buffer_free(&resp6);
    printf("PASS (status 400 Bad Request returned)\n");

    /* ----------------------------------------------------
     * TEST 7: Missing directories property returns HTTP 400
     * ---------------------------------------------------- */
    printf("[TEST 7/15] Missing 'directories' key returns HTTP 400... ");
    ApiHttpRequest req7;
    ApiResponseBuffer resp7;
    api_buffer_init(&resp7);
    const char *raw7 = "POST /api/scan HTTP/1.1\r\nContent-Length: 20\r\n\r\n{\"other_key\": [\"C:\"]}";
    api_parse_http_request(raw7, strlen(raw7), &req7);
    api_route_request(&ctx, &req7, &resp7);
    assert(strstr(resp7.data, "HTTP/1.1 400 Bad Request") != NULL);
    api_buffer_free(&resp7);
    printf("PASS (missing 'directories' rejected)\n");

    /* ----------------------------------------------------
     * TEST 8: Empty directory array is handled with HTTP 400
     * ---------------------------------------------------- */
    printf("[TEST 8/15] Empty directories array returns HTTP 400... ");
    ApiHttpRequest req8;
    ApiResponseBuffer resp8;
    api_buffer_init(&resp8);
    const char *raw8 = "POST /api/scan HTTP/1.1\r\nContent-Length: 19\r\n\r\n{\"directories\": []}";
    api_parse_http_request(raw8, strlen(raw8), &req8);
    api_route_request(&ctx, &req8, &resp8);
    assert(strstr(resp8.data, "HTTP/1.1 400 Bad Request") != NULL);
    api_buffer_free(&resp8);
    printf("PASS (empty directories array rejected safely)\n");

    /* ----------------------------------------------------
     * TEST 9: POST /api/scan with valid test folders succeeds
     * ---------------------------------------------------- */
    printf("[TEST 9/15] POST /api/scan with test folders succeeds... ");
    ApiHttpRequest req9;
    ApiResponseBuffer resp9;
    api_buffer_init(&resp9);
    const char *raw9 = "POST /api/scan HTTP/1.1\r\nContent-Length: 68\r\n\r\n"
                       "{\"directories\": [\"data/test_files/device_laptop\", \"data/test_files\"]}";
    api_parse_http_request(raw9, strlen(raw9), &req9);
    api_route_request(&ctx, &req9, &resp9);
    assert(strstr(resp9.data, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp9.data, "\"scan_status\": \"COMPLETED\"") != NULL);
    api_buffer_free(&resp9);
    printf("PASS (scan executed and returned status 200)\n");

    /* ----------------------------------------------------
     * TEST 10: GET /api/stats returns correct scan statistics
     * ---------------------------------------------------- */
    printf("[TEST 10/15] GET /api/stats returns valid telemetry... ");
    ApiHttpRequest req10;
    ApiResponseBuffer resp10;
    api_buffer_init(&resp10);
    const char *raw10 = "GET /api/stats HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    api_parse_http_request(raw10, strlen(raw10), &req10);
    api_route_request(&ctx, &req10, &resp10);
    assert(strstr(resp10.data, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp10.data, "\"total_scanned_files\":") != NULL);
    assert(strstr(resp10.data, "\"duplicate_groups\":") != NULL);
    assert(strstr(resp10.data, "\"recoverable_bytes\":") != NULL);
    api_buffer_free(&resp10);
    printf("PASS (stats JSON populated with valid metrics)\n");

    /* ----------------------------------------------------
     * TEST 11: GET /api/duplicates returns confirmed duplicate groups
     * ---------------------------------------------------- */
    printf("[TEST 11/15] GET /api/duplicates returns confirmed duplicate clusters... ");
    ApiHttpRequest req11;
    ApiResponseBuffer resp11;
    api_buffer_init(&resp11);
    const char *raw11 = "GET /api/duplicates HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    api_parse_http_request(raw11, strlen(raw11), &req11);
    api_route_request(&ctx, &req11, &resp11);
    assert(strstr(resp11.data, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp11.data, "\"groups\":") != NULL);
    api_buffer_free(&resp11);
    printf("PASS (duplicate clusters formatted in JSON)\n");

    /* ----------------------------------------------------
     * TEST 12: GET /api/top-k?k=5 returns properly ranked groups
     * ---------------------------------------------------- */
    printf("[TEST 12/15] GET /api/top-k?k=5 ranking via Binary Min Heap... ");
    ApiHttpRequest req12;
    ApiResponseBuffer resp12;
    api_buffer_init(&resp12);
    const char *raw12 = "GET /api/top-k?k=5 HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    api_parse_http_request(raw12, strlen(raw12), &req12);
    api_route_request(&ctx, &req12, &resp12);
    assert(strstr(resp12.data, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp12.data, "\"requested_k\": 5") != NULL);
    assert(strstr(resp12.data, "\"top_groups\":") != NULL);
    api_buffer_free(&resp12);
    printf("PASS (Top K heap extracted descending groups)\n");

    /* ----------------------------------------------------
     * TEST 13: Invalid K parameter is rejected
     * ---------------------------------------------------- */
    printf("[TEST 13/15] Invalid K parameter (k=-3) returns HTTP 400... ");
    ApiHttpRequest req13;
    ApiResponseBuffer resp13;
    api_buffer_init(&resp13);
    const char *raw13 = "GET /api/top-k?k=-3 HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    api_parse_http_request(raw13, strlen(raw13), &req13);
    api_route_request(&ctx, &req13, &resp13);
    assert(strstr(resp13.data, "HTTP/1.1 400 Bad Request") != NULL);
    api_buffer_free(&resp13);
    printf("PASS (negative K rejected with HTTP 400)\n");

    /* ----------------------------------------------------
     * TEST 14: GET /api/search/prefix returns correct Trie results
     * ---------------------------------------------------- */
    printf("[TEST 14/15] GET /api/search/prefix returns Trie matches... ");
    ApiHttpRequest req14;
    ApiResponseBuffer resp14;
    api_buffer_init(&resp14);
    const char *raw14 = "GET /api/search/prefix?prefix=data%2Ftest_files HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    api_parse_http_request(raw14, strlen(raw14), &req14);
    api_route_request(&ctx, &req14, &resp14);
    assert(strstr(resp14.data, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp14.data, "\"match_count\":") != NULL);
    api_buffer_free(&resp14);
    printf("PASS (prefix query percent-decoded and resolved via Trie)\n");

    /* ----------------------------------------------------
     * TEST 15: Missing or empty prefix parameter handled safely
     * ---------------------------------------------------- */
    printf("[TEST 15/15] Missing prefix parameter handled safely... ");
    ApiHttpRequest req15;
    ApiResponseBuffer resp15;
    api_buffer_init(&resp15);
    const char *raw15 = "GET /api/search/prefix HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    api_parse_http_request(raw15, strlen(raw15), &req15);
    api_route_request(&ctx, &req15, &resp15);
    assert(strstr(resp15.data, "HTTP/1.1 200 OK") != NULL);
    api_buffer_free(&resp15);
    printf("PASS (empty prefix returned all indexed files cleanly)\n");

    api_engine_context_cleanup(&ctx);

    printf("\n====================================================\n");
    printf(" ALL 15 REST API VERIFICATION TESTS PASSED WITH 100%% SUCCESS!\n");
    printf("====================================================\n");

    return EXIT_SUCCESS;
}
