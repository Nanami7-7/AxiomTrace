/**
 * @file    app_ble_debug_cli.h
 * @brief   UART0 调试命令转发到 UART2 BLE 模块。
 *
 * 使用方式：
 *   在 UART0 输入：ble AT+ROLE\r\n
 *   或直接输入：AT+ROLE\r\n
 * 本模块只做“读一行、发一条 AT、打印响应”，不实现复杂命令行框架。
 */
#ifndef APP_BLE_DEBUG_CLI_H
#define APP_BLE_DEBUG_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化命令行状态并打印使用说明。 */
void app_ble_debug_cli_init(void);

/**
 * @brief 轮询处理 UART0 中已经收到的字节。
 * @note 必须在已经初始化 BLE 服务的任务上下文中调用。
 */
void app_ble_debug_cli_process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BLE_DEBUG_CLI_H */