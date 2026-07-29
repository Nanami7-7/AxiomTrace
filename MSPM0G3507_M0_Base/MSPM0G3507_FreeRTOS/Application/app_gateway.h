/**
 * @file    app_gateway.h
 * @brief   Board B 板间 UART1 与 BLE UART2 的协议网关任务。
 *
 * UART1 接 Board A，UART2 接 BLE。两个 UART 各自使用独立 COBS
 * 分帧状态，任务上下文完成解码、路由和发送；中断只负责搬运字节。
 */
#ifndef APP_GATEWAY_H
#define APP_GATEWAY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化 UART1/UART2 协议传输和网关路由器。 */
int32_t app_gateway_init(void);

/**
 * 从 Board B 本地 OLED Mode 发送一个自定义命令到 Board A。
 * 内部仍使用正常的事务、重试和 ACK/NACK 解析；ACK/NACK 不回传 BLE。
 */
bool app_gateway_send_custom_command(uint8_t opcode,
                                      const uint8_t *payload,
                                      uint16_t payload_len);

/** Board B 网关任务入口。 */
void app_gateway_task(void *param);

#ifdef __cplusplus
}
#endif

#endif /* APP_GATEWAY_H */
