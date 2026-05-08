/* test_selftest.c — Validate axiom_selftest() passes. */
#include "axiom_selftest.h"
#include "axiom_crc.h"
#include "axiom_ring.h"
#include "axiom_encode.h"
#include "axiom_event.h"
#include <stdio.h>
#include <string.h>

/* Inline subtests with printf for diagnostics */
static void run_subtests(void) {
    /* CRC-16 */
    const uint8_t d1[] = "123456789";
    uint16_t crc = axiom_crc16(d1, 9);
    printf("  CRC-16(123456789) = 0x%04X (expect 0x29B1): %s\n", crc, crc == 0x29B1 ? "OK" : "FAIL");

    /* CRC-16 update */
    const uint8_t d2[] = "Hello, AxiomTrace!";
    size_t len = sizeof(d2) - 1;
    uint16_t crc_whole = axiom_crc16(d2, len);
    uint16_t crc_byte = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) crc_byte = axiom_crc16_update(crc_byte, d2[i]);
    printf("  CRC-16 update: %s\n", crc_whole == crc_byte ? "OK" : "FAIL");

    /* Ring buffer */
    axiom_ring_t ring;
    uint8_t buf[256];
    axiom_ring_init(&ring, buf, 256);
    uint8_t wd[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    bool ok = axiom_ring_write(&ring, wd, sizeof(wd));
    uint8_t rd[8];
    uint16_t n = axiom_ring_read(&ring, rd, sizeof(rd));
    printf("  Ring buffer write: %s, read: %u bytes, memcmp: %s\n",
           ok ? "OK" : "FAIL", n, memcmp(wd, rd, sizeof(wd)) == 0 ? "OK" : "FAIL");

    /* Encoder roundtrip */
    uint8_t enc[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;
    axiom_enc_u8(enc, &pos, 0x42);
    axiom_enc_u16(enc, &pos, 0xBEEF);
    axiom_enc_u32(enc, &pos, 0xDEADBEEF);
    axiom_enc_bool(enc, &pos, true);
    printf("  Encoder roundtrip pos=%u: ", pos);
    printf("enc[0]=0x%02X(enc[1]=0x%02X) ", enc[0], enc[1]);
    printf("enc[2]=0x%02X(enc[3]=0x%02X,enc[4]=0x%02X) ", enc[2], enc[3], enc[4]);
    printf("enc[5]=0x%02X ", enc[5]);
    printf("enc[10]=0x%02X,enc[11]=0x%02X\n", enc[10], enc[11]);

    /* Encoder overflow — buffer too small for u16 (needs tag(1)+val(2)=3 bytes).
     * The overflow check uses AXIOM_MAX_PAYLOAD_LEN, not buffer size,
     * so we use a full-size buffer and rely on the overflow flag. */
    uint8_t sb[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t sp = 0;
    axiom_enc_u8(sb, &sp, 0x01);
    axiom_enc_u8(sb, &sp, 0x02);
    printf("  Encoder overflow pre: sp=%u\n", sp);
    axiom_enc_u16(sb, &sp, 0x1234);
    printf("  Encoder overflow post: sp=%u (expect <=4)\n", sp);
}

int main(void) {
    printf("Running axiom_selftest()...\n");
    run_subtests();
    if (!axiom_selftest()) {
        printf("FAIL: axiom_selftest() returned false\n");
        return 1;
    }
    printf("PASS: axiom_selftest() returned true\n");
    return 0;
}
