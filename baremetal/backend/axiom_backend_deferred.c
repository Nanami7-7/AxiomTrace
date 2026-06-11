#include "axiom_backend_deferred.h"
#include "axiom_timestamp.h"
#include "axiom_port.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal: deferred ring 操作
 * --------------------------------------------------------------------------- */

static int deferred_write(const uint8_t *data, uint16_t len, void *ctx) {
    axiom_backend_deferred_ctx_t *d = (axiom_backend_deferred_ctx_t *)ctx;
    if (!d || !data || len == 0) {
        return -1;
    }

    /*
     * 写入 deferred ring，使用 DROP 策略。
     * 热路径不阻塞，如果 ring 满则丢弃最老的数据。
     * axiom_ring_write 已使用临界区保护，ISR 安全。
     */
    bool ok = axiom_ring_write(&d->deferred_ring.ring, data, len);
    return ok ? 0 : -2;
}

static int deferred_ready(void *ctx) {
    axiom_backend_deferred_ctx_t *d = (axiom_backend_deferred_ctx_t *)ctx;
    if (!d || !d->downstream) {
        return 0;
    }

    /*
     * 如果下游 backend 提供了 ready 回调，则检查其就绪状态。
     * 否则默认认为就绪。
     */
    if (d->downstream->ready) {
        return d->downstream->ready(d->downstream->ctx);
    }
    return 1;
}

static int deferred_flush(void *ctx) {
    axiom_backend_deferred_ctx_t *d = (axiom_backend_deferred_ctx_t *)ctx;
    if (!d || !d->downstream) {
        return -1;
    }

    /*
     * 将 deferred ring 中的数据级联到下游 backend。
     * 使用逐帧 peek → read → dispatch 方式，保证下游 backend 的帧边界。
     * 即使下游 write() 失败（busy），也继续处理下一帧，避免死锁。
     *
     * @note 此函数应在非 ISR 上下文中调用（通常在 main loop 或
     *       专门的后台任务中调用 axiom_backend_flush_all）。
     *       ISR 上下文中只应触发 write 操作。
     */
    axiom_ring_t *ring = &d->deferred_ring.ring;
    const axiom_backend_t *downstream = d->downstream;
    uint8_t frame_buf[AXIOM_MAX_FRAME_LEN];
    int dispatched = 0;

    while (axiom_ring_used(ring) > 0) {
        uint16_t avail = axiom_ring_peek(ring, frame_buf, sizeof(frame_buf));
        if (avail < 12) break; /* minimum: 8 header + 1 ts + 1 len + 0 payload + 2 crc */
        /* Decode variable-length timestamp to locate payload_len */
        uint8_t ts_len = axiom_timestamp_decode_len(frame_buf[8]);
        uint16_t payload_len = frame_buf[8 + ts_len];
        uint16_t frame_len = (uint16_t)(8u + ts_len + 1u + payload_len + 2u);
        if (avail < frame_len) break;
        /* Forward BEFORE consuming from ring, so a failed write does not lose data */
        if (downstream->write) {
            int rc = downstream->write(frame_buf, frame_len, downstream->ctx);
            if (rc < 0) {
                break; /* downstream busy or error — stop and retry next flush */
            }
        }
        /* Data already copied by peek — just advance tail pointer */
        axiom_ring_consume(ring, frame_len);
        dispatched++;
    }
    return dispatched;
}

static int deferred_panic_write(const uint8_t *data, uint16_t len, void *ctx) {
    axiom_backend_deferred_ctx_t *d = (axiom_backend_deferred_ctx_t *)ctx;
    if (!d || !d->downstream || !data) {
        return -1;
    }

    /*
     * Panic 模式：绕过 deferred ring，直接透传到下游。
     * 如果下游有 panic_write 则调用之，否则使用普通 write。
     */
    if (d->downstream->panic_write) {
        return d->downstream->panic_write(data, len, d->downstream->ctx);
    }
    if (d->downstream->write) {
        return d->downstream->write(data, len, d->downstream->ctx);
    }
    return -2;
}

static void deferred_on_drop(uint32_t lost, void *ctx) {
    axiom_backend_deferred_ctx_t *d = (axiom_backend_deferred_ctx_t *)ctx;
    if (!d || !d->downstream) {
        return;
    }

    /* 传播 drop 事件到下游 */
    if (d->downstream->on_drop) {
        d->downstream->on_drop(lost, d->downstream->ctx);
    }
}

/* ---------------------------------------------------------------------------
 * Public API Implementation
 * --------------------------------------------------------------------------- */

int axiom_backend_deferred_init(axiom_backend_deferred_ctx_t *ctx,
                                 axiom_deferred_ring_ctx_t *deferred_ring_ctx,
                                 const axiom_backend_t *downstream) {
    if (!ctx || !deferred_ring_ctx || !downstream) {
        return -1;
    }

    if (!downstream->write) {
        return -2;  /* downstream 必须提供 write 回调 */
    }

    /* Initialize deferred backend context.
     * The actual ring buffer is embedded in ctx->deferred_ring, not the
     * external deferred_ring_ctx parameter (which is kept for API compatibility
     * but whose ring member is unused). */
    memset(ctx, 0, sizeof(*ctx));
    ctx->downstream = downstream;
    axiom_ring_init(&ctx->deferred_ring.ring,
                    ctx->deferred_ring.buf,
                    AXIOM_DEFERRED_RING_SIZE);
    (void)deferred_ring_ctx; /* API compatibility — not used */

    /* 初始化 embedded backend 结构，使用 AXIOM_BACKEND_INIT 宏
     * 自动填充 size 字段，确保前向兼容的结构体演进机制。
     * 赋值场景需显式转换为 compound literal。 */
    ctx->backend = (axiom_backend_t)AXIOM_BACKEND_INIT(
        .name = "deferred",
        .caps = 0,  /* deferred 不声明具体 capability */
        .write = deferred_write,
        .ready = deferred_ready,
        .flush = deferred_flush,
        .panic_write = deferred_panic_write,
        .on_drop = deferred_on_drop,
        .ctx = ctx,
    );

    return 0;
}

const axiom_backend_t *axiom_backend_deferred_get_backend(axiom_backend_deferred_ctx_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    return &ctx->backend;
}

uint32_t axiom_backend_deferred_pending(const axiom_backend_deferred_ctx_t *ctx) {
    if (!ctx) {
        return 0;
    }
    return axiom_ring_used(&ctx->deferred_ring.ring);
}

void axiom_backend_deferred_reset(axiom_backend_deferred_ctx_t *ctx) {
    if (!ctx) {
        return;
    }
    axiom_ring_reset(&ctx->deferred_ring.ring);
}