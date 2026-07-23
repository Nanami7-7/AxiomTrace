/* test_dynamic_call_chain.c -- end-to-end frontend->core->backend flow tests */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "axiomtrace.h"
#include "axiom_backend.h"
#include "axiom_crc.h"
#include "axiom_timestamp.h"

#define MAX_CAPTURED_FRAMES 32u

typedef struct {
    uint8_t level;
    uint8_t module_id;
    uint16_t event_id;
    uint16_t seq;
    uint8_t timestamp_len;
    uint8_t payload_len;
    const uint8_t *payload;
} parsed_frame_t;

static uint8_t captured_frames[MAX_CAPTURED_FRAMES][AXIOM_MAX_FRAME_LEN];
static uint16_t captured_lengths[MAX_CAPTURED_FRAMES];
static uint8_t captured_count;
static uint32_t busy_drop_count;
static uint32_t failing_drop_count;
static uint32_t failing_write_count;
static bool busy_backend_ready = true;
static int failures;

#define CHECK(msg, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        failures++; \
    } \
} while(0)

static int capture_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)ctx;
    if (captured_count >= MAX_CAPTURED_FRAMES || len > AXIOM_MAX_FRAME_LEN) {
        return -1;
    }
    memcpy(captured_frames[captured_count], buf, len);
    captured_lengths[captured_count] = len;
    captured_count++;
    return 0;
}

static int busy_ready(void *ctx) {
    (void)ctx;
    return busy_backend_ready ? 1 : 0;
}

static int busy_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)buf;
    (void)len;
    (void)ctx;
    return 0;
}

static void busy_drop(uint32_t lost, void *ctx) {
    (void)ctx;
    busy_drop_count += lost;
}

static int failing_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)buf;
    (void)len;
    (void)ctx;
    failing_write_count++;
    return -1;
}

static uint16_t read_le16(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8u));
}

static void failing_drop(uint32_t lost, void *ctx) {
    (void)ctx;
    failing_drop_count += lost;
}

static void reset_capture(void) {
    memset(captured_frames, 0, sizeof(captured_frames));
    memset(captured_lengths, 0, sizeof(captured_lengths));
    captured_count = 0;
}

static bool parse_frame(uint8_t index, parsed_frame_t *parsed) {
    const uint8_t *frame;
    uint16_t len;
    uint8_t payload_offset;
    uint16_t expected_len;
    uint16_t expected_crc;
    uint16_t actual_crc;

    if (index >= captured_count) {
        return false;
    }
    frame = captured_frames[index];
    len = captured_lengths[index];
    if (len < 12u || frame[0] != AXIOM_SYNC_BYTE || frame[1] != AXIOM_WIRE_VERSION) {
        return false;
    }

    parsed->timestamp_len = axiom_timestamp_decode_len(frame[AXIOM_HEADER_LEN]);
    payload_offset = (uint8_t)(AXIOM_HEADER_LEN + parsed->timestamp_len + 1u);
    if (payload_offset + AXIOM_CRC_LEN > len) {
        return false;
    }
    parsed->payload_len = frame[AXIOM_HEADER_LEN + parsed->timestamp_len];
    expected_len = (uint16_t)(payload_offset + parsed->payload_len + AXIOM_CRC_LEN);
    if (expected_len != len) {
        return false;
    }

    expected_crc = read_le16(frame + len - 2u);
    actual_crc = axiom_crc16(frame, (uint16_t)(len - AXIOM_CRC_LEN));
    if (expected_crc != actual_crc) {
        return false;
    }

    parsed->level = frame[2];
    parsed->module_id = frame[3];
    parsed->event_id = read_le16(frame + 4u);
    parsed->seq = read_le16(frame + 6u);
    parsed->payload = frame + payload_offset;
    return true;
}

static void test_frontend_to_backend_frames(void) {
    parsed_frame_t event_frame;
    parsed_frame_t probe_frame;
    parsed_frame_t fault_frame;

    axiom_init();
    reset_capture();
    AX_EVT(INFO, 0x21u, 0x0101u, (uint16_t)0x1234u, (int16_t)-2);
    AX_PROBE("adc", (uint8_t)9u);
    AX_FAULT(0x22u, 0x0202u, (uint8_t)7u);
    axiom_flush();

    CHECK("frontend chain: three frames dispatched", captured_count == 3u);
    CHECK("frontend chain: event frame parses", parse_frame(0, &event_frame));
    CHECK("frontend chain: probe frame parses", parse_frame(1, &probe_frame));
    CHECK("frontend chain: fault frame parses", parse_frame(2, &fault_frame));

    CHECK("event: level", event_frame.level == AXIOM_LEVEL_INFO);
    CHECK("event: module", event_frame.module_id == 0x21u);
    CHECK("event: id", event_frame.event_id == 0x0101u);
    CHECK("event: packed payload length", event_frame.payload_len == 4u);
    CHECK("event: u16 payload low", event_frame.payload[0] == 0x34u);
    CHECK("event: u16 payload high", event_frame.payload[1] == 0x12u);
    CHECK("event: i16 payload low", event_frame.payload[2] == 0xFEu);
    CHECK("event: i16 payload high", event_frame.payload[3] == 0xFFu);

    CHECK("probe: system module", probe_frame.module_id == AXIOM_SYSTEM_MODULE_ID);
    CHECK("probe: system event", probe_frame.event_id == 0u);
    CHECK("probe: typed tag hash", probe_frame.payload[0] == AXIOM_TYPE_U16);
    CHECK("probe: typed value tag", probe_frame.payload[3] == AXIOM_TYPE_U8);
    CHECK("probe: value", probe_frame.payload[4] == 9u);

    CHECK("fault: level", fault_frame.level == AXIOM_LEVEL_FAULT);
    CHECK("fault: module", fault_frame.module_id == 0x22u);
    CHECK("fault: id", fault_frame.event_id == 0x0202u);
    CHECK("fault: payload", fault_frame.payload_len == 1u && fault_frame.payload[0] == 7u);
}

static void test_filter_drop_summary_recovery(void) {
    parsed_frame_t event_frame;
    parsed_frame_t drop_frame;
    uint32_t lost;
    uint16_t event_id;

    axiom_init();
    reset_capture();
    axiom_level_mask_set(0xFFFFFFFEu); /* drop DEBUG */
    AX_EVT(DEBUG, 0x33u, 0x0033u);
    axiom_level_mask_set(0xFFFFFFFFu);
    AX_EVT(INFO, 0x33u, 0x0034u);
    axiom_flush();

    CHECK("drop summary: event + summary dispatched", captured_count == 2u);
    CHECK("drop summary: event parses", parse_frame(0, &event_frame));
    CHECK("drop summary: summary parses", parse_frame(1, &drop_frame));
    lost = (uint32_t)drop_frame.payload[0] |
           ((uint32_t)drop_frame.payload[1] << 8u) |
           ((uint32_t)drop_frame.payload[2] << 16u) |
           ((uint32_t)drop_frame.payload[3] << 24u);
    event_id = read_le16(drop_frame.payload + 5u);

    CHECK("drop summary: normal event first", event_frame.event_id == 0x0034u);
    CHECK("drop summary: system warn", drop_frame.level == AXIOM_LEVEL_WARN);
    CHECK("drop summary: system module", drop_frame.module_id == AXIOM_SYSTEM_MODULE_ID);
    CHECK("drop summary: system event", drop_frame.event_id == AXIOM_SYSTEM_EVENT_DROP_SUMMARY);
    CHECK("drop summary: payload length", drop_frame.payload_len == 7u);
    CHECK("drop summary: lost count", lost == 1u);
    CHECK("drop summary: last module", drop_frame.payload[4] == 0x33u);
    CHECK("drop summary: last event", event_id == 0x0033u);
}

static void test_busy_backend_does_not_block_healthy_backend(void) {
    parsed_frame_t frame;

    axiom_init();
    reset_capture();
    busy_backend_ready = false;
    busy_drop_count = 0u;
    AX_EVT(INFO, 0x44u, 0x0044u, (uint8_t)1u);
    axiom_flush();
    busy_backend_ready = true;

    CHECK("busy backend: healthy backend still receives frame", captured_count == 1u);
    CHECK("busy backend: frame parses", parse_frame(0, &frame));
    CHECK("busy backend: event id", frame.event_id == 0x0044u);
    CHECK("busy backend: drop callback", busy_drop_count >= 1u);
}

static void test_failing_backend_degrades_and_recovers(void) {
    uint8_t active_before;

    axiom_init();
    reset_capture();
    failing_drop_count = 0u;
    failing_write_count = 0u;
    g_axiom_port_simulated_time = 1000u;
    active_before = axiom_backend_active_count();

    for (uint8_t i = 0; i < AXIOM_BACKEND_FAIL_THRESHOLD; ++i) {
        AX_EVT(INFO, 0x55u, (uint16_t)(0x0500u + i), (uint8_t)i);
        axiom_flush();
    }

    CHECK("failing backend: healthy backend receives frames",
          captured_count == AXIOM_BACKEND_FAIL_THRESHOLD);
    CHECK("failing backend: drop callback count",
          failing_drop_count >= AXIOM_BACKEND_FAIL_THRESHOLD);
    CHECK("failing backend: active count decreased",
          axiom_backend_active_count() == (uint8_t)(active_before - 1u));

    g_axiom_port_simulated_time += AXIOM_BACKEND_RECOVERY_US + 1u;
    AX_EVT(INFO, 0x55u, 0x05FFu, (uint8_t)0xFFu);
    axiom_flush();

    CHECK("failing backend: recovery restores active count",
          axiom_backend_active_count() == active_before);
    CHECK("failing backend: recovery attempted write",
          failing_write_count >= (uint32_t)(AXIOM_BACKEND_FAIL_THRESHOLD + 1u));
}

int main(void) {
    static const axiom_backend_t capture_backend = AXIOM_BACKEND_INIT(
        .name = "capture",
        .caps = AXIOM_BACKEND_CAP_MEMORY,
        .write = capture_write
    );
    static const axiom_backend_t busy_backend = AXIOM_BACKEND_INIT(
        .name = "busy",
        .caps = AXIOM_BACKEND_CAP_UART,
        .write = busy_write,
        .ready = busy_ready,
        .on_drop = busy_drop
    );
    static const axiom_backend_t failing_backend = AXIOM_BACKEND_INIT(
        .name = "failing",
        .caps = AXIOM_BACKEND_CAP_USB,
        .write = failing_write,
        .on_drop = failing_drop
    );

    CHECK("register capture backend", axiom_backend_register(&capture_backend) == AXIOM_BACKEND_OK);
    test_frontend_to_backend_frames();
    test_filter_drop_summary_recovery();

    CHECK("register busy backend", axiom_backend_register(&busy_backend) == AXIOM_BACKEND_OK);
    test_busy_backend_does_not_block_healthy_backend();

    CHECK("register failing backend", axiom_backend_register(&failing_backend) == AXIOM_BACKEND_OK);
    test_failing_backend_degrades_and_recovers();

    if (failures == 0) {
        printf("test_dynamic_call_chain: PASSED (4 scenario groups)\n");
    } else {
        printf("test_dynamic_call_chain: FAILED (%d failures)\n", failures);
    }
    return failures;
}
