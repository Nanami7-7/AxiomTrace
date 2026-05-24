#include "axiomtrace.h"

void emit_a(void) {
    AX_EVT(INFO, 0x10u, 0x0001u);
}
