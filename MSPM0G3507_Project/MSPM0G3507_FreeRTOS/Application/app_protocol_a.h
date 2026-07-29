/**
 * @file    app_protocol_a.h
 * @brief   Board A UART1 板间协议任务。
 *
 * UART1 只运行 COBS 二进制协议，不输出调试文本。协议任务负责把
 * UART1 字节流交给 COBS 分帧器和协议分发器，ISR 不执行协议解析。
 */
#ifndef APP_PROTOCOL_A_H
#define APP_PROTOCOL_A_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** 初始化 Board A 协议传输、分发器和协议任务。 */
int32_t app_protocol_a_init(void);

/** Board A 协议任务入口，供 FreeRTOS 创建任务使用。 */
void app_protocol_a_task(void *param);

#ifdef __cplusplus
}
#endif
#endif /* APP_PROTOCOL_A_H */