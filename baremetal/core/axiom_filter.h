#ifndef AXIOM_FILTER_H
#define AXIOM_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include "axiom_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Runtime level/module filter and drop statistics
 * --------------------------------------------------------------------------- */

typedef struct {
    volatile uint32_t level_mask;   /* May be read in ISR context */
    volatile uint32_t module_mask;  /* May be read in ISR context */
    uint32_t drop_count;        /* Total dropped events */
    uint8_t  drop_module;       /* Module ID of last dropped event */
    uint16_t drop_event;        /* Event ID of last dropped event */
    bool     drop_pending;      /* True if drops occurred since last summary */
} axiom_filter_t;

/* Initialize filter (all levels/modules enabled, counters zeroed) */
void axiom_filter_init(axiom_filter_t *filter);

/* Check if event passes filter. Returns true if should emit. */
bool axiom_filter_check(axiom_filter_t *filter, axiom_level_t level, uint8_t module_id);

/* Record a drop. Call when event is discarded. */
void axiom_filter_drop(axiom_filter_t *filter, uint8_t module_id, uint16_t event_id);

/* Check and clear drop_pending flag. Returns true if a DROP_SUMMARY should be emitted.
 * Note: Drop summary is currently handled internally by axiom_write().
 * These functions are provided for external consumers (e.g., custom backends,
 * test harnesses) that need direct access to drop statistics. */
bool axiom_filter_drop_summary_ready(axiom_filter_t *filter);

/* Get drop counter and reset.
 * Provided for external consumers — see note above. */
uint32_t axiom_filter_drop_count_get_and_clear(axiom_filter_t *filter);

/* Runtime filter control — implementations are in axiom_event.c where s_filter is defined */
void axiom_level_mask_set(uint32_t mask);
uint32_t axiom_level_mask_get(void);
void axiom_module_mask_set(uint32_t mask);
uint32_t axiom_module_mask_get(void);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_FILTER_H */
