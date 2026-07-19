#ifndef APP_TEST_TIMER_H
#define APP_TEST_TIMER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file app_test_timer.h
 * @brief 精准秒级测试计时器 (基于 TIMG8 硬件计数器)
 *
 * 硬件基准: TIMG8 (bsp_timer), 已在 main.c 中初始化
 * 计时精度: 1 ms (足够秒级测试控制)
 *
 * 用法:
 *   app_test_timer_start(60);              // 启动 60 秒测试
 *   while (!app_test_timer_expired()) {
 *       // 执行测试逻辑, 每次循环检查超时
 *   }
 *   app_test_timer_cleanup();              // 清理
 *
 * 或带回调:
 *   app_test_timer_set_callback(on_done);  // 注册超时回调
 *   app_test_timer_start(30);
 *   while (app_test_timer_running()) {
 *       // 测试逻辑
 *   }
 */

/* 测试状态 */
typedef enum {
    TEST_TIMER_IDLE = 0,
    TEST_TIMER_RUNNING,
    TEST_TIMER_EXPIRED
} test_timer_state_t;

/* 超时回调函数类型 */
typedef void (*test_timer_callback_t)(void);

/**
 * @brief  注册超时回调函数
 * @param  cb  超时时调用的函数, 传 NULL 取消
 */
void app_test_timer_set_callback(test_timer_callback_t cb);

/**
 * @brief  启动测试计时器
 * @param  duration_sec  测试时长 (秒), 范围 1~86400
 * @return 0=成功, -1=参数无效或已有测试运行中
 */
int app_test_timer_start(uint32_t duration_sec);

/**
 * @brief  检查测试是否超时
 * @return true=已超时, false=仍在运行或未启动
 * @note   若超时且已注册回调, 首次调用时触发回调
 */
bool app_test_timer_expired(void);

/**
 * @brief  检查测试是否仍在运行
 * @return true=运行中, false=未启动/已超时
 */
bool app_test_timer_running(void);

/**
 * @brief  获取已运行时间 (ms)
 * @return 已运行毫秒数, 未启动时返回 0
 */
uint32_t app_test_timer_elapsed_ms(void);

/**
 * @brief  获取剩余时间 (ms)
 * @return 剩余毫秒数, 未启动返回 0, 已超时返回 0
 */
uint32_t app_test_timer_remaining_ms(void);

/**
 * @brief  获取测试进度百分比
 * @return 0~100
 */
uint8_t app_test_timer_progress(void);

/**
 * @brief  终止测试并清理 (正常结束或主动中止)
 * @note   不会触发回调 (回调仅超时自动触发)
 */
void app_test_timer_cleanup(void);

#endif /* APP_TEST_TIMER_H */
