/* test_encoder.c — unit tests for axiom_encode.h inline encoders */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "axiom_encode.h"

volatile bool axiom_encode_overflow = false;

static int failures = 0;

#define CHECK(msg, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        failures++; \
    } \
} while(0)

/* ---- Test 1: axiom_enc_u8 ---- */
static void test_enc_u8(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    axiom_enc_u8(buf, &pos, 0x42);
    CHECK("u8: value", buf[0] == 0x42);
    CHECK("u8: pos == 1", pos == 1);
}

/* ---- Test 2: axiom_enc_u16 little-endian ---- */
static void test_enc_u16(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    axiom_enc_u16(buf, &pos, 0x1234);
    CHECK("u16: low byte", buf[0] == 0x34);
    CHECK("u16: high byte", buf[1] == 0x12);
    CHECK("u16: pos == 2", pos == 2);
}

/* ---- Test 3: axiom_enc_u32 little-endian ---- */
static void test_enc_u32(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    axiom_enc_u32(buf, &pos, 0xDEADBEEFu);
    CHECK("u32: byte0", buf[0] == 0xEF);
    CHECK("u32: byte1", buf[1] == 0xBE);
    CHECK("u32: byte2", buf[2] == 0xAD);
    CHECK("u32: byte3", buf[3] == 0xDE);
    CHECK("u32: pos == 4", pos == 4);
}

/* ---- Test 4: axiom_enc_bool ---- */
static void test_enc_bool(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    axiom_enc_bool(buf, &pos, true);
    CHECK("bool true: value", buf[0] == 1u);

    axiom_enc_bool(buf, &pos, false);
    CHECK("bool false: value", buf[1] == 0u);
    CHECK("bool: pos == 2", pos == 2);
}

/* ---- Test 5: axiom_enc_i8 ---- */
static void test_enc_i8(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    axiom_enc_i8(buf, &pos, -42);
    CHECK("i8: value", buf[0] == (uint8_t)(-42));
    CHECK("i8: pos == 1", pos == 1);
}

/* ---- Test 6: axiom_enc_i16 ---- */
static void test_enc_i16(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    axiom_enc_i16(buf, &pos, -1234);
    CHECK("i16: low byte", buf[0] == (uint8_t)((-1234) & 0xFF));
    CHECK("i16: high byte", buf[1] == (uint8_t)(((-1234) >> 8) & 0xFF));
}

/* ---- Test 7: axiom_enc_i32 ---- */
static void test_enc_i32(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    axiom_enc_i32(buf, &pos, -1);
    CHECK("i32: byte0 (0xFF)", buf[0] == 0xFF);
    CHECK("i32: byte3 (0xFF)", buf[3] == 0xFF);
}

/* ---- Test 8: axiom_enc_f32 ---- */
static void test_enc_f32(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    axiom_enc_f32(buf, &pos, 3.14f);
    CHECK("f32: pos == 4", pos == 4);
}

/* ---- Test 9: axiom_enc_timestamp ---- */
static void test_enc_timestamp(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    axiom_enc_timestamp(buf, &pos, 0x12345678u);
    CHECK("timestamp: byte0", buf[0] == 0x78);
    CHECK("timestamp: byte3", buf[3] == 0x12);
    CHECK("timestamp: pos == 4", pos == 4);
}

/* ---- Test 10: axiom_enc_bytes ---- */
static void test_enc_bytes(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    const uint8_t src[] = {0xAA, 0xBB, 0xCC};
    axiom_enc_bytes(buf, &pos, src, 3);
    CHECK("bytes: length", buf[0] == 3);
    CHECK("bytes: data[0]", buf[1] == 0xAA);
    CHECK("bytes: data[1]", buf[2] == 0xBB);
    CHECK("bytes: data[2]", buf[3] == 0xCC);
    CHECK("bytes: pos == 4", pos == 4);
}

/* ---- Test 11: Multiple fields sequential encoding ---- */
static void test_multi_field(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;

    axiom_enc_u16(buf, &pos, 0x1234);   /* pos = 2 */
    axiom_enc_u8(buf, &pos, 0xFF);       /* pos = 3 */
    axiom_enc_u32(buf, &pos, 0xDEADBEEFu); /* pos = 7 */
    axiom_enc_bool(buf, &pos, true);     /* pos = 8 */

    CHECK("multi: total pos == 8", pos == 8);
    CHECK("multi: field1 val", buf[0] == 0x34);
    CHECK("multi: field2 val", buf[2] == 0xFF);
    CHECK("multi: field3 val", buf[3] == 0xEF);
    CHECK("multi: field4 val", buf[7] == 1u);
}

/* ---- Test 12: Overflow detection ---- */
static void test_overflow(void) {
    /* Use a buffer sized to AXIOM_MAX_PAYLOAD_LEN but try to fill past it */
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = AXIOM_MAX_PAYLOAD_LEN - 0; /* 0 byte free, need 1 for u8 */

    axiom_encode_overflow = false;
    axiom_enc_u8(buf, &pos, 0x01);
    CHECK("overflow: u8 skipped", pos == AXIOM_MAX_PAYLOAD_LEN - 0);
    CHECK("overflow: flag set", axiom_encode_overflow == true);

    /* Reset and test u16 overflow (needs 2 bytes) */
    pos = AXIOM_MAX_PAYLOAD_LEN - 1;
    axiom_encode_overflow = false;
    axiom_enc_u16(buf, &pos, 0x1234);
    CHECK("overflow: u16 skipped", pos == AXIOM_MAX_PAYLOAD_LEN - 1);
    CHECK("overflow: flag set again", axiom_encode_overflow == true);

    /* Test u32 overflow (needs 4 bytes) */
    pos = AXIOM_MAX_PAYLOAD_LEN - 3;
    axiom_encode_overflow = false;
    axiom_enc_u32(buf, &pos, 0xDEADBEEF);
    CHECK("overflow: u32 skipped", pos == AXIOM_MAX_PAYLOAD_LEN - 3);
    CHECK("overflow: u32 flag set", axiom_encode_overflow == true);
}

/* ---- Test 13: Bytes overflow ---- */
static void test_bytes_overflow(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = AXIOM_MAX_PAYLOAD_LEN - 2;
    uint8_t src[] = {0x01, 0x02, 0x03};

    axiom_encode_overflow = false;
    axiom_enc_bytes(buf, &pos, src, 3);
    /* Needs 1 (len) + 3 (data) = 4 bytes, only 2 free */
    CHECK("bytes overflow: skipped", pos == AXIOM_MAX_PAYLOAD_LEN - 2);
    CHECK("bytes overflow: flag set", axiom_encode_overflow == true);
}

/* ---- Test 14: Exact fit at boundary ---- */
static void test_exact_fit(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = AXIOM_MAX_PAYLOAD_LEN - 1;
    axiom_encode_overflow = false;
    axiom_enc_u8(buf, &pos, 0x01);
    CHECK("exact fit: u8 written", pos == AXIOM_MAX_PAYLOAD_LEN);
    CHECK("exact fit: no overflow", axiom_encode_overflow == false);
}

int main(void) {
    test_enc_u8();
    test_enc_u16();
    test_enc_u32();
    test_enc_bool();
    test_enc_i8();
    test_enc_i16();
    test_enc_i32();
    test_enc_f32();
    test_enc_timestamp();
    test_enc_bytes();
    test_multi_field();
    test_overflow();
    test_bytes_overflow();
    test_exact_fit();

    if (failures == 0) {
        printf("test_encoder: PASSED (14 test groups)\n");
    } else {
        printf("test_encoder: FAILED (%d failures)\n", failures);
    }
    return failures;
}