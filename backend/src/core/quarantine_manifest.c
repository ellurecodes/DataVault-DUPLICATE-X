/**
 * @file quarantine_manifest.c
 * @brief Implementation of persistent quarantine manifest and write-ahead transaction journal.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "quarantine_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <time.h>

#define INITIAL_MANIFEST_CAPACITY 16

/* Forward declarations */
static const char *status_enum_to_str(QuarantineStatus status);
static QuarantineStatus status_str_to_enum(const char *str);
static void escape_json_string(FILE *f, const char *str);

StatusCode quarantine_manifest_init(QuarantineManifest *manifest, const char *manifest_path, const char *journal_path) {
    if (!manifest) return STATUS_ERROR_INVALID_ARGUMENT;

    manifest->records = (QuarantineRecord *)calloc(INITIAL_MANIFEST_CAPACITY, sizeof(QuarantineRecord));
    if (!manifest->records) return STATUS_ERROR_OUT_OF_MEMORY;

    manifest->count = 0;
    manifest->capacity = INITIAL_MANIFEST_CAPACITY;

    if (manifest_path && strlen(manifest_path) > 0) {
        strncpy(manifest->manifest_filepath, manifest_path, sizeof(manifest->manifest_filepath) - 1);
        manifest->manifest_filepath[sizeof(manifest->manifest_filepath) - 1] = '\0';
    } else {
        strcpy(manifest->manifest_filepath, "data/quarantine_manifest.json");
    }

    if (journal_path && strlen(journal_path) > 0) {
        strncpy(manifest->journal_filepath, journal_path, sizeof(manifest->journal_filepath) - 1);
        manifest->journal_filepath[sizeof(manifest->journal_filepath) - 1] = '\0';
    } else {
        strcpy(manifest->journal_filepath, "data/quarantine_journal.log");
    }

    /* Load existing manifest if file exists */
    quarantine_manifest_load(manifest);

    /* Reconcile pending operations */
    quarantine_manifest_reconcile(manifest);

    return STATUS_SUCCESS;
}

void quarantine_manifest_free(QuarantineManifest *manifest) {
    if (!manifest) return;
    if (manifest->records) {
        free(manifest->records);
        manifest->records = NULL;
    }
    manifest->count = 0;
    manifest->capacity = 0;
}

StatusCode quarantine_manifest_add(QuarantineManifest *manifest, const QuarantineRecord *rec) {
    if (!manifest || !rec) return STATUS_ERROR_INVALID_ARGUMENT;

    if (manifest->count >= manifest->capacity) {
        size_t new_cap = manifest->capacity * 2;
        QuarantineRecord *new_records = (QuarantineRecord *)realloc(manifest->records, new_cap * sizeof(QuarantineRecord));
        if (!new_records) return STATUS_ERROR_OUT_OF_MEMORY;
        manifest->records = new_records;
        manifest->capacity = new_cap;
    }

    manifest->records[manifest->count] = *rec;
    manifest->count++;

    /* Append to write-ahead journal first */
    quarantine_manifest_append_journal(manifest, rec, "QUARANTINE_ADD");

    /* Persist updated manifest atomically */
    quarantine_manifest_save(manifest);

    return STATUS_SUCCESS;
}

QuarantineRecord *quarantine_manifest_find(QuarantineManifest *manifest, const char *transaction_id) {
    if (!manifest || !transaction_id) return NULL;

    for (size_t i = 0; i < manifest->count; i++) {
        if (strcmp(manifest->records[i].transaction_id, transaction_id) == 0) {
            return &manifest->records[i];
        }
    }
    return NULL;
}

StatusCode quarantine_manifest_update_status(QuarantineManifest *manifest, const char *transaction_id, QuarantineStatus new_status) {
    if (!manifest || !transaction_id) return STATUS_ERROR_INVALID_ARGUMENT;

    QuarantineRecord *rec = quarantine_manifest_find(manifest, transaction_id);
    if (!rec) return STATUS_ERROR_PATH_NOT_FOUND;

    rec->status = new_status;
    if (new_status == QUARANTINE_STATUS_RESTORED) {
        rec->restore_timestamp = (int64_t)time(NULL);
    }

    quarantine_manifest_append_journal(manifest, rec, "STATUS_UPDATE");
    quarantine_manifest_save(manifest);

    return STATUS_SUCCESS;
}

static const char *status_enum_to_str(QuarantineStatus status) {
    switch (status) {
        case QUARANTINE_STATUS_ACTIVE:   return "ACTIVE";
        case QUARANTINE_STATUS_RESTORED: return "RESTORED";
        case QUARANTINE_STATUS_FAILED:   return "FAILED";
        default:                         return "PREVIEWED";
    }
}

static QuarantineStatus status_str_to_enum(const char *str) {
    if (!str) return QUARANTINE_STATUS_PREVIEWED;
    if (strcmp(str, "ACTIVE") == 0) return QUARANTINE_STATUS_ACTIVE;
    if (strcmp(str, "RESTORED") == 0) return QUARANTINE_STATUS_RESTORED;
    if (strcmp(str, "FAILED") == 0) return QUARANTINE_STATUS_FAILED;
    return QUARANTINE_STATUS_PREVIEWED;
}

static void escape_json_string(FILE *f, const char *str) {
    if (!str) return;
    for (const char *p = str; *p != '\0'; p++) {
        switch (*p) {
            case '\"': fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\b': fputs("\\b", f); break;
            case '\f': fputs("\\f", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if ((unsigned char)*p < 32) {
                    fprintf(f, "\\u%04x", (unsigned char)*p);
                } else {
                    fputc(*p, f);
                }
                break;
        }
    }
}

StatusCode quarantine_manifest_save(const QuarantineManifest *manifest) {
    if (!manifest) return STATUS_ERROR_INVALID_ARGUMENT;

    /* Write to temp file then atomic replace */
    char temp_path[1050];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", manifest->manifest_filepath);

    FILE *f = fopen(temp_path, "w");
    if (!f) return STATUS_ERROR_FILE_READ_FAILED;

    fputs("[\n", f);
    for (size_t i = 0; i < manifest->count; i++) {
        const QuarantineRecord *r = &manifest->records[i];
        fputs("  {\n", f);
        fprintf(f, "    \"transaction_id\": \"%s\",\n", r->transaction_id);
        fprintf(f, "    \"group_id\": %u,\n", r->group_id);
        fprintf(f, "    \"file_record_id\": %llu,\n", (unsigned long long)r->file_record_id);

        fputs("    \"original_path\": \"", f);
        escape_json_string(f, r->original_path);
        fputs("\",\n", f);

        fputs("    \"quarantine_path\": \"", f);
        escape_json_string(f, r->quarantine_path);
        fputs("\",\n", f);

        fputs("    \"original_filename\": \"", f);
        escape_json_string(f, r->original_filename);
        fputs("\",\n", f);

        fprintf(f, "    \"file_size\": %llu,\n", (unsigned long long)r->file_size);
        fprintf(f, "    \"sha256_hex\": \"%s\",\n", r->sha256_hex);
        fprintf(f, "    \"quarantine_timestamp\": %lld,\n", (long long)r->quarantine_timestamp);
        fprintf(f, "    \"restore_timestamp\": %lld,\n", (long long)r->restore_timestamp);
        fprintf(f, "    \"status\": \"%s\"\n", status_enum_to_str(r->status));
        fprintf(f, "  }%s\n", (i + 1 < manifest->count) ? "," : "");
    }
    fputs("]\n", f);

    fflush(f);
    fclose(f);

    /* Atomic replace */
    MoveFileExA(temp_path, manifest->manifest_filepath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);

    return STATUS_SUCCESS;
}

static bool extract_json_str(const char *obj_start, const char *obj_end, const char *key, char *out_val, size_t max_len) {
    if (!obj_start || !obj_end || !key || !out_val || max_len == 0) return false;
    const char *k = strstr(obj_start, key);
    if (!k || k >= obj_end) return false;
    const char *colon = strchr(k, ':');
    if (!colon || colon >= obj_end) return false;
    const char *q1 = strchr(colon, '\"');
    if (!q1 || q1 >= obj_end) return false;
    q1++;
    size_t idx = 0;
    while (q1 < obj_end && *q1 != '\"' && idx + 1 < max_len) {
        if (*q1 == '\\' && (q1 + 1) < obj_end) {
            q1++;
            if (*q1 == '\\') out_val[idx++] = '\\';
            else if (*q1 == '\"') out_val[idx++] = '\"';
            else if (*q1 == '/') out_val[idx++] = '/';
            else out_val[idx++] = *q1;
        } else {
            out_val[idx++] = *q1;
        }
        q1++;
    }
    out_val[idx] = '\0';
    return true;
}

static bool extract_json_uint64(const char *obj_start, const char *obj_end, const char *key, uint64_t *out_val) {
    if (!obj_start || !obj_end || !key || !out_val) return false;
    const char *k = strstr(obj_start, key);
    if (!k || k >= obj_end) return false;
    const char *colon = strchr(k, ':');
    if (!colon || colon >= obj_end) return false;
    colon++;
    while (colon < obj_end && (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n')) colon++;
    unsigned long long val = 0;
    if (sscanf(colon, "%llu", &val) == 1) {
        *out_val = (uint64_t)val;
        return true;
    }
    return false;
}

static bool extract_json_int64(const char *obj_start, const char *obj_end, const char *key, int64_t *out_val) {
    if (!obj_start || !obj_end || !key || !out_val) return false;
    const char *k = strstr(obj_start, key);
    if (!k || k >= obj_end) return false;
    const char *colon = strchr(k, ':');
    if (!colon || colon >= obj_end) return false;
    colon++;
    while (colon < obj_end && (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n')) colon++;
    long long val = 0;
    if (sscanf(colon, "%lld", &val) == 1) {
        *out_val = (int64_t)val;
        return true;
    }
    return false;
}

StatusCode quarantine_manifest_load(QuarantineManifest *manifest) {
    if (!manifest) return STATUS_ERROR_INVALID_ARGUMENT;

    FILE *f = fopen(manifest->manifest_filepath, "r");
    if (!f) {
        /* File doesn't exist yet, which is normal on initial clean startup */
        return STATUS_SUCCESS;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fclose(f);
        return STATUS_SUCCESS;
    }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);

    /* Simple JSON array record parser */
    const char *p = buf;
    while ((p = strchr(p, '{')) != NULL) {
        const char *end_obj = strchr(p, '}');
        if (!end_obj) break;

        QuarantineRecord rec;
        memset(&rec, 0, sizeof(QuarantineRecord));

        extract_json_str(p, end_obj, "\"transaction_id\"", rec.transaction_id, sizeof(rec.transaction_id));
        uint64_t gid = 0;
        if (extract_json_uint64(p, end_obj, "\"group_id\"", &gid)) {
            rec.group_id = (uint32_t)gid;
        }
        extract_json_uint64(p, end_obj, "\"file_record_id\"", &rec.file_record_id);
        extract_json_str(p, end_obj, "\"original_path\"", rec.original_path, sizeof(rec.original_path));
        extract_json_str(p, end_obj, "\"quarantine_path\"", rec.quarantine_path, sizeof(rec.quarantine_path));
        extract_json_str(p, end_obj, "\"original_filename\"", rec.original_filename, sizeof(rec.original_filename));
        extract_json_uint64(p, end_obj, "\"file_size\"", &rec.file_size);
        extract_json_str(p, end_obj, "\"sha256_hex\"", rec.sha256_hex, sizeof(rec.sha256_hex));
        extract_json_int64(p, end_obj, "\"quarantine_timestamp\"", &rec.quarantine_timestamp);
        extract_json_int64(p, end_obj, "\"restore_timestamp\"", &rec.restore_timestamp);
        char st_str[32] = {0};
        if (extract_json_str(p, end_obj, "\"status\"", st_str, sizeof(st_str))) {
            rec.status = status_str_to_enum(st_str);
        }

        if (strlen(rec.transaction_id) > 0) {
            /* Add to in-memory list without re-triggering save */
            if (manifest->count >= manifest->capacity) {
                size_t new_cap = manifest->capacity * 2;
                QuarantineRecord *new_records = (QuarantineRecord *)realloc(manifest->records, new_cap * sizeof(QuarantineRecord));
                if (new_records) {
                    manifest->records = new_records;
                    manifest->capacity = new_cap;
                }
            }
            if (manifest->count < manifest->capacity) {
                manifest->records[manifest->count++] = rec;
            }
        }

        p = end_obj + 1;
    }

    free(buf);
    return STATUS_SUCCESS;
}

StatusCode quarantine_manifest_append_journal(const QuarantineManifest *manifest, const QuarantineRecord *rec, const char *action) {
    if (!manifest || !rec || !action) return STATUS_ERROR_INVALID_ARGUMENT;

    FILE *f = fopen(manifest->journal_filepath, "a");
    if (!f) return STATUS_ERROR_FILE_READ_FAILED;

    int64_t now = (int64_t)time(NULL);
    fprintf(f, "%lld | %s | %s | %s | %s | %s\n",
            (long long)now,
            action,
            rec->transaction_id,
            status_enum_to_str(rec->status),
            rec->original_path,
            rec->quarantine_path);

    fflush(f);
    fclose(f);

    return STATUS_SUCCESS;
}

StatusCode quarantine_manifest_reconcile(QuarantineManifest *manifest) {
    if (!manifest) return STATUS_ERROR_INVALID_ARGUMENT;

    bool modified = false;

    for (size_t i = 0; i < manifest->count; i++) {
        QuarantineRecord *r = &manifest->records[i];
        if (r->status != QUARANTINE_STATUS_ACTIVE) continue;

        /* Convert UTF-8 paths to wide for Windows file existence checking */
        wchar_t orig_w[1024];
        wchar_t quar_w[1024];
        MultiByteToWideChar(CP_UTF8, 0, r->original_path, -1, orig_w, 1024);
        MultiByteToWideChar(CP_UTF8, 0, r->quarantine_path, -1, quar_w, 1024);

        DWORD orig_attr = GetFileAttributesW(orig_w);
        DWORD quar_attr = GetFileAttributesW(quar_w);

        bool orig_exists = (orig_attr != INVALID_FILE_ATTRIBUTES);
        bool quar_exists = (quar_attr != INVALID_FILE_ATTRIBUTES);

        if (!quar_exists && orig_exists) {
            /* Move was interrupted before completion: original still there, quarantine absent */
            r->status = QUARANTINE_STATUS_FAILED;
            modified = true;
            quarantine_manifest_append_journal(manifest, r, "RECONCILE_FAILED_INCOMPLETE_MOVE");
        }
    }

    if (modified) {
        quarantine_manifest_save(manifest);
    }

    return STATUS_SUCCESS;
}
