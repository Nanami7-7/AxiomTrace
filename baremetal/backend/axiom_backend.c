#include "axiom_backend.h"
#include "axiom_port.h"

static const axiom_backend_t *s_backends[AXIOM_BACKEND_MAX];
static uint8_t s_backend_count = 0;

int axiom_backend_register(const axiom_backend_t *backend) {
    if (!backend || !backend->write) {
        return -1;
    }
    if (s_backend_count >= AXIOM_BACKEND_MAX) {
        return -2;
    }
    /* Backward compatibility: old backends may be zero-initialized with size==0.
     * Treat size==0 as legacy layout (v1 without the size field).
     * size > 0 but < current sizeof means a newer library compiled against
     * an older struct — reject to prevent reading garbage. */
    if (backend->size != 0 && backend->size < sizeof(axiom_backend_t)) {
        return -3; /* struct too small — recompile the backend */
    }
    s_backends[s_backend_count++] = backend;
    return 0;
}

void axiom_backend_dispatch(const uint8_t *frame, uint16_t len) {
    for (uint8_t i = 0; i < s_backend_count; ++i) {
        const axiom_backend_t *be = s_backends[i];
        if (be->ready && !be->ready(be->ctx)) {
            if (be->on_drop) {
                be->on_drop(1, be->ctx);
            }
            continue;
        }
        int ret = be->write(frame, len, be->ctx);
        if (ret < 0 && be->on_drop) {
            be->on_drop(1, be->ctx);
        }
    }
}

void axiom_backend_flush_all(void) {
    for (uint8_t i = 0; i < s_backend_count; ++i) {
        const axiom_backend_t *be = s_backends[i];
        if (be->flush) {
            (void)be->flush(be->ctx);
        }
    }
}

void axiom_backend_panic_write(const uint8_t *frame, uint16_t len) {
    for (uint8_t i = 0; i < s_backend_count; ++i) {
        const axiom_backend_t *be = s_backends[i];
        if (be->panic_write) {
            (void)be->panic_write(frame, len, be->ctx);
        } else if (be->write) {
            (void)be->write(frame, len, be->ctx);
        }
    }
}
