#include "axiomtrace.h"

#include <stdint.h>

void single_header_emit_a(void);
void single_header_emit_b(void);

int main(void) {
    uint8_t storage[128];
    axiom_memory_backend_ctx_t context;
    axiom_backend_t backend = axiom_backend_memory("single", storage, sizeof(storage),
                                                   &context);
    axiom_init();
    if (axiom_backend_register(&backend) != AXIOM_BACKEND_OK) {
        return 1;
    }
    single_header_emit_a();
    single_header_emit_b();
    axiom_flush();
    return context.head >= 24u && storage[0] == AXIOM_SYNC_BYTE ? 0 : 2;
}
