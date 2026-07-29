/**
 * @file    proto_replay.c
 * @brief   重复包检测与重放缓存实现 (规范 §10.1)
 * @note    匹配键: src + seq + msg_class + opcode
 *          冲突判定: 键相同但 payload_crc 不同 → DUPLICATE_CONFLICT
 *          环形覆盖策略，缓存最近 PROTO_REPLAY_CACHE_SIZE 项。
 *          不分配堆内存。
 */
#include "proto_replay.h"
#include "proto_crc.h"
#include <string.h>

/* ========================================================================
 * 内部工具: 计算 payload 的 CRC-16 用于冲突检测
 * ======================================================================== */
static uint16_t replay_compute_payload_crc(const proto_frame_view_t *frame)
{
    if (frame->payload_len == 0u || frame->payload == NULL) {
        return 0u;
    }
    return proto_crc16_ccitt_false(frame->payload, frame->payload_len);
}

/* ========================================================================
 * 内部工具: 匹配键比较
 * ======================================================================== */
/**
 * @brief 执行函数 replay_key_match，完成对应模块的功能处理。
 * @param entry 函数参数 entry。
 * @param frame 函数参数 frame。
 * @return 函数执行结果。
 */
static bool replay_key_match(const proto_replay_entry_t *entry,
                             const proto_frame_view_t *frame)
{
    return (entry->used &&
            entry->src       == frame->src &&
            entry->seq       == frame->seq &&
            entry->msg_class == frame->msg_class &&
            entry->opcode    == frame->opcode);
}

/* ========================================================================
 * API 实现
 * ======================================================================== */

/**
 * @brief 初始化函数 proto_replay_init，完成对应模块的功能处理。
 * @param cache 函数参数 cache。
 * @return 函数执行结果。
 */
void proto_replay_init(proto_replay_cache_t *cache)
{
    if (cache == NULL) {
        return;
    }
    memset(cache, 0, sizeof(*cache));
}

/**
 * @brief 检查函数 proto_replay_check，完成对应模块的功能处理。
 * @param cache 函数参数 cache。
 * @param frame 函数参数 frame。
 * @return 函数执行结果。
 */
proto_replay_result_t proto_replay_check(const proto_replay_cache_t *cache,
                                         const proto_frame_view_t *frame)
{
    if (cache == NULL || frame == NULL) {
        return PROTO_REPLAY_NEW;
    }

    uint16_t payload_crc = replay_compute_payload_crc(frame);

    for (uint8_t i = 0u; i < PROTO_REPLAY_CACHE_SIZE; ++i) {
        const proto_replay_entry_t *e = &cache->entries[i];
        if (!e->used) {
            continue;
        }
        if (!replay_key_match(e, frame)) {
            continue;
        }
        /* 键匹配: 检查 payload_crc */
        if (e->payload_crc == payload_crc &&
            e->payload_len == frame->payload_len) {
            return PROTO_REPLAY_DUPLICATE;
        }
        return PROTO_REPLAY_CONFLICT;
    }

    return PROTO_REPLAY_NEW;
}

/**
 * @brief 获取函数 proto_replay_get_result，完成对应模块的功能处理。
 * @param cache 函数参数 cache。
 * @param frame 函数参数 frame。
 * @param out 函数参数 out。
 * @param out_cap 函数参数 out_cap。
 * @param out_len 函数参数 out_len。
 * @return 函数执行结果。
 */
bool proto_replay_get_result(const proto_replay_cache_t *cache,
                             const proto_frame_view_t *frame,
                             uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (cache == NULL || frame == NULL || out == NULL || out_len == NULL) {
        return false;
    }

    for (uint8_t i = 0u; i < PROTO_REPLAY_CACHE_SIZE; ++i) {
        const proto_replay_entry_t *e = &cache->entries[i];
        if (!e->used || !replay_key_match(e, frame)) {
            continue;
        }
        if (out_cap < e->result_frame_len) {
            return false;
        }
        memcpy(out, e->result_frame, e->result_frame_len);
        *out_len = e->result_frame_len;
        return true;
    }
    return false;
}

/**
 * @brief 执行函数 proto_replay_store，完成对应模块的功能处理。
 * @param cache 函数参数 cache。
 * @param frame 函数参数 frame。
 * @param result_frame 函数参数 result_frame。
 * @param result_len 函数参数 result_len。
 * @return 函数执行结果。
 */
void proto_replay_store(proto_replay_cache_t *cache,
                        const proto_frame_view_t *frame,
                        const uint8_t *result_frame, size_t result_len)
{
    if (cache == NULL || frame == NULL || result_frame == NULL) {
        return;
    }
    if (result_len > PROTO_MAX_DECODED) {
        return;
    }

    /* 环形覆盖: 写入 write_idx 指向的槽位 */
    proto_replay_entry_t *slot = &cache->entries[cache->write_idx];

    slot->used       = true;
    slot->src        = frame->src;
    slot->dst        = frame->dst;
    slot->msg_class  = frame->msg_class;
    slot->opcode     = frame->opcode;
    slot->seq        = frame->seq;
    slot->payload_len = frame->payload_len;
    slot->payload_crc = replay_compute_payload_crc(frame);

    memcpy(slot->result_frame, result_frame, result_len);
    slot->result_frame_len = result_len;

    /* 推进写入索引 (环形) */
    cache->write_idx = (uint8_t)((cache->write_idx + 1u) % PROTO_REPLAY_CACHE_SIZE);
}
