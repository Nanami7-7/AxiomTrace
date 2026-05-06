#include "axiom_filter.h"

void axiom_filter_init(axiom_filter_t *filter) {
    filter->level_mask  = 0xFFFFFFFFu;
    filter->module_mask = 0xFFFFFFFFu;
    filter->drop_count  = 0;
    filter->drop_module = 0;
    filter->drop_event  = 0;
    filter->drop_pending = false;
}

bool axiom_filter_check(axiom_filter_t *filter, axiom_level_t level, uint8_t module_id) {
    if (level >= AXIOM_LEVEL_MAX) return false;
    if ((filter->level_mask & (1u << (uint32_t)level)) == 0) return false;
    /* 32-bit mask limits per-module filtering to module_id 0..(AXIOM_MODULE_MAX-1).
     * Modules with id >= AXIOM_MODULE_MAX always pass the filter (no masking applied). */
    if (module_id < AXIOM_MODULE_MAX && (filter->module_mask & (1u << (uint32_t)module_id)) == 0) return false;
    return true;
}

void axiom_filter_drop(axiom_filter_t *filter, uint8_t module_id, uint16_t event_id) {
    filter->drop_count++;
    filter->drop_module = module_id;
    filter->drop_event  = event_id;
    filter->drop_pending = true;
}

bool axiom_filter_drop_summary_ready(axiom_filter_t *filter) {
    return filter->drop_pending;
}

uint32_t axiom_filter_drop_count_get_and_clear(axiom_filter_t *filter) {
    uint32_t count = filter->drop_count;
    filter->drop_count = 0;
    filter->drop_pending = false;
    filter->drop_module = 0;
    filter->drop_event  = 0;
    return count;
}
