/**
 * @file file_scanner.h
 * @brief Recursive directory scanner interface supporting Unicode and multiple roots.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_FILE_SCANNER_H
#define DATAVAULT_FILE_SCANNER_H

#include "file_record.h"
#include "status.h"
#include <wchar.h>

/**
 * @struct ScannerStats
 * @brief Real-time telemetry recorded during scanning.
 */
typedef struct {
    size_t directories_scanned;
    size_t files_scanned;
    size_t reparse_points_skipped;
    size_t errors_encountered;
    uint64_t total_bytes_scanned;
} ScannerStats;

/**
 * @brief Initialize scanner statistics counter.
 * @param stats Pointer to ScannerStats.
 */
void scanner_stats_init(ScannerStats *stats);

/**
 * @brief Scan a single directory recursively and register files in the inventory.
 * @param root_path Unicode path to the target folder.
 * @param inventory Pointer to destination FileInventory.
 * @param stats Optional pointer to track statistics (can be NULL).
 * @return STATUS_SUCCESS or specific error code.
 */
StatusCode scan_directory(const wchar_t *root_path, FileInventory *inventory, ScannerStats *stats);

/**
 * @brief Scan multiple user-selected directories in a single unified operation.
 * @param root_paths Array of Unicode directory paths.
 * @param num_paths Number of paths in the array.
 * @param inventory Pointer to destination FileInventory.
 * @param stats Optional pointer to track statistics (can be NULL).
 * @return STATUS_SUCCESS or specific error code.
 */
StatusCode scan_multiple_directories(const wchar_t **root_paths, size_t num_paths,
                                     FileInventory *inventory, ScannerStats *stats);

#endif /* DATAVAULT_FILE_SCANNER_H */

