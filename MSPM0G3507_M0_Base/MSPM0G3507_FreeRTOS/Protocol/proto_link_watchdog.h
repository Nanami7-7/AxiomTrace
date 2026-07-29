/**
 * @file    proto_link_watchdog.h
 * @brief   失联 watchdog (规范 §7.3)
 * @note    刷新条件 (全部满足才能刷新):
 *            1. COBS 解码成功  2. 逻辑帧长度正确  3. version 正确
 *            4. src/dst 正确   5. CRC 正确
 *            6. 消息为 HEARTBEAT 或有效控制命令
 *            7. 命令通过权限和参数校验
 *          watchdog_armed 在首次有效 HEARTBEAT/控制命令后置 true。
 *          超过 200ms 未刷新 → LINK_LOST (仅当状态在 DISABLED/ENABLED/RUNNING)。
 *          QUERY/STATUS/ACK/NACK/EVENT 不刷新 watchdog。
 *          时间差使用无符号减法，支持 uint32_t 回绕。
 */
#ifndef PROTO_LINK_WATCHDOG_H
#define PROTO_LINK_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>
#include "proto_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Watchdog 状态
 * ======================================================================== */
typedef struct {
    uint32_t last_valid_control_ms;   /**< 最近一次有效控制刷新时间戳 */
    bool     watchdog_armed;          /**< 首次有效控制后置 true */
    bool     link_lost_latched;       /**< LINK_LOST 锁存标志 */
    uint16_t last_control_seq;        /**< 最近有效控制帧 seq */
    uint32_t timeout_count;           /**< 统计: watchdog 超时次数 */
} proto_watchdog_t;

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief  初始化 watchdog
 */
void proto_watchdog_init(proto_watchdog_t *wd);

/**
 * @brief  刷新 watchdog
 * @param  wd      watchdog 状态
 * @param  now_ms  当前时间戳 (ms)
 * @param  seq     有效控制帧的 seq
 * @note   仅在帧通过全部校验且为 HEARTBEAT 或有效控制命令时调用。
 *         首次刷新会将 watchdog_armed 置 true。
 */
void proto_watchdog_refresh(proto_watchdog_t *wd, uint32_t now_ms, uint16_t seq);

/**
 * @brief  检查 watchdog 是否超时
 * @param  wd      watchdog 状态
 * @param  now_ms  当前时间戳 (ms)
 * @retval true    watchdog 已武装且超过 200ms 未刷新，应触发 LINK_LOST
 * @retval false   未超时或 watchdog 未武装
 * @note   时间差使用无符号减法: (uint32_t)(now_ms - last_valid_control_ms)
 */
bool proto_watchdog_is_timeout(const proto_watchdog_t *wd, uint32_t now_ms);

/**
 * @brief  标记 LINK_LOST 已锁存
 * @note   由 dispatch 在触发 LINK_LOST 状态时调用
 */
void proto_watchdog_set_link_lost(proto_watchdog_t *wd);

/**
 * @brief  清除 LINK_LOST 锁存
 * @note   由 dispatch 在 CLEAR_FAULT(FAULT_LINK_LOST) 成功后调用
 */
void proto_watchdog_clear_link_lost(proto_watchdog_t *wd);

/**
 * @brief  获取距最近有效控制刷新的时间 (ms)
 * @param  wd      watchdog 状态
 * @param  now_ms  当前时间戳
 * @retval 距离毫秒数 (无符号减法，支持回绕)
 */
uint32_t proto_watchdog_age_ms(const proto_watchdog_t *wd, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
#endif /* PROTO_LINK_WATCHDOG_H */
