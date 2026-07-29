/**
 * @file    proto_frame.c
 * @brief   逻辑帧校验与编码实现 (规范 §3.3, §5.2)
 * @note    帧布局: version(1) | flags(1) | src(1) | dst(1) | msg_class(1) |
 *          opcode(1) | seq(2 LE) | payload_len(2 LE) | payload(0..128) | crc16(2 LE)
 *          CRC 覆盖 version..payload (偏移 0 到 9+N)，不含 CRC 自身。
 *          严禁直接序列化 C struct；使用显式 LE 读写。
 *          不分配堆内存。
 */
#include "proto_frame.h"
#include "proto_crc.h"
#include "proto_cobs.h"
#include <string.h>

/* ========================================================================
 * 帧校验 — 从 COBS 解码后的逻辑帧提取并验证字段
 *
 * 校验顺序 (规范 §5.2):
 *   1. 总长度: 12 <= decoded_len <= 140
 *   2. payload_len 一致性: decoded_len == 12 + payload_len
 *   3. version: == 0x01
 *   4. src/dst: 匹配期望值
 *   5. CRC: 重新计算并比较
 *   6. 填充 frame_view (建立 payload 指针)
 * ======================================================================== */
bool proto_frame_validate(const uint8_t *decoded, size_t decoded_len,
                          uint8_t expected_src, uint8_t expected_dst,
                          proto_frame_view_t *out)
{
    if (decoded == NULL || out == NULL) {
        return false;
    }

    /* 1. 总长度检查: logical_len = FIXED_OVERHEAD + payload_len, 范围 [12, 140] */
    if (decoded_len < PROTO_FIXED_OVERHEAD || decoded_len > PROTO_MAX_DECODED) {
        return false;
    }

    /* 2. payload_len 一致性 */
    uint16_t payload_len = proto_get_u16_le(&decoded[8]);
    if (payload_len > PROTO_MAX_PAYLOAD) {
        return false;
    }
    if (decoded_len != (size_t)(PROTO_FIXED_OVERHEAD + payload_len)) {
        return false;
    }

    /* 3. version 检查 */
    if (decoded[0] != PROTO_VERSION) {
        return false;
    }

    /* 4. 地址检查 */
    if (decoded[2] != expected_src || decoded[3] != expected_dst) {
        return false;
    }

    /* 5. CRC 检查: 覆盖偏移 0 到 9+payload_len (即 decoded_len - 2 字节) */
    size_t crc_region_len = decoded_len - PROTO_CRC_LEN;
    uint16_t computed_crc = proto_crc16_ccitt_false(decoded, crc_region_len);
    uint16_t embedded_crc = proto_get_u16_le(&decoded[decoded_len - PROTO_CRC_LEN]);
    if (computed_crc != embedded_crc) {
        return false;
    }

    /* 6. 填充 frame_view */
    out->version     = decoded[0];
    out->flags       = decoded[1];
    out->src         = decoded[2];
    out->dst         = decoded[3];
    out->msg_class   = decoded[4];
    out->opcode      = decoded[5];
    out->seq         = proto_get_u16_le(&decoded[6]);
    out->payload_len = payload_len;
    out->payload     = (payload_len > 0u) ? &decoded[PROTO_HEADER_LEN] : NULL;
    out->crc16       = embedded_crc;

    return true;
}

/* ========================================================================
 * 构建逻辑帧 — 从 frame_view 组装含 CRC 的逻辑帧
 * ======================================================================== */
/**
 * @brief 构建函数 proto_frame_build_logical，完成对应模块的功能处理。
 * @param frame 函数参数 frame。
 * @param out 函数参数 out。
 * @param out_cap 函数参数 out_cap。
 * @param out_len 函数参数 out_len。
 * @return 函数执行结果。
 */
bool proto_frame_build_logical(const proto_frame_view_t *frame,
                               uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (frame == NULL || out == NULL || out_len == NULL) {
        return false;
    }
    if (frame->payload_len > PROTO_MAX_PAYLOAD) {
        return false;
    }
    if (frame->payload == NULL && frame->payload_len > 0u) {
        return false;
    }

    size_t logical_len = (size_t)PROTO_FIXED_OVERHEAD + frame->payload_len;
    if (out_cap < logical_len) {
        return false;
    }

    /* 写入头字段 (偏移 0-9) */
    out[0] = frame->version;
    out[1] = frame->flags;
    out[2] = frame->src;
    out[3] = frame->dst;
    out[4] = frame->msg_class;
    out[5] = frame->opcode;
    proto_put_u16_le(&out[6], frame->seq);
    proto_put_u16_le(&out[8], frame->payload_len);

    /* 写入 payload (偏移 10..10+N-1) */
    if (frame->payload_len > 0u) {
        memcpy(&out[PROTO_HEADER_LEN], frame->payload, frame->payload_len);
    }

    /* 计算 CRC: 覆盖偏移 0 到 9+N (即 logical_len - 2 字节) */
    size_t crc_region_len = logical_len - PROTO_CRC_LEN;
    uint16_t crc = proto_crc16_ccitt_false(out, crc_region_len);
    proto_put_u16_le(&out[logical_len - PROTO_CRC_LEN], crc);

    *out_len = logical_len;
    return true;
}

/* ========================================================================
 * 构建线上帧 — 逻辑帧 + COBS 编码 + 0x00 分隔符
 * ======================================================================== */
/**
 * @brief 编码函数 proto_frame_encode，完成对应模块的功能处理。
 * @param frame 函数参数 frame。
 * @param wire 函数参数 wire。
 * @param wire_cap 函数参数 wire_cap。
 * @param wire_len 函数参数 wire_len。
 * @return 函数执行结果。
 */
bool proto_frame_encode(const proto_frame_view_t *frame,
                        uint8_t *wire, size_t wire_cap, size_t *wire_len)
{
    if (frame == NULL || wire == NULL || wire_len == NULL) {
        return false;
    }
    if (wire_cap < PROTO_MAX_WIRE) {
        return false;
    }

    /* 1. 构建逻辑帧 */
    uint8_t logical[PROTO_MAX_DECODED];
    size_t logical_len;
    if (!proto_frame_build_logical(frame, logical, sizeof(logical), &logical_len)) {
        return false;
    }

    /* 2. COBS 编码 */
    size_t cobs_len;
    if (!proto_cobs_encode(logical, logical_len,
                           &wire[0], wire_cap - 1u, &cobs_len)) {
        return false;
    }

    /* 3. 追加 0x00 分隔符 */
    wire[cobs_len] = PROTO_DELIMITER;
    *wire_len = cobs_len + 1u;

    return true;
}
