#ifndef AXIOM_CRC_H
#define AXIOM_CRC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * CRC-16/CCITT-FALSE
 * poly = 0x1021, init = 0xFFFF, no reflection, final XOR = 0x0000
 * --------------------------------------------------------------------------- */

uint16_t axiom_crc16(const uint8_t *data, size_t len);

/* Lookup table (256 bytes, stored in ROM) */
extern const uint16_t axiom_crc16_table[256];

/* Incremental CRC update — single byte */
static inline uint16_t axiom_crc16_update(uint16_t crc, uint8_t data) {
    return (uint16_t)((crc << 8) ^ axiom_crc16_table[((crc >> 8) ^ data) & 0xFFu]);
}

/* Incremental CRC update — byte range (N bytes) */
static inline uint16_t axiom_crc16_update_range(uint16_t crc, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        crc = axiom_crc16_update(crc, data[i]);
    }
    return crc;
}

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_CRC_H */
