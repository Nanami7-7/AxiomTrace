#include "axiom_backend.h"
#include "axiom_port.h"
#include <stdbool.h>

/* ---------------------------------------------------------------------------
 * Backend Registry & Degradation State
 * --------------------------------------------------------------------------- */
static const axiom_backend_t *s_backends[AXIOM_BACKEND_MAX];
static uint8_t s_backend_count = 0;

#if AXIOM_BACKEND_DEGRADATION
/* Per-backend degradation tracking.
 * disabled=true means the backend has exceeded AXIOM_BACKEND_FAIL_THRESHOLD
 * and will be skipped until AXIOM_BACKEND_RECOVERY_US has elapsed. */
static bool     s_be_disabled[AXIOM_BACKEND_MAX];
static uint32_t s_be_error_count[AXIOM_BACKEND_MAX];
static uint32_t s_be_disabled_at[AXIOM_BACKEND_MAX];
#endif

int axiom_backend_register(const axiom_backend_t *backend) {
    if (!backend || !backend->write) {
        return AXIOM_BACKEND_ERR_NULL;
    }
    if (s_backend_count >= AXIOM_BACKEND_MAX) {
        return AXIOM_BACKEND_ERR_FULL;
    }
    /* Backward compatibility: old backends may be zero-initialized with size==0.
     * Treat size==0 as legacy layout (v1 without the size field).
     * size > 0 but < current sizeof means a newer library compiled against
     * an older struct — reject to prevent reading garbage. */
    if (backend->size != 0 && backend->size < sizeof(axiom_backend_t)) {
        return AXIOM_BACKEND_ERR_STRUCT;
    }

#if AXIOM_BACKEND_DEGRADATION
    s_be_disabled[s_backend_count] = false;
    s_be_error_count[s_backend_count] = 0;
    s_be_disabled_at[s_backend_count] = 0;
#endif

    s_backends[s_backend_count++] = backend;
    return 0;
}

void axiom_backend_dispatch(const uint8_t *frame, uint16_t len) {
    for (uint8_t i = 0; i < s_backend_count; ++i) {
        const axiom_backend_t *be = s_backends[i];

#if AXIOM_BACKEND_DEGRADATION
        /* Auto-recovery: if recovery time has elapsed, re-enable backend.
         * Handles 32-bit timestamp overflow via unsigned subtraction:
         * (now - disabled_at) correctly wraps on overflow. */
        if (s_be_disabled[i]) {
            uint32_t now = axiom_port_timestamp();
            uint32_t elapsed = now - s_be_disabled_at[i];
            if (elapsed >= AXIOM_BACKEND_RECOVERY_US) {
                s_be_disabled[i] = false;
                s_be_error_count[i] = 0;
            } else {
                continue; /* still in recovery window — skip */
            }
        }
#endif

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

#if AXIOM_BACKEND_DEGRADATION
        if (ret < 0) {
            s_be_error_count[i]++;
            if (s_be_error_count[i] >= AXIOM_BACKEND_FAIL_THRESHOLD) {
                s_be_disabled[i] = true;
                s_be_disabled_at[i] = axiom_port_timestamp();
            }
        } else {
            s_be_error_count[i] = 0;
        }
#endif
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
    /* Panic bypasses degradation — always attempt all backends.
     * A backend that is broken in normal mode may still work for
     * panic output (e.g., SWO may be functional while UART is not). */
    for (uint8_t i = 0; i < s_backend_count; ++i) {
        const axiom_backend_t *be = s_backends[i];
        if (be->panic_write) {
            (void)be->panic_write(frame, len, be->ctx);
        } else if (be->write) {
            (void)be->write(frame, len, be->ctx);
        }
    }
}

uint8_t axiom_backend_active_count(void) {
    uint8_t active = 0;
    for (uint8_t i = 0; i < s_backend_count; ++i) {
#if AXIOM_BACKEND_DEGRADATION
        if (!s_be_disabled[i]) {
            active++;
        }
#else
        active++;
#endif
    }
    return active;
}

uint8_t axiom_backend_count(void) {
    return s_backend_count;
}
