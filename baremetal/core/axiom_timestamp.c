#include "axiom_timestamp.h"
#include "axiom_port.h"

void axiom_timestamp_init(axiom_timestamp_ctx_t *ctx) {
    ctx->last_raw = 0;
    ctx->baseline = 0;
}

uint8_t axiom_timestamp_encode(axiom_timestamp_ctx_t *ctx, uint8_t *out) {
    uint32_t raw = axiom_port_timestamp();
    uint32_t delta = raw - ctx->last_raw;
    ctx->last_raw = raw;

    /* Variable-length encoding: smaller deltas use fewer bytes */
    if (delta <= 0x7Fu) {
        out[0] = (uint8_t)(delta & 0x7Fu);
        return 1;
    } else if (delta <= 0x3FFFu) {
        out[0] = (uint8_t)(0x80u | ((delta >> 8) & 0x3Fu));
        out[1] = (uint8_t)(delta & 0xFFu);
        return 2;
    } else if (delta <= 0x1FFFFFu) {
        out[0] = (uint8_t)(0xC0u | ((delta >> 16) & 0x1Fu));
        out[1] = (uint8_t)((delta >> 8) & 0xFFu);
        out[2] = (uint8_t)(delta & 0xFFu);
        return 3;
    } else {
        /* 使用 0xFE 标记大 delta，0xFF 可以正常出现在数据中 */
        out[0] = 0xFEu;
        out[1] = (uint8_t)(delta & 0xFFu);
        out[2] = (uint8_t)((delta >> 8) & 0xFFu);
        out[3] = (uint8_t)((delta >> 16) & 0xFFu);
        out[4] = (uint8_t)((delta >> 24) & 0xFFu);
        return 5;
    }
}
