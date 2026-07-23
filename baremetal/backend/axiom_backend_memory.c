#include "axiom_backend.h"
#include "axiom_ring.h"
#include <string.h>

/* Memory Backend: writes into a secondary RAM buffer for host inspection.
 * This is a simple backend template that copies frames to a user-provided
 * linear buffer. It does not use the Core ring buffer directly. */

static int memory_write(const uint8_t *data, uint16_t len, void *ctx) {
    axiom_memory_backend_ctx_t *m = (axiom_memory_backend_ctx_t *)ctx;
    if (!m || !m->buf || len > m->size) return -1;
    if (m->head + len > m->size) m->head = 0; /* wrap */
    memcpy(m->buf + m->head, data, len);
    m->head += len;
    return 0;
}

static int memory_ready(void *ctx) {
    (void)ctx;
    return 1; /* always ready */
}

axiom_backend_t axiom_backend_memory(const char *name, uint8_t *buf, uint32_t size,
                                     axiom_memory_backend_ctx_t *ctx) {
    if (!ctx || !buf || size == 0u) {
        return (axiom_backend_t){0};
    }
    ctx->buf = buf;
    ctx->size = size;
    ctx->head = 0;
    return (axiom_backend_t)AXIOM_BACKEND_INIT(
        .name = name ? name : "memory",
        .caps = AXIOM_BACKEND_CAP_MEMORY,
        .write = memory_write,
        .ready = memory_ready,
        .ctx = ctx,
    );
}
