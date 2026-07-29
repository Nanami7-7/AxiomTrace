/**
 * @file    app_custom_modes.h
 * @brief   Board B OLED 菜单使用的十个自定义协议测试模板。
 *
 * 长按进入 CUST01~CUST10 时，菜单会调用 app_custom_mode_send(0~9)，
 * 通过 Board B UART1 按正常协议事务发送到 Board A。
 */
#ifndef APP_CUSTOM_MODES_H
#define APP_CUSTOM_MODES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_CUSTOM_MODE_COUNT (10U)

/** 发送指定模板，mode_index 范围为 0~9。 */
bool app_custom_mode_send(uint8_t mode_index);

/** 获取模板显示名称、opcode 和最近一次发送结果。 */
const char *app_custom_mode_name(uint8_t mode_index);
uint8_t app_custom_mode_opcode(uint8_t mode_index);
bool app_custom_mode_last_send_ok(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CUSTOM_MODES_H */