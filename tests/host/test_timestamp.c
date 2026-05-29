#include "test_helpers.h"

#include "axiom_timestamp.h"
#include "axiom_port.h"

#include <string.h>

/* Helper: set simulated time and encode */
static uint8_t encode_at(axiom_timestamp_ctx_t *ctx, uint32_t time_val, uint8_t *out) {
    g_axiom_port_simulated_time = time_val;
    return axiom_timestamp_encode(ctx, out);
}

static void test_init(void) {
    axiom_timestamp_ctx_t ctx;
    axiom_timestamp_init(&ctx);
    CHECK("init: last_raw == 0", ctx.last_raw == 0);
    CHECK("init: baseline == 0", ctx.baseline == 0);
}

static void test_1byte_encoding(void) {
    axiom_timestamp_ctx_t ctx;
    uint8_t out[5];
    uint8_t len;

    axiom_timestamp_init(&ctx);

    /* delta=0 → 1 byte, value=0x00 */
    len = encode_at(&ctx, 0, out);
    CHECK("1byte: delta=0 len=1", len == 1);
    CHECK("1byte: delta=0 value", out[0] == 0x00);

    /* delta=127 → 1 byte, value=0x7F */
    len = encode_at(&ctx, 127, out);
    CHECK("1byte: delta=127 len=1", len == 1);
    CHECK("1byte: delta=127 value", out[0] == 0x7F);
}

static void test_2byte_encoding(void) {
    axiom_timestamp_ctx_t ctx;
    uint8_t out[5];
    uint8_t len;

    axiom_timestamp_init(&ctx);

    /* delta=128 → 2 bytes: 0x80 | ((128>>8)&0x3F) = 0x80 | 0 = 0x80, lo byte = 128 = 0x80 */
    len = encode_at(&ctx, 128, out);
    CHECK("2byte: delta=128 len=2", len == 2);
    CHECK("2byte: delta=128 high", out[0] == 0x80);
    CHECK("2byte: delta=128 low", out[1] == 0x80);

    /* delta=16383 → 2 bytes, value=0xBF 0xFF */
    len = encode_at(&ctx, 128 + 16383, out);
    CHECK("2byte: delta=16383 len=2", len == 2);
    CHECK("2byte: delta=16383 high", out[0] == 0xBF);
    CHECK("2byte: delta=16383 low", out[1] == 0xFF);
}

static void test_3byte_encoding(void) {
    axiom_timestamp_ctx_t ctx;
    uint8_t out[5];
    uint8_t len;

    axiom_timestamp_init(&ctx);

    /* delta=16384 → 3 bytes */
    len = encode_at(&ctx, 16384, out);
    CHECK("3byte: delta=16384 len=3", len == 3);
    CHECK("3byte: delta=16384 high", out[0] == 0xC0);
    CHECK("3byte: delta=16384 mid", out[1] == 0x40);
    CHECK("3byte: delta=16384 low", out[2] == 0x00);

    /* delta=2097151 → 3 bytes */
    len = encode_at(&ctx, 16384 + 2097151, out);
    CHECK("3byte: delta=2097151 len=3", len == 3);
    CHECK("3byte: delta=2097151 high", out[0] == 0xDF);
    CHECK("3byte: delta=2097151 mid", out[1] == 0xFF);
    CHECK("3byte: delta=2097151 low", out[2] == 0xFF);
}

static void test_5byte_encoding(void) {
    axiom_timestamp_ctx_t ctx;
    uint8_t out[5];
    uint8_t len;

    axiom_timestamp_init(&ctx);

    /* delta=2097152 → 5 bytes, 0xFE prefix */
    len = encode_at(&ctx, 2097152, out);
    CHECK("5byte: delta=2097152 len=5", len == 5);
    CHECK("5byte: delta=2097152 prefix", out[0] == 0xFE);
    CHECK("5byte: delta=2097152 b0", out[1] == 0x00);
    CHECK("5byte: delta=2097152 b1", out[2] == 0x00);
    CHECK("5byte: delta=2097152 b2", out[3] == 0x20);
    CHECK("5byte: delta=2097152 b3", out[4] == 0x00);
}

static void test_decode_len(void) {
    CHECK("decode_len: 0x00 → 1", axiom_timestamp_decode_len(0x00) == 1);
    CHECK("decode_len: 0x7F → 1", axiom_timestamp_decode_len(0x7F) == 1);
    CHECK("decode_len: 0x80 → 2", axiom_timestamp_decode_len(0x80) == 2);
    CHECK("decode_len: 0xBF → 2", axiom_timestamp_decode_len(0xBF) == 2);
    CHECK("decode_len: 0xC0 → 3", axiom_timestamp_decode_len(0xC0) == 3);
    CHECK("decode_len: 0xFD → 3", axiom_timestamp_decode_len(0xFD) == 3);
    CHECK("decode_len: 0xFE → 5", axiom_timestamp_decode_len(0xFE) == 5);
    CHECK("decode_len: 0xFF → 5", axiom_timestamp_decode_len(0xFF) == 5);
}

static void test_consecutive_encoding(void) {
    axiom_timestamp_ctx_t ctx;
    uint8_t out[5];
    uint8_t len;

    axiom_timestamp_init(&ctx);

    /* First encode: time=100, delta=100 */
    len = encode_at(&ctx, 100, out);
    CHECK("consecutive: first len=1", len == 1);
    CHECK("consecutive: first value", out[0] == 100);

    /* Second encode: time=250, delta=150 → 2-byte, lo byte = 150 = 0x96 */
    len = encode_at(&ctx, 250, out);
    CHECK("consecutive: second len=2", len == 2);
    CHECK("consecutive: second high", out[0] == 0x80);
    CHECK("consecutive: second low", out[1] == 0x96);  /* 150 & 0xFF = 150 = 0x96 */

    /* Third encode: time=500, delta=250 */
    len = encode_at(&ctx, 500, out);
    CHECK("consecutive: third len=2", len == 2);
}

int main(void) {
    test_init();
    test_1byte_encoding();
    test_2byte_encoding();
    test_3byte_encoding();
    test_5byte_encoding();
    test_decode_len();
    test_consecutive_encoding();

    TEST_RESULT("test_timestamp", failures);
    return failures;
}
