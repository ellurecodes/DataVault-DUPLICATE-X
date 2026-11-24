/**
 * @file engine.c
 * @brief Implementation of the DUPLICATE-X core engine lifecycle routines.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "engine.h"
#include "config.h"
#include <stdio.h>

void engine_display_banner(void) {
    printf("=================================\n");
    printf("        %s\n", PROJECT_NAME);
    printf("       Powered by %s\n", TEAM_NAME);
    printf("=================================\n\n");
    printf("Intelligent Multi-Device File\n");
    printf("Deduplication and Storage System\n\n");
}

StatusCode engine_init(EngineState *state) {
    if (state == NULL) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    state->is_initialized = true;
    state->total_files_scanned = 0;
    state->total_duplicate_groups = 0;
    state->total_recoverable_bytes = 0;

    printf("Backend initialized successfully.\n");
    return STATUS_SUCCESS;
}

StatusCode engine_shutdown(EngineState *state) {
    if (state == NULL || !state->is_initialized) {
        return STATUS_ERROR_NOT_INITIALIZED;
    }

    state->is_initialized = false;
    return STATUS_SUCCESS;
}

