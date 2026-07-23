/* ============================================================================
 * AxiomTrace Minimal Example
 * ============================================================================
 * Smallest complete path: backend registration, event, flush, valid frame.
 *
 * Build with CMake (recommended):
 *   cmake -B build -DAXIOM_PLATFORM=host
 *   cmake --build build --target example_minimal
 *
 * Build with Make (from baremetal directory):
 *   make example_minimal
 * ============================================================================ */

#include "axiomtrace.h"
#include <stdio.h>

int main(void) {
    uint8_t trace[128] = {0};
    axiom_memory_backend_ctx_t memory_ctx;
    axiom_backend_t memory = axiom_backend_memory("quickstart", trace, sizeof(trace),
                                                  &memory_ctx);
    axiom_init();
    if (axiom_backend_register(&memory) != AXIOM_BACKEND_OK) {
        return 1;
    }

    /* Structured event: module=0x03, event=0x01, payload=one uint16_t */
    AX_EVT(INFO, 0x03, 0x01, (uint16_t)3200);
    axiom_flush();

    if (memory_ctx.head < 12u || trace[0] != AXIOM_SYNC_BYTE) {
        return 2;
    }
    for (uint32_t i = 0; i < memory_ctx.head; ++i) {
        printf("%02X", trace[i]);
    }
    printf("\n");
    return 0;
}
