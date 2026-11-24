/**
 * @file duplicate_detector.h
 * @brief Duplicate detection engine integrating Hash Table and byte-by-byte verification.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_DUPLICATE_DETECTOR_H
#define DATAVAULT_DUPLICATE_DETECTOR_H

#include "file_record.h"
#include "duplicate_group.h"
#include "status.h"

/**
 * @brief Run the duplicate detection algorithm on the scanned FileInventory.
 * 
 * 1. Filters successfully hashed files.
 * 2. Uses (file_size, sha256) to look up or insert into the custom Hash Table.
 * 3. On candidate collision, performs byte-by-byte verification before grouping.
 * 4. Merges into DuplicateGroupList only if verified copies >= 2.
 * 5. Calculates exact recoverable storage: (copies - 1) * file_size.
 * 
 * @param inventory Pointer to scanned FileInventory.
 * @param out_groups Pointer to DuplicateGroupList receiving verified duplicate clusters.
 * @return STATUS_SUCCESS or specific error code.
 */
StatusCode duplicate_detector_process(const FileInventory *inventory,
                                      DuplicateGroupList *out_groups);

/**
 * @brief Format a byte count into a human-readable string (B, KB, MB, GB).
 * @param bytes Number of bytes.
 * @param buffer Output buffer.
 * @param buf_size Size of output buffer.
 */
void duplicate_detector_format_size(uint64_t bytes, char *buffer, size_t buf_size);

/**
 * @brief Print the formatted duplicate detection report matching DataVault requirements.
 * @param groups Pointer to populated DuplicateGroupList.
 * @param total_scanned Total files scanned.
 * @param total_hashed Total successfully hashed files.
 */
void duplicate_report_print(const DuplicateGroupList *groups,
                            size_t total_scanned,
                            size_t total_hashed);

/**
 * @brief Display the Top K largest storage-wasting duplicate groups using Binary Min Heap.
 * 
 * @param groups Pointer to DuplicateGroupList.
 * @param k Number of top groups to display (e.g. 5).
 */
void duplicate_report_print_top_k(const DuplicateGroupList *groups, size_t k);

#endif /* DATAVAULT_DUPLICATE_DETECTOR_H */

