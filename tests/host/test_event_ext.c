/* test_event_ext.c — extended tests for axiom_event.c (axiom_write/axiom_flush) */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "axiom_event.h"
#include "axiom_frontend.h"
#include "axiom_backend.h"
#include "axiom_diagnostics.h"
#include "axiom_filter.h"
#include "axiom_timestamp.h"
#include "axiom_port.h"


/* ---- Mock backend ---- */
static uint8_t last_frame[512];
static uint16_t last_frame_len;
static int dispatch_count;
static int backend_flush_count;
static uint32_t drop_summary_lost;
static int drop_summary_count;

static int mock_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)ctx;
    if (len <= sizeof(last_frame)) {
        memcpy(last_frame, buf, len);
        last_frame_len = len;
    }
    dispatch_count++;
    if (len >= 19u && buf[3] == AXIOM_SYSTEM_MODULE_ID &&
        ((uint16_t)buf[4] | ((uint16_t)buf[5] << 8u)) == AXIOM_SYSTEM_EVENT_DROP_SUMMARY) {
        uint8_t timestamp_len = axiom_timestamp_decode_len(buf[8]);
        uint16_t payload_offset = (uint16_t)(9u + timestamp_len);
        if (payload_offset + 7u <= len - AXIOM_CRC_LEN) {
            const uint8_t *payload = buf + payload_offset;
            drop_summary_lost += (uint32_t)payload[0] |
                ((uint32_t)payload[1] << 8u) |
                ((uint32_t)payload[2] << 16u) |
                ((uint32_t)payload[3] << 24u);
            drop_summary_count++;
        }
    }
    return 0;
}

static int mock_flush(void *ctx) {
    (void)ctx;
    backend_flush_count++;
    return 0;
}

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
    backend_flush_count = 0;
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
    CHECK("empty_write: public flush cascades to backend", backend_flush_count == 1);
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
    axiom_diagnostics_reset();
    dispatch_count = 0;
    axiom_write(AXIOM_LEVEL_MAX, 0xFF, 0xFFFF, NULL, 0);
    axiom_write(AXIOM_LEVEL_INFO, 1u, 2u, NULL, 1u);
    uint8_t oversized[AXIOM_MAX_PAYLOAD_LEN + 1u];
    axiom_write(AXIOM_LEVEL_INFO, 1u, 3u, oversized, (uint8_t)sizeof(oversized));
    axiom_flush();

    CHECK("invalid_level: no dispatch", dispatch_count == 0);
    axiom_diagnostics_t diagnostics;
    axiom_diagnostics_get(&diagnostics);
    CHECK("invalid_input: all rejected inputs counted", diagnostics.invalid_input == 3u);
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

    axiom_diagnostics_t diagnostics;
    axiom_diagnostics_get(&diagnostics);
    CHECK("filter_mask: filtered event counted", diagnostics.filtered == 1u);

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

/* ---- Test 8: Ring pressure reports one accurate recovery summary ---- */
static void test_ring_full_drop_summary(void) {
    uint8_t payload[AXIOM_MAX_PAYLOAD_LEN] = {0};
    axiom_init();
    drop_summary_lost = 0u;
    drop_summary_count = 0;
    for (uint16_t i = 0u; i < 40u; ++i) {
        axiom_write(AXIOM_LEVEL_INFO, 1u, (uint16_t)(0x200u + i),
                    payload, sizeof(payload));
    }
    axiom_diagnostics_t diagnostics;
    axiom_diagnostics_get(&diagnostics);
    CHECK("ring_full: pressure counted", diagnostics.ring_full > 0u);

    axiom_flush();
    CHECK("ring_full: no summary before recovery write", drop_summary_count == 0);
    axiom_write(AXIOM_LEVEL_INFO, 1u, 0x300u, NULL, 0u);
    axiom_flush();
    CHECK("ring_full: one recovery summary", drop_summary_count == 1);
    CHECK("ring_full: summary count accurate", drop_summary_lost == diagnostics.ring_full);
}

/* ---- Test 9: Timestamp VLE compression & overflow / abrupt jumps ---- */
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
    test_ring_full_drop_summary();
    test_timestamp_vle_overflow_and_abrupt_jumps();

    if (failures == 0) {
        printf("test_event_ext: PASSED (8 test groups)\n");
    } else {
        printf("test_event_ext: FAILED (%d failures)\n", failures);
    }
    return failures;
}
