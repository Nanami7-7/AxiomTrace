/**
 * @file    proto_link_watchdog.c
 * @brief   失联 watchdog 实现 (规范 §7.3)
 * @note    超时阈值: PROTO_WATCHDOG_TIMEOUT_MS (200ms)
 *          时间差使用无符号减法，支持 uint32_t 回绕。
 *          watchdog_armed 在首次有效控制刷新后置 true，此前不触发 LINK_LOST。
 */
#include "proto_link_watchdog.h"
#include <string.h>

/* ========================================================================
 * API 实现
 * ======================================================================== */

void proto_watchdog_init(proto_watchdog_t *wd)
{
    if (wd == NULL) {
        return;
    }
    memset(wd, 0, sizeof(*wd));
}

/**
 * @brief 刷新函数 proto_watchdog_refresh，完成对应模块的功能处理。
 * @param wd 函数参数 wd。
 * @param now_ms 函数参数 now_ms。
 * @param seq 函数参数 seq。
 * @return 函数执行结果。
 */
void proto_watchdog_refresh(proto_watchdog_t *wd, uint32_t now_ms, uint16_t seq)
{
    if (wd == NULL) {
        return;
    }
    wd->last_valid_control_ms = now_ms;
    wd->last_control_seq      = seq;
    wd->watchdog_armed        = true;
}

/**
 * @brief 判断函数 proto_watchdog_is_timeout，完成对应模块的功能处理。
 * @param wd 函数参数 wd。
 * @param now_ms 函数参数 now_ms。
 * @return 函数执行结果。
 */
bool proto_watchdog_is_timeout(const proto_watchdog_t *wd, uint32_t now_ms)
{
    if (wd == NULL || !wd->watchdog_armed) {
        return false;
    }
    /* 无符号减法，支持 uint32_t 回绕 (规范 §3.7) */
    uint32_t elapsed = (uint32_t)(now_ms - wd->last_valid_control_ms);
    return (elapsed > (uint32_t)PROTO_WATCHDOG_TIMEOUT_MS);
}

/**
 * @brief 设置函数 proto_watchdog_set_link_lost，完成对应模块的功能处理。
 * @param wd 函数参数 wd。
 * @return 函数执行结果。
 */
void proto_watchdog_set_link_lost(proto_watchdog_t *wd)
{
    if (wd == NULL) {
        return;
    }
    wd->link_lost_latched = true;
    wd->timeout_count++;
}

/**
 * @brief 清除函数 proto_watchdog_clear_link_lost，完成对应模块的功能处理。
 * @param wd 函数参数 wd。
 * @return 函数执行结果。
 */
void proto_watchdog_clear_link_lost(proto_watchdog_t *wd)
{
    if (wd == NULL) {
        return;
    }
    wd->link_lost_latched = false;
}

/**
 * @brief 执行函数 proto_watchdog_age_ms，完成对应模块的功能处理。
 * @param wd 函数参数 wd。
 * @param now_ms 函数参数 now_ms。
 * @return 函数执行结果。
 */
uint32_t proto_watchdog_age_ms(const proto_watchdog_t *wd, uint32_t now_ms)
{
    if (wd == NULL || !wd->watchdog_armed) {
        return 0u;
    }
    return (uint32_t)(now_ms - wd->last_valid_control_ms);
}
