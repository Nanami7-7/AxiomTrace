/**
 * @file    app_custom_modes.c
 * @brief   Board B 十个自定义协议模板的最小测试实现。
 *
 * 业务开发时只需要修改 s_templates[] 中的 opcode/payload，或者直接在
 * app_mode_menu.c 的 USERxx 函数中继续增加业务逻辑。不需要修改 COBS、
 * CRC、UART、事务和 ACK/NACK 代码。
 *
 * ======================== 上位机蓝牙发送说明 ========================
 * BLE UART2 是透明二进制串口，必须发送协议工具生成的 COBS 线上帧，
 * 不能直接发送字符串 "01 01 ..."。推荐运行：
 *   tools/custom_command_generator/custom_command_generator.py --template 1
 *
 * 十个模板的业务定义：
 *   模板01: opcode=0x20, payload=01 55 AA
 *   模板02: opcode=0x21, payload=02 55 AA
 *   模板03: opcode=0x22, payload=03 55 AA
 *   模板04: opcode=0x23, payload=04 55 AA
 *   模板05: opcode=0x24, payload=05 55 AA
 *   模板06: opcode=0x25, payload=06 55 AA
 *   模板07: opcode=0x26, payload=07 55 AA
 *   模板08: opcode=0x27, payload=08 55 AA
 *   模板09: opcode=0x28, payload=09 55 AA
 *   模板10: opcode=0x29, payload=0A 55 AA
 *
 * 上位机帧固定使用：src=0x01、dst=0x20、msg_class=0x01、flags=0x01。
 * Board B OLED 本地测试不经过 BLE，内部使用 src=0x20 发往 Board A。
 */
#include "app_custom_modes.h"
#include "app_gateway.h"
#include <stdio.h>

#define APP_CUSTOM_PAYLOAD_LEN (3U)

typedef struct {
    const char *name;
    uint8_t opcode;
    uint8_t payload[APP_CUSTOM_PAYLOAD_LEN];
} app_custom_template_t;

/*
 * 以后增加/修改模板时，优先只改这个表：
 *   1. opcode 必须在 0x20~0x3F；
 *   2. payload 长度最多 128 字节；
 *   3. Board A 的 app_protocol_user.c 必须使用相同 opcode 解析。
 */
static const app_custom_template_t s_templates[APP_CUSTOM_MODE_COUNT] = {
    { "CUST01", 0x20U, { 0x01U, 0x55U, 0xAAU } },
    { "CUST02", 0x21U, { 0x02U, 0x55U, 0xAAU } },
    { "CUST03", 0x22U, { 0x03U, 0x55U, 0xAAU } },
    { "CUST04", 0x23U, { 0x04U, 0x55U, 0xAAU } },
    { "CUST05", 0x24U, { 0x05U, 0x55U, 0xAAU } },
    { "CUST06", 0x25U, { 0x06U, 0x55U, 0xAAU } },
    { "CUST07", 0x26U, { 0x07U, 0x55U, 0xAAU } },
    { "CUST08", 0x27U, { 0x08U, 0x55U, 0xAAU } },
    { "CUST09", 0x28U, { 0x09U, 0x55U, 0xAAU } },
    { "CUST10", 0x29U, { 0x0AU, 0x55U, 0xAAU } }
};

static bool s_last_send_ok;

bool app_custom_mode_send(uint8_t mode_index)
{
    if (mode_index >= APP_CUSTOM_MODE_COUNT) {
        s_last_send_ok = false;
        return false;
    }

    s_last_send_ok = app_gateway_send_custom_command(
        s_templates[mode_index].opcode,
        s_templates[mode_index].payload,
        APP_CUSTOM_PAYLOAD_LEN);

    printf("[CUSTOM] %s opcode=0x%02X payload=%02X %02X %02X %s\r\n",
           s_templates[mode_index].name,
           (unsigned int)s_templates[mode_index].opcode,
           (unsigned int)s_templates[mode_index].payload[0],
           (unsigned int)s_templates[mode_index].payload[1],
           (unsigned int)s_templates[mode_index].payload[2],
           s_last_send_ok ? "OK" : "FAIL");
    return s_last_send_ok;
}

const char *app_custom_mode_name(uint8_t mode_index)
{
    return (mode_index < APP_CUSTOM_MODE_COUNT) ?
           s_templates[mode_index].name : "INVALID";
}

uint8_t app_custom_mode_opcode(uint8_t mode_index)
{
    return (mode_index < APP_CUSTOM_MODE_COUNT) ?
           s_templates[mode_index].opcode : 0U;
}

bool app_custom_mode_last_send_ok(void)
{
    return s_last_send_ok;
}