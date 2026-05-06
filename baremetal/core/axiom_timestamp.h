#ifndef AXIOM_TIMESTAMP_H
#define AXIOM_TIMESTAMP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Compressed relative timestamp
 * Stores delta from previous event to reduce payload size.
 * --------------------------------------------------------------------------- */

typedef struct {
    uint32_t last_raw;
    uint32_t baseline; /* Reserved for future time-sync anchor (Unix epoch mapping) */
} axiom_timestamp_ctx_t;

/* Initialize timestamp context */
void axiom_timestamp_init(axiom_timestamp_ctx_t *ctx);

/* Encode current timestamp into up to 5 bytes. Returns encoded length (1..5). */
uint8_t axiom_timestamp_encode(axiom_timestamp_ctx_t *ctx, uint8_t *out);

/* Decode timestamp length from the first byte after the frame header.
 * Returns 1, 2, 3, or 5. Call this on frame[8] to locate payload_len.
 */
static inline uint8_t axiom_timestamp_decode_len(uint8_t first_byte) {
    if (first_byte >= 0xC0) {
        /* 0xFE and 0xFF both signal 5-byte full-delta encoding.
         * 0xC0..0xFD are 3-byte encoding (21-bit delta). */
        return (first_byte >= 0xFEu) ? 5 : 3;
    } else if (first_byte >= 0x80) {
        return 2;
    }
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_TIMESTAMP_H */
