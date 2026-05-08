#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "axiom_crc.h"

static int failures = 0;

#define CHECK(msg, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        failures++; \
    } \
} while(0)

int main(void) {
    /* ---- Test 1: CRC-16/CCITT-FALSE known vector: "123456789" ---- */
    {
        const uint8_t data[] = "123456789";
        uint16_t crc = axiom_crc16(data, 9);
        CHECK("known vector '123456789' -> 0x29B1", crc == 0x29B1u);
    }

    /* ---- Test 2: Empty data (NULL, 0) ---- */
    {
        uint16_t crc = axiom_crc16(NULL, 0);
        CHECK("empty data -> 0xFFFF", crc == 0xFFFFu);
    }

    /* ---- Test 3: Single byte ---- */
    {
        const uint8_t b = 0x42;
        uint16_t crc = axiom_crc16(&b, 1);
        CHECK("single byte produces valid CRC (not 0xFFFF)", crc != 0xFFFFu);
    }

    /* ---- Test 4: axiom_crc16_update consistency ---- */
    {
        const uint8_t data[] = "123456789";
        uint16_t crc_batch = axiom_crc16(data, 9);

        uint16_t crc_inc = 0xFFFFu;
        for (int i = 0; i < 9; i++) {
            crc_inc = axiom_crc16_update(crc_inc, data[i]);
        }
        CHECK("update() matches batch()", crc_inc == crc_batch);
    }

    /* ---- Test 5: axiom_crc16_update_range consistency ---- */
    {
        const uint8_t data[] = "123456789";
        uint16_t crc_batch = axiom_crc16(data, 9);
        uint16_t crc_range = axiom_crc16_update_range(0xFFFFu, data, 9);
        CHECK("update_range() matches batch()", crc_range == crc_batch);
    }

    /* ---- Test 6: update_range in chunks matches batch ---- */
    {
        const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        uint16_t crc_full = axiom_crc16_update_range(0xFFFFu, data, 8);

        uint16_t crc_chunk = 0xFFFFu;
        crc_chunk = axiom_crc16_update_range(crc_chunk, data, 3);
        crc_chunk = axiom_crc16_update_range(crc_chunk, data + 3, 5);
        CHECK("update_range chunked matches full", crc_chunk == crc_full);
    }

    /* ---- Test 7: 256-byte incrementing sequence ---- */
    {
        uint8_t buf[256];
        for (int i = 0; i < 256; i++) {
            buf[i] = (uint8_t)i;
        }
        uint16_t crc1 = axiom_crc16(buf, 256);
        uint16_t crc2 = axiom_crc16_update_range(0xFFFFu, buf, 256);
        CHECK("256-byte: batch == update_range", crc1 == crc2);

        uint16_t crc_inc = 0xFFFFu;
        for (int i = 0; i < 256; i++) {
            crc_inc = axiom_crc16_update(crc_inc, buf[i]);
        }
        CHECK("256-byte: batch == per-byte update", crc1 == crc_inc);
    }

    if (failures == 0) {
        printf("test_crc: PASSED (7 cases)\n");
    } else {
        printf("test_crc: FAILED (%d failures)\n", failures);
    }
    return failures;
}