/**
 * @file    proto_crc.c
 * @brief   CRC-16/CCITT-FALSE 独立实现 (规范 §4.1)
 * @note    poly=0x1021, init=0xFFFF, refin=false, refout=false, xorout=0x0000
 *          测试向量: CRC16-CCITT-FALSE("123456789") = 0x29B1
 */
#include "proto_crc.h"

uint16_t proto_crc16_ccitt_false(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;

    if (data == NULL) {
        return crc;
    }

    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (uint8_t i = 0; i < 8; ++i) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}
