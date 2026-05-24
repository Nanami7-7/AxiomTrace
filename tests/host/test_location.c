#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "axiomtrace.h"

static uint8_t s_frame[512];
static uint16_t s_frame_len;
static int s_failures;

static int capture_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)ctx;
    if (len <= sizeof(s_frame)) {
        memcpy(s_frame, buf, len);
        s_frame_len = len;
    }
    return 0;
}

#define CHECK(msg, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        s_failures++; \
    } \
} while (0)

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8u);
}

static const uint8_t *payload_start(uint8_t *payload_len) {
    uint8_t ts_len = axiom_timestamp_decode_len(s_frame[AXIOM_HEADER_LEN]);
    *payload_len = s_frame[AXIOM_HEADER_LEN + ts_len];
    return s_frame + AXIOM_HEADER_LEN + ts_len + 1u;
}

static void check_location_suffix(uint16_t line, uint8_t user_payload_len) {
    uint8_t payload_len = 0;
    const uint8_t *payload = payload_start(&payload_len);
#if AXIOM_CFG_LOCATION_MODE == AXIOM_CFG_LOCATION_MODE_FILE_ID
    (void)user_payload_len;
    const uint8_t *location = payload + payload_len - 6u;
    CHECK("location: file-id payload length", payload_len >= 6u);
    CHECK("location: tag", location[0] == AXIOM_TYPE_META_LOCATION);
    CHECK("location: file-id mode", location[1] == AXIOM_CFG_LOCATION_MODE_FILE_ID);
    CHECK("location: source id", read_u16(location + 2) == (uint16_t)AXIOM_SOURCE_FILE_ID);
    CHECK("location: line", read_u16(location + 4) == line);
#elif AXIOM_CFG_LOCATION_MODE == AXIOM_CFG_LOCATION_MODE_HASH
    (void)user_payload_len;
    const uint8_t *location = payload + payload_len - 8u;
    CHECK("location: hash payload length", payload_len >= 8u);
    CHECK("location: tag", location[0] == AXIOM_TYPE_META_LOCATION);
    CHECK("location: hash mode", location[1] == AXIOM_CFG_LOCATION_MODE_HASH);
    CHECK("location: file hash", read_u16(location + 2) == _axiom_fnv1a_16(__FILE__));
    CHECK("location: line", read_u16(location + 4) == line);
    CHECK("location: function hash", read_u16(location + 6) == _axiom_fnv1a_16("test_location_events"));
#else
    (void)line;
    (void)payload;
    CHECK("location: none preserves payload", payload_len == user_payload_len);
#endif
}

static void test_location_events(void) {
    uint16_t event_line = (uint16_t)(__LINE__ + 1u);
    AX_EVT(INFO, 0x10, 0x0001, (uint8_t)42);
    axiom_flush();
    check_location_suffix(event_line, 2u);

    uint16_t fault_line = (uint16_t)(__LINE__ + 1u);
    AX_FAULT(0x10, 0x0002, (uint16_t)7);
    axiom_flush();
    check_location_suffix(fault_line, 3u);

    uint16_t kv_line = (uint16_t)(__LINE__ + 1u);
    AX_KV(INFO, 0x10, 0x0003, "value", (uint8_t)3);
    axiom_flush();
    check_location_suffix(kv_line, 5u);
}

static void test_probe_has_no_location(void) {
    uint8_t payload_len = 0;
    AX_PROBE("sample", (uint8_t)9);
    axiom_flush();
    (void)payload_start(&payload_len);
    CHECK("probe: metadata not appended", payload_len == 5u);
}

static void test_metadata_identity(void) {
    static const uint8_t metadata_id[AXIOM_METADATA_ID_LEN] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    uint8_t payload_len = 0;
    const uint8_t *payload;

    axiom_emit_metadata_id(metadata_id);
    axiom_flush();
    payload = payload_start(&payload_len);
    CHECK("identity: module", s_frame[3] == AXIOM_SYSTEM_MODULE_ID);
    CHECK("identity: event", read_u16(s_frame + 4) == AXIOM_SYSTEM_EVENT_METADATA_ID);
    CHECK("identity: length", payload_len == 9u);
    CHECK("identity: tag", payload[0] == AXIOM_TYPE_META_IDENTITY);
    CHECK("identity: bytes", memcmp(payload + 1, metadata_id, AXIOM_METADATA_ID_LEN) == 0);
}

static void test_location_overflow(void) {
#if AXIOM_CFG_LOCATION_MODE != AXIOM_CFG_LOCATION_MODE_NONE
    uint8_t payload[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t position;
    axiom_encode_overflow = false;
#if AXIOM_CFG_LOCATION_MODE == AXIOM_CFG_LOCATION_MODE_FILE_ID
    position = (uint8_t)(AXIOM_MAX_PAYLOAD_LEN - 5u);
    axiom_enc_meta_location_file_id(payload, &position, 1u, 2u);
#else
    position = (uint8_t)(AXIOM_MAX_PAYLOAD_LEN - 7u);
    axiom_enc_meta_location_hash(payload, &position, 1u, 2u, 3u);
#endif
    CHECK("location: overflow detected", axiom_encode_overflow);
#endif
}

int main(void) {
    static const axiom_backend_t backend = AXIOM_BACKEND_INIT(.name = "capture", .write = capture_write);
    axiom_init();
    CHECK("backend register", axiom_backend_register(&backend) == AXIOM_BACKEND_OK);

    test_location_events();
    test_probe_has_no_location();
    test_metadata_identity();
    test_location_overflow();

    CHECK("frame captured", s_frame_len > 0u);
    if (s_failures == 0) {
        printf("test_location: PASSED\n");
    }
    return s_failures;
}
