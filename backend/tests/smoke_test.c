/**
 * @file smoke_test.c
 * @brief Smoke test verifying foundational C11 initialization and banner rendering.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(void) {
    /* Test 1: Banner output */
    engine_display_banner();

    /* Test 2: Engine initialization */
    EngineState engine;
    StatusCode status = engine_init(&engine);
    assert(status == STATUS_SUCCESS);
    assert(engine.is_initialized == true);
    assert(engine.total_files_scanned == 0);
    assert(engine.total_duplicate_groups == 0);
    assert(engine.total_recoverable_bytes == 0);

    /* Test 3: Engine shutdown */
    status = engine_shutdown(&engine);
    assert(status == STATUS_SUCCESS);
    assert(engine.is_initialized == false);

    return EXIT_SUCCESS;
}

