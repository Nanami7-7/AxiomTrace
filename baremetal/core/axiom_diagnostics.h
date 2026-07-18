#ifndef AXIOM_DIAGNOSTICS_H
#define AXIOM_DIAGNOSTICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t filtered;
    uint32_t ring_full;
    uint32_t encode_overflow;
    uint32_t invalid_input;
    uint32_t backend;
} axiom_diagnostics_t;

void axiom_diagnostics_get(axiom_diagnostics_t *out);
void axiom_diagnostics_reset(void);

/* Internal accounting hooks shared by the core, frontend, and backends. */
void axiom_diagnostics_note_filtered(uint32_t count);
void axiom_diagnostics_note_ring_full(uint32_t count);
void axiom_diagnostics_note_encode_overflow(uint32_t count);
void axiom_diagnostics_note_invalid_input(uint32_t count);
void axiom_diagnostics_note_backend(uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_DIAGNOSTICS_H */
