/**
 * @file    proto_crc.h
 * @brief   CRC-16/CCITT-FALSE 独立实现 (规范 §4.1)
 * @note    参数: poly=0x1021, init=0xFFFF, refin=false, refout=false, xorout=0x0000
 *          不依赖外部库，可独立编译。
 */
#ifndef PROTO_CRC_H
#define PROTO_CRC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  计算 CRC-16/CCITT-FALSE
 * @param  data  数据指针
 * @param  len   数据长度
 * @retval CRC-16 校验值
 * @note   测试向量: CRC16-CCITT-FALSE("123456789") = 0x29B1
 *         不分配堆内存，可在中断安全上下文调用
 */
uint16_t proto_crc16_ccitt_false(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
#endif /* PROTO_CRC_H */
