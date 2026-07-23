#include "axiom_diagnostics.h"

#include "axiom_port.h"

#include <string.h>

static axiom_diagnostics_t s_diagnostics;

static void diagnostics_add(uint32_t *counter, uint32_t count) {
    axiom_port_critical_enter();
    *counter += count;
    axiom_port_critical_exit();
}

void axiom_diagnostics_get(axiom_diagnostics_t *out) {
    if (!out) {
        return;
    }
    axiom_port_critical_enter();
    *out = s_diagnostics;
    axiom_port_critical_exit();
}

void axiom_diagnostics_reset(void) {
    axiom_port_critical_enter();
    memset(&s_diagnostics, 0, sizeof(s_diagnostics));
    axiom_port_critical_exit();
}

void axiom_diagnostics_note_filtered(uint32_t count) {
    diagnostics_add(&s_diagnostics.filtered, count);
}

void axiom_diagnostics_note_ring_full(uint32_t count) {
    diagnostics_add(&s_diagnostics.ring_full, count);
}

void axiom_diagnostics_note_encode_overflow(uint32_t count) {
    diagnostics_add(&s_diagnostics.encode_overflow, count);
}

void axiom_diagnostics_note_invalid_input(uint32_t count) {
    diagnostics_add(&s_diagnostics.invalid_input, count);
}

void axiom_diagnostics_note_backend(uint32_t count) {
    diagnostics_add(&s_diagnostics.backend, count);
}
