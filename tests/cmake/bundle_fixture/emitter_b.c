#include "axiomtrace.h"

void emit_b(void) {
    AX_EVT(INFO, 0x10u, 0x0002u);
}
