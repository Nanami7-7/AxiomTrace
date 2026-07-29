/**
 * @file    proto_cobs.h
 * @brief   COBS (Consistent Overhead Byte Stuffing) 编解码 (规范 §4.2)
 * @note    编码器输入为完整逻辑帧 (含 CRC)，输出为无零字节的 COBS 数据。
 *          编码器不追加分隔符，由调用者追加 0x00。
 *          解码器输入不含分隔符；收到分隔符后调用解码器。
 *          不分配堆内存。
 */
#ifndef PROTO_COBS_H
#define PROTO_COBS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  COBS 编码
 * @param  src      原始数据 (可含 0x00)
 * @param  src_len  原始数据长度 (0 也合法，编码为单字节 0x01)
 * @param  dst      编码输出缓冲区
 * @param  dst_cap  输出缓冲区容量 (>= src_len + src_len/254 + 2)
 * @param  dst_len  输出: 编码后长度
 * @retval true     编码成功
 * @retval false    参数非法或 dst_cap 不足
 * @note   输出保证不含 0x00；不追加分隔符
 */
bool proto_cobs_encode(const uint8_t *src, size_t src_len,
                       uint8_t *dst, size_t dst_cap, size_t *dst_len);

/**
 * @brief  COBS 解码
 * @param  src      COBS 编码数据 (不含尾部 0x00 分隔符)
 * @param  src_len  编码数据长度 (0 为非法)
 * @param  dst      解码输出缓冲区
 * @param  dst_cap  输出缓冲区容量
 * @param  dst_len  输出: 解码后长度
 * @retval true     解码成功
 * @retval false    数据损坏 (含 0x00、code 越界、输出超长或 src_len==0)
 */
bool proto_cobs_decode(const uint8_t *src, size_t src_len,
                       uint8_t *dst, size_t dst_cap, size_t *dst_len);

#ifdef __cplusplus
}
#endif
#endif /* PROTO_COBS_H */
