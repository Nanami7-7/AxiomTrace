#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "axiom_event.h"
#include "axiom_encode.h"
#include "axiom_crc.h"

int main(void) {
    /* Verify header size */
    if (sizeof(axiom_event_header_t) != 8) {
        printf("test_event: FAILED header size %zu != 8\n", sizeof(axiom_event_header_t));
        return 1;
    }

    /* Verify wire version */
    if (AXIOM_WIRE_VERSION != 0x20) {
        printf("test_event: FAILED wire version 0x%02X != 0x20\n", AXIOM_WIRE_VERSION);
        return 1;
    }

    /* Test encoding */
    uint8_t payload[32];
    uint8_t pos = 0;
    axiom_enc_u16(payload, &pos, 0x1234);
    axiom_enc_i16(payload, &pos, (int16_t)(-1));
    axiom_enc_f32(payload, &pos, 1.5f);

    if (pos != 8) {
        printf("test_event: FAILED payload len %u != 8\n", pos);
        return 1;
    }

    /* Verify little-endian encoding of u16 */
    if (payload[0] != 0x34 || payload[1] != 0x12) {
        printf("test_event: FAILED u16 endianness\n");
        return 1;
    }

    printf("test_event: PASSED\n");
    return 0;
}
