/**
 * @file    proto_replay.h
 * @brief   重复包检测与重放缓存 (规范 §10.1)
 * @note    缓存最近 PROTO_REPLAY_CACHE_SIZE 个已执行请求，每项保存:
 *            src, dst, msg_class, opcode, seq, payload_len, payload_crc, result_frame
 *          匹配键: src + seq + msg_class + opcode
 *          冲突判定: 键相同但 payload_crc 不同 → DUPLICATE_CONFLICT
 *          不分配堆内存；result_frame 缓存 ACK/NACK 逻辑帧用于重发。
 */
#ifndef PROTO_REPLAY_H
#define PROTO_REPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "proto_types.h"
#include "proto_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 重复判定结果
 * ======================================================================== */
typedef enum {
    PROTO_REPLAY_NEW        = 0,  /**< 新请求，正常执行 */
    PROTO_REPLAY_DUPLICATE  = 1,  /**< 重复请求，重发缓存结果 */
    PROTO_REPLAY_CONFLICT   = 2,  /**< seq 相同但 payload 不同，冲突 */
} proto_replay_result_t;

/* ========================================================================
 * 重放缓存条目
 * ======================================================================== */
typedef struct {
    bool     used;                          /**< 本槽位是否有效 */
    uint8_t  src;
    uint8_t  dst;
    uint8_t  msg_class;
    uint8_t  opcode;
    uint16_t seq;
    uint16_t payload_len;
    uint16_t payload_crc;                   /**< payload 的 CRC-16，用于冲突检测 */
    uint8_t  result_frame[PROTO_MAX_DECODED]; /**< 缓存的 ACK/NACK 逻辑帧 */
    size_t   result_frame_len;              /**< 缓存逻辑帧长度 */
} proto_replay_entry_t;

/* ========================================================================
 * 重放缓存
 * ======================================================================== */
typedef struct {
    proto_replay_entry_t entries[PROTO_REPLAY_CACHE_SIZE];
    uint8_t  write_idx;                     /**< 环形写入索引 */
    uint32_t hits;                          /**< 统计: 重复命中次数 */
    uint32_t conflicts;                     /**< 统计: 冲突次数 */
} proto_replay_cache_t;

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief  初始化重放缓存
 */
void proto_replay_init(proto_replay_cache_t *cache);

/**
 * @brief  检查帧是否为重复请求
 * @param  cache  重放缓存
 * @param  frame  已校验的帧视图
 * @retval PROTO_REPLAY_NEW       新请求
 * @retval PROTO_REPLAY_DUPLICATE 重复请求 (可通过 proto_replay_get_result 获取缓存结果)
 * @retval PROTO_REPLAY_CONFLICT  冲突 (seq 相同但 payload 不同)
 * @note   匹配键: src + seq + msg_class + opcode
 *         冲突判定: 键相同但 payload_crc 不同
 */
proto_replay_result_t proto_replay_check(const proto_replay_cache_t *cache,
                                         const proto_frame_view_t *frame);

/**
 * @brief  获取重复请求的缓存结果帧
 * @param  cache    重放缓存
 * @param  frame    已校验的帧视图 (用于匹配)
 * @param  out      输出缓冲区 (容量 >= PROTO_MAX_DECODED)
 * @param  out_cap  输出缓冲区容量
 * @param  out_len  输出: 逻辑帧长度
 * @retval true     成功获取缓存结果
 * @retval false    未找到匹配项或缓冲区不足
 */
bool proto_replay_get_result(const proto_replay_cache_t *cache,
                             const proto_frame_view_t *frame,
                             uint8_t *out, size_t out_cap, size_t *out_len);

/**
 * @brief  存储命令执行结果到重放缓存
 * @param  cache         重放缓存
 * @param  frame         原始请求帧视图
 * @param  result_frame  结果逻辑帧 (ACK 或 NACK)
 * @param  result_len    结果逻辑帧长度
 * @note   采用环形覆盖策略，覆盖最旧条目。
 *         payload_crc 由本函数计算。
 */
void proto_replay_store(proto_replay_cache_t *cache,
                        const proto_frame_view_t *frame,
                        const uint8_t *result_frame, size_t result_len);

#ifdef __cplusplus
}
#endif
#endif /* PROTO_REPLAY_H */
