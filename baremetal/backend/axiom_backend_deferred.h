#ifndef AXIOM_BACKEND_DEFERRED_H
#define AXIOM_BACKEND_DEFERRED_H

#include "axiom_backend.h"
#include "axiom_ring.h"
#include "axiom_event.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Deferred Ring Configuration
 * --------------------------------------------------------------------------- */

/* Default deferred ring size — user-overridable at build time */
#ifndef AXIOM_DEFERRED_RING_SIZE
#define AXIOM_DEFERRED_RING_SIZE 256
#endif

/* ---------------------------------------------------------------------------
 * Deferred Ring Context
 * Independent ring, isolated from the main ring, statically allocated.
 * --------------------------------------------------------------------------- */
typedef struct {
    axiom_ring_t ring;
    uint8_t buf[AXIOM_DEFERRED_RING_SIZE];
} axiom_deferred_ring_ctx_t;

/* ---------------------------------------------------------------------------
 * Deferred Backend Context
 * Cascades to a downstream (actual) backend; hot-path write() is non-blocking.
 * --------------------------------------------------------------------------- */
typedef struct {
    axiom_backend_t backend;           /* Registrable backend instance            */
    const axiom_backend_t *downstream; /* Downstream actual backend               */
    axiom_deferred_ring_ctx_t deferred_ring;
} axiom_backend_deferred_ctx_t;

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

/**
 * @brief Initialize a deferred backend.
 *
 * @param ctx          Deferred backend context (caller-allocated, static storage).
 * @param deferred_ring_ctx  Deferred ring context (caller-allocated, kept for
 *                           API compatibility — actual ring is embedded in ctx).
 * @param downstream   Downstream actual backend (deferred -> actual).
 * @return int  0 on success, negative on error.
 *
 * @note After registration, hot-path write() only copies data into the local
 *       ring and returns immediately — no blocking.
 *       Flush (axiom_backend_flush_all) cascades buffered frames to the
 *       downstream backend at a later, non-ISR context.
 * @note No dynamic memory allocation; all buffers are statically allocated.
 */
int axiom_backend_deferred_init(axiom_backend_deferred_ctx_t *ctx,
                                 axiom_deferred_ring_ctx_t *deferred_ring_ctx,
                                 const axiom_backend_t *downstream);

/**
 * @brief Get the backend instance for registration into the registry.
 *
 * @param ctx  Deferred backend context.
 * @return const axiom_backend_t*  Backend instance (valid after init).
 */
const axiom_backend_t *axiom_backend_deferred_get_backend(axiom_backend_deferred_ctx_t *ctx);

/**
 * @brief Query the number of bytes currently buffered in the deferred ring.
 *
 * @param ctx  Deferred backend context.
 * @return uint32_t  Buffered byte count.
 */
uint32_t axiom_backend_deferred_pending(const axiom_backend_deferred_ctx_t *ctx);

/**
 * @brief Reset the deferred ring, discarding all buffered data.
 *
 * @param ctx  Deferred backend context.
 */
void axiom_backend_deferred_reset(axiom_backend_deferred_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_BACKEND_DEFERRED_H */