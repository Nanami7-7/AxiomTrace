#include "axiomtrace.h"

void single_header_emit_a(void) {
    AX_EVT(INFO, 1u, 1u, (uint16_t)42u);
}
