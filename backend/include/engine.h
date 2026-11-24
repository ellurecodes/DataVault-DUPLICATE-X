/**
 * @file engine.h
 * @brief Core engine initialization, lifecycle, and banner reporting.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_ENGINE_H
#define DATAVAULT_ENGINE_H

#include "status.h"
#include <stdbool.h>

/**
 * @struct EngineState
 * @brief High-level tracking state for the DUPLICATE-X core engine.
 */
typedef struct {
    bool is_initialized;
    unsigned long total_files_scanned;
    unsigned long total_duplicate_groups;
    unsigned long long total_recoverable_bytes;
} EngineState;

/**
 * @brief Display the official DUPLICATE-X and DataVault startup banner.
 */
void engine_display_banner(void);

/**
 * @brief Initialize the DUPLICATE-X backend engine and internal state.
 * @param state Pointer to the EngineState structure to initialize.
 * @return STATUS_SUCCESS on successful initialization, or appropriate error code.
 */
StatusCode engine_init(EngineState *state);

/**
 * @brief Clean up and shut down the engine subsystems safely.
 * @param state Pointer to the active EngineState structure.
 * @return STATUS_SUCCESS on successful shutdown.
 */
StatusCode engine_shutdown(EngineState *state);

#endif /* DATAVAULT_ENGINE_H */

