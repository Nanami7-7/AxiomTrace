#ifndef AXIOM_CAPSULE_H
#define AXIOM_CAPSULE_H

#include <stdbool.h>
#include <stdint.h>

#include "axiom_config.h"
#include "axiom_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize volatile capsule capture state. Called by axiom_init(). */
void axiom_capsule_init(void);

/* Observe a complete wire frame after axiom_write() has assembled it. */
void axiom_capsule_observe_frame(const uint8_t *frame, uint16_t len, axiom_level_t level);

/* Add dropped-event statistics to the next capsule header. */
void axiom_capsule_record_drops(uint32_t lost);

/* Commit the captured capsule to the port flash backend. */
bool axiom_capsule_commit(void);

/* True when a valid RAM or flash capsule is available. */
bool axiom_capsule_present(void);

/* Read a complete valid capsule into out. Returns bytes copied, or 0 on error. */
uint32_t axiom_capsule_read(uint8_t *out, uint32_t max_len);

/* Clear volatile capture state and erase the configured flash capsule region. */
void axiom_capsule_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_CAPSULE_H */
