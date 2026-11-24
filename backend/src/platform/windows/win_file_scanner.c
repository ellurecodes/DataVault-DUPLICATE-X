/**
 * @file win_file_scanner.c
 * @brief Windows filesystem traversal using FindFirstFileW and FindNextFileW.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "file_scanner.h"
#include "sha256.h"
#include "status.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define MAX_SCAN_DEPTH 64

/**
 * @brief Convert a wide Unicode string (UTF-16) to a newly heap-allocated UTF-8 string.
 */
static char *wchar_to_utf8(const wchar_t *wstr) {
    if (!wstr) return NULL;
    int bytes_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (bytes_needed <= 0) return NULL;

    char *utf8 = (char *)malloc((size_t)bytes_needed);
    if (!utf8) return NULL;

    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8, bytes_needed, NULL, NULL);
    return utf8;
}

/**
 * @brief Convert Windows FILETIME to Unix epoch timestamp in seconds.
 */
static uint64_t filetime_to_unix_epoch(const FILETIME *ft) {
    ULARGE_INTEGER ull;
    ull.LowPart = ft->dwLowDateTime;
    ull.HighPart = ft->dwHighDateTime;
    /* Intervals of 100ns between Jan 1, 1601 and Jan 1, 1970 */
    const unsigned long long EPOCH_DIFF = 116444736000000000ULL;
    if (ull.QuadPart < EPOCH_DIFF) return 0;
    return (ull.QuadPart - EPOCH_DIFF) / 10000000ULL;
}

/**
 * @brief Extract file extension including the leading dot, or empty string.
 */
static const wchar_t *get_wide_extension(const wchar_t *filename) {
    if (!filename) return L"";
    const wchar_t *dot = wcsrchr(filename, L'.');
    return (dot && dot != filename) ? dot : L"";
}

/**
 * @brief Check if a directory name is a dangerous or protected OS folder.
 */
static bool is_system_or_special_directory(const wchar_t *name) {
    if (!name) return false;
    if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0) return true;
    if (_wcsicmp(name, L"$Recycle.Bin") == 0) return true;
    if (_wcsicmp(name, L"System Volume Information") == 0) return true;
    return false;
}

/**
 * @brief Recursive directory scanner engine using Win32 Unicode APIs.
 */
static StatusCode win_scan_recursive(const wchar_t *current_dir,
                                     const wchar_t *device_id,
                                     FileInventory *inventory,
                                     ScannerStats *stats,
                                     uint32_t depth) {
    if (depth > MAX_SCAN_DEPTH) {
        return STATUS_SUCCESS; /* Prevent stack overflow on excessively deep trees */
    }

    if (stats) {
        stats->directories_scanned++;
    }

    /* Build search pattern: current_dir\* */
    wchar_t search_pattern[MAX_PATH * 2];
    size_t len = wcslen(current_dir);
    if (len == 0) return STATUS_ERROR_INVALID_ARGUMENT;

    if (current_dir[len - 1] == L'\\' || current_dir[len - 1] == L'/') {
        swprintf(search_pattern, sizeof(search_pattern) / sizeof(wchar_t), L"%s*", current_dir);
    } else {
        swprintf(search_pattern, sizeof(search_pattern) / sizeof(wchar_t), L"%s\\*", current_dir);
    }

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search_pattern, &fd);

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (stats) stats->errors_encountered++;

        if (err == ERROR_ACCESS_DENIED) {
            /* Non-fatal: permission denied on this subfolder, continue other folders */
            return STATUS_SUCCESS;
        } else if (err == ERROR_FILE_NOT_FOUND) {
            return STATUS_SUCCESS; /* Empty folder */
        }
        return STATUS_ERROR_PATH_NOT_FOUND;
    }

    do {
        /* Skip current and parent relative markers */
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }

        /* Module 1 Requirement 6: Skip symbolic links, junctions and reparse points */
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            if (stats) stats->reparse_points_skipped++;
            continue;
        }

        /* Build full child path */
        wchar_t child_path[MAX_PATH * 2];
        if (current_dir[len - 1] == L'\\' || current_dir[len - 1] == L'/') {
            swprintf(child_path, sizeof(child_path) / sizeof(wchar_t), L"%s%s", current_dir, fd.cFileName);
        } else {
            swprintf(child_path, sizeof(child_path) / sizeof(wchar_t), L"%s\\%s", current_dir, fd.cFileName);
        }

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* Skip system special folders like $Recycle.Bin */
            if (is_system_or_special_directory(fd.cFileName)) {
                continue;
            }

            /* Recurse into subdirectories */
            win_scan_recursive(child_path, device_id, inventory, stats, depth + 1);
        } else {
            /* Regular file discovered */
            uint64_t file_size = ((uint64_t)fd.nFileSizeHigh << 32) | (uint64_t)fd.nFileSizeLow;
            uint64_t mod_time = filetime_to_unix_epoch(&fd.ftLastWriteTime);

            FileRecord rec;
            memset(&rec, 0, sizeof(FileRecord));
            rec.filename = wchar_to_utf8(fd.cFileName);
            rec.filepath = wchar_to_utf8(child_path);
            rec.extension = wchar_to_utf8(get_wide_extension(fd.cFileName));
            rec.device_id = wchar_to_utf8(device_id);
            rec.file_size = file_size;
            rec.modified_time = mod_time;

            /* Calculate SHA-256 fingerprint with concurrency safety checks */
            sha256_calculate_file(child_path, rec.sha256_binary, rec.sha256_hex, &rec.hash_status);

            file_inventory_add(inventory, &rec);
            file_record_cleanup(&rec);

            if (stats) {
                stats->files_scanned++;
                stats->total_bytes_scanned += file_size;
            }
        }

    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return STATUS_SUCCESS;
}

StatusCode scan_directory(const wchar_t *root_path, FileInventory *inventory, ScannerStats *stats) {
    if (!root_path || !inventory) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    /* Verify directory exists and is accessible */
    DWORD attr = GetFileAttributesW(root_path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        if (stats) stats->errors_encountered++;
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            return STATUS_ERROR_PATH_NOT_FOUND;
        } else if (err == ERROR_ACCESS_DENIED) {
            return STATUS_ERROR_ACCESS_DENIED;
        }
        return STATUS_ERROR_PATH_NOT_FOUND;
    }

    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return STATUS_ERROR_INVALID_ARGUMENT; /* Target is not a directory */
    }

    /* Extract device/root identifier (e.g. drive letter or folder name) */
    wchar_t device_id[64] = {0};
    if (root_path[1] == L':') {
        wcsncpy(device_id, root_path, 2);
        device_id[2] = L'\0';
    } else {
        wcsncpy(device_id, root_path, 63);
    }

    return win_scan_recursive(root_path, device_id, inventory, stats, 0);
}

