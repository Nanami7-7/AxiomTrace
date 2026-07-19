/**
 * @file    app_test_timer.c
 * @brief   精准秒级测试计时器 (基于 TIMG8 硬件计数器)
 *
 * 硬件基准: TIMG8 (bsp_timer), 提供微秒级计数
 * 计时原理:
 *   1. start() 记录 TIMG8 的起始 ms 时间戳
 *   2. expired() 轮询当前 ms 与起始值之差, 与设定时长比较
 *   3. 首次超时时触发回调 (在调用 expired() 的线程上下文中执行)
 *
 * 优势:
 *   - 零硬件占用 (复用已运行的 TIMG8)
 *   - 无中断冲突 (轮询方式, 无需 ISR)
 *   - 精度 1ms (满足秒级测试需求)
 */

#include "app_test_timer.h"
#include "bsp_timer.h"
#include <stddef.h>  /* NULL */

/* ---- 内部状态 ---- */
static volatile test_timer_state_t s_state     = TEST_TIMER_IDLE;
static volatile uint32_t           s_start_ms  = 0;
static volatile uint32_t           s_duration_ms = 0;
static volatile bool               s_cb_fired  = false;
static test_timer_callback_t       s_callback  = NULL;

/* ---- 接口实现 ---- */

void app_test_timer_set_callback(test_timer_callback_t cb)
{
    s_callback = cb;
}

int app_test_timer_start(uint32_t duration_sec)
{
    if (duration_sec == 0 || duration_sec > 86400UL) {
        return -1;
    }
    if (s_state == TEST_TIMER_RUNNING) {
        return -1;  /* 已有测试运行中 */
    }

    s_start_ms    = bsp_get_ms();
    s_duration_ms = duration_sec * 1000UL;
    s_cb_fired    = false;
    s_state       = TEST_TIMER_RUNNING;
    return 0;
}

bool app_test_timer_expired(void)
{
    if (s_state != TEST_TIMER_RUNNING) {
        return (s_state == TEST_TIMER_EXPIRED);
    }

    uint32_t now    = bsp_get_ms();
    uint32_t elapsed = now - s_start_ms;  /* 32位回绕安全: 减法溢出仍正确 */

    if (elapsed >= s_duration_ms) {
        s_state = TEST_TIMER_EXPIRED;

        /* 首次超时触发回调 */
        if (!s_cb_fired && s_callback) {
            s_cb_fired = true;
            s_callback();
        }
        return true;
    }
    return false;
}

bool app_test_timer_running(void)
{
    /* 顺带检查是否已超时 */
    app_test_timer_expired();
    return (s_state == TEST_TIMER_RUNNING);
}

uint32_t app_test_timer_elapsed_ms(void)
{
    if (s_state == TEST_TIMER_IDLE) {
        return 0;
    }
    if (s_state == TEST_TIMER_EXPIRED) {
        return s_duration_ms;
    }
    return bsp_get_ms() - s_start_ms;
}

uint32_t app_test_timer_remaining_ms(void)
{
    if (s_state != TEST_TIMER_RUNNING) {
        return 0;
    }
    uint32_t elapsed = bsp_get_ms() - s_start_ms;
    if (elapsed >= s_duration_ms) {
        return 0;
    }
    return s_duration_ms - elapsed;
}

uint8_t app_test_timer_progress(void)
{
    if (s_state == TEST_TIMER_IDLE) {
        return 0;
    }
    if (s_state == TEST_TIMER_EXPIRED) {
        return 100;
    }
    uint32_t elapsed = bsp_get_ms() - s_start_ms;
    return (uint8_t)((elapsed * 100UL) / s_duration_ms);
}

void app_test_timer_cleanup(void)
{
    s_state       = TEST_TIMER_IDLE;
    s_start_ms    = 0;
    s_duration_ms = 0;
    s_cb_fired    = false;
    /* 注意: 保留 s_callback, 允许下次复用 */
}
