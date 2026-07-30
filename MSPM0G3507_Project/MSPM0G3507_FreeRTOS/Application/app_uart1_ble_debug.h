/**
 * @file    app_uart1_ble_debug.h
 * @brief   Board A UART1 蓝牙循迹调试接口。
 *
 * 打开 PRJ_UART1_BLE_DEBUG_ENABLE 后：
 * - UART1 切换为 9600 8N1 透明串口调试，与 AB 板协议互斥；
 * - BLE 可发送 HELP、STATUS、LOG、VIEW、PERIOD、PARAM、CFG 和 SET 文本命令；
 * - 2 ms 控制任务只复制状态快照，格式化和串口发送均在低优先级任务完成；
 * - 本接口不提供电机启动命令，不增加通信看门狗。
 */
#ifndef APP_UART1_BLE_DEBUG_H
#define APP_UART1_BLE_DEBUG_H

#include <stdbool.h>
#include <stdint.h>
#include "app_main.h"
#include "app_line_track.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化 UART1 调试传输和低优先级调试任务。
 *
 * @retval 0   初始化成功或已经初始化。
 * @retval -1  UART1 初始化失败。
 * @retval -2  调试任务创建失败。
 */
int32_t app_uart1_ble_debug_init(void);

/** UART1 中断入口，只把接收字节写入环形缓冲区。 */
void app_uart1_ble_debug_irq_handler(void);

/**
 * 更新最近一次循迹与控制状态快照。
 *
 * 可在 2 ms 控制任务中调用；函数只复制结构体，不格式化字符串、
 * 不发送 UART，也不等待 BLE。
 */
void app_uart1_ble_debug_update_line(uint32_t now_ms,
                                     bool running,
                                     const line_track_output_t *line,
                                     const app_shared_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* APP_UART1_BLE_DEBUG_H */
