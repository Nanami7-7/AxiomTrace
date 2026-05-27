/* test_event_ext.c — extended tests for axiom_event.c (axiom_write/axiom_flush) */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "axiom_event.h"
#include "axiom_frontend.h"
#include "axiom_backend.h"
#include "axiom_filter.h"
#include "axiom_timestamp.h"
#include "axiom_port.h"


/* ---- Mock backend ---- */
static uint8_t last_frame[512];
static uint16_t last_frame_len;
static int dispatch_count;

static int mock_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)ctx;
    if (len <= sizeof(last_frame)) {
        memcpy(last_frame, buf, len);
        last_frame_len = len;
    }
    dispatch_count++;
    return 0;
}

static int mock_flush(void *ctx) { (void)ctx; return 0; }

static int failures = 0;

#define CHECK(msg, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        failures++; \
    } \
} while(0)

/* ---- Test 1: axiom_init + write empty payload ---- */
static void test_init_and_empty_write(void) {
    axiom_init();

    /* Register a mock backend */
    static const axiom_backend_t be = AXIOM_BACKEND_INIT(
        .name = "mock", .caps = AXIOM_BACKEND_CAP_MEMORY,
        .write = mock_write, .flush = mock_flush);
    axiom_backend_register(&be);

    dispatch_count = 0;
    axiom_write(AXIOM_LEVEL_INFO, 0x01, 0x0010, NULL, 0);
    axiom_flush();

    CHECK("empty_write: backend dispatched", dispatch_count >= 1);
    CHECK("empty_write: frame sync byte", last_frame[0] == 0xA5);
    CHECK("empty_write: frame version", last_frame[1] == AXIOM_WIRE_VERSION);
    CHECK("empty_write: level byte", last_frame[2] == AXIOM_LEVEL_INFO);
    CHECK("empty_write: module_id", last_frame[3] == 0x01);
    CHECK("empty_write: event_id low", last_frame[4] == 0x10);
    CHECK("empty_write: event_id high", last_frame[5] == 0x00);
    CHECK("empty_write: seq starts at 0", last_frame[6] == 0x00 && last_frame[7] == 0x00);
}

/* ---- Test 2: Sequence number increments ---- */
static void test_seq_increment(void) {
    /* Record seq before writing */
    dispatch_count = 0;
    axiom_write(AXIOM_LEVEL_INFO, 0x02, 0x0020, NULL, 0);
    /* Flush to get the frame for this single write */
    axiom_flush();
    uint8_t seq_before = last_frame[6]; /* seq of the event we just wrote */

    axiom_write(AXIOM_LEVEL_INFO, 0x02, 0x0021, NULL, 0);
    axiom_write(AXIOM_LEVEL_INFO, 0x02, 0x0022, NULL, 0);
    axiom_flush();

    /* After flush, last_frame contains the last dispatched frame */
    /* Its seq should be exactly seq_before + 2 */
    uint8_t expected_seq = seq_before + 2;
    CHECK("seq: incremented by 2", last_frame[6] == expected_seq);
    CHECK("seq: seq high == 0", last_frame[7] == 0x00);
}

/* ---- Test 3: Write with payload ---- */
static void test_write_with_payload(void) {
    dispatch_count = 0;
    uint8_t payload[] = {AXIOM_TYPE_U8, 0x42};
    axiom_write(AXIOM_LEVEL_ERROR, 0x03, 0x0030, payload, sizeof(payload));
    axiom_flush();

    CHECK("payload: backend dispatched", dispatch_count >= 1);
    CHECK("payload: sync", last_frame[0] == 0xA5);
    CHECK("payload: level == ERROR", last_frame[2] == AXIOM_LEVEL_ERROR);
    CHECK("payload: module_id == 3", last_frame[3] == 0x03);
}

/* ---- Test 4: Invalid level is rejected ---- */
static void test_invalid_level(void) {
    dispatch_count = 0;
    axiom_write(AXIOM_LEVEL_MAX, 0xFF, 0xFFFF, NULL, 0);
    axiom_flush();

    CHECK("invalid_level: no dispatch", dispatch_count == 0);
}

/* ---- Test 5: Filter mask blocks events ---- */
static void test_filter_mask(void) {
    /* Reset filter to allow all */
    axiom_level_mask_set(0xFFFFFFFFu);
    dispatch_count = 0;

    axiom_write(AXIOM_LEVEL_DEBUG, 0x01, 0x0100, NULL, 0);
    axiom_flush();
    int count_with = dispatch_count;

    /* Mask out DEBUG */
    axiom_level_mask_set(0xFFFFFFFEu);
    dispatch_count = 0;
    axiom_write(AXIOM_LEVEL_DEBUG, 0x01, 0x0101, NULL, 0);
    axiom_flush();
    int count_without = dispatch_count;

    CHECK("filter_mask: events pass when mask allows", count_with >= 1);
    CHECK("filter_mask: events blocked when mask excludes", count_without == 0);

    /* Restore */
    axiom_level_mask_set(0xFFFFFFFFu);
}

/* ---- Test 6: Multiple flush cycles ---- */
static void test_multi_flush(void) {
    dispatch_count = 0;
    for (int i = 0; i < 5; i++) {
        axiom_write(AXIOM_LEVEL_INFO, 0x10, (uint16_t)(0x0100 + i), NULL, 0);
    }
    axiom_flush();
    CHECK("multi_flush: 5 events dispatched", dispatch_count >= 5);
}

/* ---- Test 7: Flush on empty ring does nothing ---- */
static void test_flush_empty(void) {
    dispatch_count = 0;
    axiom_flush();
    CHECK("flush_empty: no dispatch", dispatch_count == 0);
}

/* ---- Test 8: Timestamp VLE compression & overflow / abrupt jumps ---- */
static void test_timestamp_vle_overflow_and_abrupt_jumps(void) {
    axiom_timestamp_ctx_t ctx;
    axiom_timestamp_init(&ctx);
    uint8_t encoded[8];
    uint8_t len;

    /* 1. Normal small delta encoding (1 byte) */
    ctx.last_raw = 1000;
    g_axiom_port_simulated_time = 1050; /* delta = 50 */
    len = axiom_timestamp_encode(&ctx, encoded);
    CHECK("timestamp: delta 50 encoded in 1 byte", len == 1);
    CHECK("timestamp: 1-byte VLE mask matches", (encoded[0] & 0x80) == 0);

    /* 2. Extreme overflow: 32-bit hardware timer overflow/wrap-around */
    ctx.last_raw = 0xFFFFFFF0u;
    g_axiom_port_simulated_time = 0x00000005u; /* actual modular delta = 21 */
    len = axiom_timestamp_encode(&ctx, encoded);
    CHECK("timestamp: modular delta 21 encoded in 1 byte", len == 1);
    CHECK("timestamp: overflow delta value correct", encoded[0] == 21);

    /* 3. Time dilation/abrupt jump (e.g. clock reset / synchronization jump) */
    ctx.last_raw = 10000u;
    g_axiom_port_simulated_time = 100u; /* delta = 100 - 10000 = 0xFFFFFF4C */
    len = axiom_timestamp_encode(&ctx, encoded);
    CHECK("timestamp: negative time jump falls back to 5 bytes", len == 5);
    CHECK("timestamp: fallback header is 0xFE", encoded[0] == 0xFEu);
}

int main(void) {
    test_init_and_empty_write();
    test_seq_increment();
    test_write_with_payload();
    test_invalid_level();
    test_filter_mask();
    test_multi_flush();
    test_flush_empty();
    test_timestamp_vle_overflow_and_abrupt_jumps();

    if (failures == 0) {
        printf("test_event_ext: PASSED (8 test groups)\n");
    } else {
        printf("test_event_ext: FAILED (%d failures)\n", failures);
    }
    return failures;
}
