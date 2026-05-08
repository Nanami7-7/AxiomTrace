#include "axiom_selftest.h"

#ifndef AXIOM_SELFTEST
#define AXIOM_SELFTEST 0
#endif

#if AXIOM_SELFTEST

#include "axiom_crc.h"
#include "axiom_ring.h"
#include "axiom_encode.h"
#include "axiom_port.h"
#include <string.h>

/* CRC-16/CCITT-FALSE known-answer: CRC16("123456789", 9) = 0x29B1 */
#define AXIOM_CRC16_KNOWN_ANSWER 0x29B1u

static bool test_crc16(void) {
    const uint8_t test_data[] = "123456789";
    uint16_t crc = axiom_crc16(test_data, 9);
    return crc == AXIOM_CRC16_KNOWN_ANSWER;
}

static bool test_crc16_update(void) {
    /* Byte-at-a-time must match whole-buffer result */
    const uint8_t test_data[] = "Hello, AxiomTrace!";
    size_t len = sizeof(test_data) - 1; /* exclude NUL */

    uint16_t crc_whole = axiom_crc16(test_data, len);

    uint16_t crc_byte = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc_byte = axiom_crc16_update(crc_byte, test_data[i]);
    }

    return crc_whole == crc_byte;
}

static bool test_ring_buffer(void) {
    axiom_ring_t ring;
    uint8_t buf[256]; /* power of 2, small for stack */
    axiom_ring_init(&ring, buf, 256);

    /* Write and read back a known pattern */
    uint8_t write_data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    bool ok = axiom_ring_write(&ring, write_data, sizeof(write_data));
    if (!ok) return false;

    uint8_t read_data[8];
    uint16_t n = axiom_ring_read(&ring, read_data, sizeof(read_data));
    if (n != sizeof(write_data)) return false;

    return memcmp(write_data, read_data, sizeof(write_data)) == 0;
}

static bool test_ring_overflow(void) {
    axiom_ring_t ring;
    uint8_t buf[16]; /* very small ring */
    axiom_ring_init(&ring, buf, 16);

    /* Fill to capacity */
    uint8_t data[16];
    memset(data, 0xAA, sizeof(data));
    bool ok = axiom_ring_write(&ring, data, 16);
    if (!ok) return false;

    /* Next write should fail in DROP mode */
    ok = axiom_ring_write(&ring, data, 4);
    return ok == false; /* expected to fail */
}

static bool test_encoder_roundtrip(void) {
    uint8_t enc_buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;

    axiom_enc_u8(enc_buf, &pos, 0x42);
    axiom_enc_u16(enc_buf, &pos, 0xBEEF);
    axiom_enc_u32(enc_buf, &pos, 0xDEADBEEF);
    axiom_enc_bool(enc_buf, &pos, true);

    /* Verify the encoded bytes contain the correct type tags */
    if (enc_buf[0] != AXIOM_TYPE_U8)  return false; /* u8 tag */
    if (enc_buf[1] != 0x42)           return false; /* u8 val */
    if (enc_buf[2] != AXIOM_TYPE_U16) return false; /* u16 tag */
    if (enc_buf[3] != 0xEF)           return false; /* u16 lo */
    if (enc_buf[4] != 0xBE)           return false; /* u16 hi */
    if (enc_buf[5] != AXIOM_TYPE_U32) return false; /* u32 tag */
    if (enc_buf[10] != AXIOM_TYPE_BOOL) return false; /* bool tag */
    if (enc_buf[11] != 1u)              return false; /* true */

    return true;
}

static bool test_encoder_overflow_protection(void) {
    /* The encoder checks against AXIOM_MAX_PAYLOAD_LEN, not the actual buffer size.
     * Fill a buffer with u8 writes until we reach the limit, then verify that
     * an additional write is rejected and sets the overflow flag. */
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;

    /* Fill with u8 values (2 bytes each: tag + value) */
    uint8_t count = 0;
    while (pos + 2u <= AXIOM_MAX_PAYLOAD_LEN) {
        axiom_enc_u8(buf, &pos, count++);
    }

    /* pos is now at or within 1 byte of AXIOM_MAX_PAYLOAD_LEN.
     * A u16 needs 3 bytes, which should be rejected. */
#if AXIOM_ENCODE_OVERFLOW_DETECTION
    axiom_encode_overflow = false;
#endif

    axiom_enc_u16(buf, &pos, 0x1234); /* should be silently skipped */

    /* Verify pos did not advance past AXIOM_MAX_PAYLOAD_LEN */
    if (pos > AXIOM_MAX_PAYLOAD_LEN) return false;

#if AXIOM_ENCODE_OVERFLOW_DETECTION
    if (!axiom_encode_overflow) return false; /* should have been flagged */
#endif

    return true;
}

/* ---------------------------------------------------------------------------
 * Test runner helper — prints failure and returns false on first failure
 * --------------------------------------------------------------------------- */
static bool run_test(const char *name, bool (*test_fn)(void)) {
    if (!test_fn()) {
#if AXIOM_DEBUG
        axiom_port_string_out("[selftest] FAIL: ");
        axiom_port_string_out(name);
        axiom_port_string_out("\n");
#endif
        return false;
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * Self-Test Entry Point
 * --------------------------------------------------------------------------- */
bool axiom_selftest(void) {
    if (!run_test("CRC-16",            test_crc16))                    return false;
    if (!run_test("CRC-16 update",     test_crc16_update))             return false;
    if (!run_test("ring buffer",       test_ring_buffer))              return false;
    if (!run_test("ring overflow",     test_ring_overflow))            return false;
    if (!run_test("encoder roundtrip", test_encoder_roundtrip))        return false;
    if (!run_test("encoder overflow",  test_encoder_overflow_protection)) return false;

    return true;
}

#else /* AXIOM_SELFTEST == 0 */

/* Stubs when self-test is disabled — always passes. */
bool axiom_selftest(void) {
    return true;
}

#endif /* AXIOM_SELFTEST */