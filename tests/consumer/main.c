#include "axiomtrace.h"

int main(void) {
    axiom_init();
    AX_EVT(INFO, 1u, 1u, (uint8_t)1u);
    axiom_flush();
    return 0;
}
