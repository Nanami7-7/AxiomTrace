#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "axiom_filter.h"

static int failures = 0;

#define CHECK(msg, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        failures++; \
    } \
} while(0)

int main(void) {
    axiom_filter_t f;
    axiom_filter_init(&f);

    /* ---- Test 1: Init — all levels pass ---- */
    CHECK("init: DEBUG passes", axiom_filter_check(&f, AXIOM_LEVEL_DEBUG, 0) == true);
    CHECK("init: INFO passes",  axiom_filter_check(&f, AXIOM_LEVEL_INFO,  0) == true);
    CHECK("init: WARN passes",  axiom_filter_check(&f, AXIOM_LEVEL_WARN,  0) == true);
    CHECK("init: ERROR passes", axiom_filter_check(&f, AXIOM_LEVEL_ERROR, 0) == true);
    CHECK("init: FAULT passes", axiom_filter_check(&f, AXIOM_LEVEL_FAULT, 0) == true);

    /* ---- Test 2: Mask out DEBUG ---- */
    f.level_mask = 0xFFFFFFFEu;
    CHECK("mask: DEBUG blocked", axiom_filter_check(&f, AXIOM_LEVEL_DEBUG, 0) == false);
    CHECK("mask: INFO passes",   axiom_filter_check(&f, AXIOM_LEVEL_INFO,  0) == true);

    /* ---- Test 3: Mask out INFO and above ---- */
    f.level_mask = 0xFFFFFFFCu;
    CHECK("mask2: DEBUG blocked", axiom_filter_check(&f, AXIOM_LEVEL_DEBUG, 0) == false);
    CHECK("mask2: INFO blocked",  axiom_filter_check(&f, AXIOM_LEVEL_INFO,  0) == false);
    CHECK("mask2: WARN passes",   axiom_filter_check(&f, AXIOM_LEVEL_WARN,  0) == true);

    /* ---- Test 4: Module mask ---- */
    f.level_mask = 0xFFFFFFFFu;
    f.module_mask = 0u;  /* reject all modules */
    CHECK("module_mask: module 0 blocked", axiom_filter_check(&f, AXIOM_LEVEL_INFO, 0) == false);
    CHECK("module_mask: module 1 blocked", axiom_filter_check(&f, AXIOM_LEVEL_INFO, 1) == false);

    f.module_mask = 0x03u;  /* only modules 0 and 1 pass */
    CHECK("module_mask: module 0 passes", axiom_filter_check(&f, AXIOM_LEVEL_INFO, 0) == true);
    CHECK("module_mask: module 1 passes", axiom_filter_check(&f, AXIOM_LEVEL_INFO, 1) == true);
    CHECK("module_mask: module 2 blocked", axiom_filter_check(&f, AXIOM_LEVEL_INFO, 2) == false);
    CHECK("module_mask: module 7 blocked", axiom_filter_check(&f, AXIOM_LEVEL_INFO, 7) == false);

    /* ---- Test 5: Drop recording ---- */
    f.level_mask = 0xFFFFFFFFu;
    f.module_mask = 0xFFFFFFFFu;
    axiom_filter_init(&f);

    axiom_filter_drop(&f, 0x03, 0x0500);
    CHECK("drop: count == 1", f.drop_count == 1);
    CHECK("drop: pending == true", f.drop_pending == true);
    CHECK("drop: module == 0x03", f.drop_module == 0x03);
    CHECK("drop: event == 0x0500", f.drop_event == 0x0500);

    /* ---- Test 6: Drop summary ready ---- */
    CHECK("summary_ready: true after drop", axiom_filter_drop_summary_ready(&f) == true);
    /* drop_summary_ready only reads pending, does NOT clear it.
     * Only drop_count_get_and_clear clears the pending flag. */
    CHECK("summary_ready: still true (read-only)", axiom_filter_drop_summary_ready(&f) == true);
    /* Now clear via get_and_clear */
    (void)axiom_filter_drop_count_get_and_clear(&f);
    CHECK("summary_ready: false after get_clear", axiom_filter_drop_summary_ready(&f) == false);

    /* ---- Test 7: Drop count get-and-clear ---- */
    f.level_mask = 0xFFFFFFFFu;
    f.module_mask = 0xFFFFFFFFu;
    axiom_filter_init(&f);
    axiom_filter_drop(&f, 0x01, 0x0001);
    axiom_filter_drop(&f, 0x02, 0x0002);
    uint32_t lost = axiom_filter_drop_count_get_and_clear(&f);
    CHECK("get_clear: count == 2", lost == 2);
    CHECK("get_clear: count zeroed", f.drop_count == 0);
    CHECK("get_clear: pending cleared", f.drop_pending == false);

    /* ---- Test 8: Level mask set/get (runtime API) ---- */
    axiom_level_mask_set(0x0Fu);
    CHECK("level_mask_get: matches set", axiom_level_mask_get() == 0x0Fu);
    axiom_level_mask_set(0xFFFFFFFFu);
    CHECK("level_mask_get: restore all", axiom_level_mask_get() == 0xFFFFFFFFu);

    /* ---- Test 9: Module mask set/get (runtime API) ---- */
    axiom_module_mask_set(0xAAu);
    CHECK("module_mask_get: matches set", axiom_module_mask_get() == 0xAAu);
    axiom_module_mask_set(0xFFFFFFFFu);
    CHECK("module_mask_get: restore all", axiom_module_mask_get() == 0xFFFFFFFFu);

    if (failures == 0) {
        printf("test_filter: PASSED (9 test groups)\n");
    } else {
        printf("test_filter: FAILED (%d failures)\n", failures);
    }
    return failures;
}
