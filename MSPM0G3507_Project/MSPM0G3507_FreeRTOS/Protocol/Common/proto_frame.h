/**
 * @file    proto_frame.h
 * @brief   逻辑帧校验与编码 (规范 §3.3, §5.2)
 * @note    帧布局: version(1) | flags(1) | src(1) | dst(1) | msg_class(1) |
 *          opcode(1) | seq(2 LE) | payload_len(2 LE) | payload(0..128) | crc16(2 LE)
 *          CRC 覆盖 version..payload (不含 CRC 自身)。
 *          严禁直接序列化 C struct；必须通过显式 LE 读写函数。
 *          不分配堆内存。
 */
#ifndef PROTO_FRAME_H
#define PROTO_FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "proto_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Little-endian 显式读写函数 (规范 §5.1)
 * ======================================================================== */

static inline void proto_put_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static inline uint16_t proto_get_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline void proto_put_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline uint32_t proto_get_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static inline void proto_put_i32_le(uint8_t *p, int32_t v)
{
    proto_put_u32_le(p, (uint32_t)v);
}

static inline int32_t proto_get_i32_le(const uint8_t *p)
{
    return (int32_t)proto_get_u32_le(p);
}

static inline void proto_put_i16_le(uint8_t *p, int16_t v)
{
    proto_put_u16_le(p, (uint16_t)v);
}

static inline int16_t proto_get_i16_le(const uint8_t *p)
{
    return (int16_t)proto_get_u16_le(p);
}

/* ========================================================================
 * 帧校验 — 从 COBS 解码后的逻辑帧提取并验证字段 (规范 §5.2)
 * ======================================================================== */

/**
 * @brief  校验逻辑帧并填充 frame_view
 * @param  decoded       COBS 解码后的逻辑帧数据
 * @param  decoded_len   逻辑帧长度
 * @param  expected_src  期望的源地址 (如 Board A 收: PROTO_ADDR_GATEWAY)
 * @param  expected_dst  期望的目的地址 (如 Board A 收: PROTO_ADDR_CONTROLLER)
 * @param  out           输出: 解析后的帧视图 (payload 指向 decoded 内部)
 * @retval true          校验通过
 * @retval false         长度/版本/地址/CRC 任一失败
 * @note   必须先验证总长度，再建立 payload 指针。
 *         out->payload 指向 decoded 缓冲区内部，调用方需保持其有效期。
 */
bool proto_frame_validate(const uint8_t *decoded, size_t decoded_len,
                          uint8_t expected_src, uint8_t expected_dst,
                          proto_frame_view_t *out);

/* ========================================================================
 * 帧编码 — 从 frame_view 构建逻辑帧 (规范 §5.2)
 * ======================================================================== */

/**
 * @brief  从 frame_view 构建逻辑帧 (含 CRC，不含 COBS)
 * @param  frame      帧视图 (需填充 version..payload_len 和 payload)
 * @param  out        输出缓冲区 (容量 >= PROTO_MAX_DECODED)
 * @param  out_cap    输出缓冲区容量
 * @param  out_len    输出: 逻辑帧长度
 * @retval true       构建成功
 * @retval false      参数非法或 payload_len 超限
 * @note   CRC 由本函数计算并追加，忽略 frame->crc16 字段。
 */
bool proto_frame_build_logical(const proto_frame_view_t *frame,
                               uint8_t *out, size_t out_cap, size_t *out_len);

/* ========================================================================
 * 帧编码 — 从 frame_view 构建完整线上帧 (规范 §5.2)
 * ======================================================================== */

/**
 * @brief  从 frame_view 构建线上帧 (COBS 编码 + 0x00 分隔符)
 * @param  frame      帧视图
 * @param  wire       输出缓冲区 (容量 >= PROTO_MAX_WIRE)
 * @param  wire_cap   输出缓冲区容量
 * @param  wire_len   输出: 线上帧长度
 * @retval true       编码成功
 * @retval false      参数非法或缓冲区不足
 * @note   内部调用 proto_frame_build_logical + proto_cobs_encode。
 *         CRC 由本函数计算，忽略 frame->crc16 字段。
 */
bool proto_frame_encode(const proto_frame_view_t *frame,
                        uint8_t *wire, size_t wire_cap, size_t *wire_len);

#ifdef __cplusplus
}
#endif
#endif /* PROTO_FRAME_H */
