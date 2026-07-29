/**
 * @file    app_ble_debug_cli.c
 * @brief   UART0 -> UART2 BLE AT 调试命令转发实现。
 */
#include "app_ble_debug_cli.h"
#include "app_dx_ble_service.h"
#include "bsp_debug.h"
#include "dx_bt311.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define APP_BLE_DEBUG_CLI_LINE_MAX     (96U)
#define APP_BLE_DEBUG_CLI_RESPONSE_MAX (256U)
#define APP_BLE_DEBUG_CLI_TIMEOUT_MS   (1200U)

static char s_line[APP_BLE_DEBUG_CLI_LINE_MAX];
static uint16_t s_line_len;
static bool s_initialized;

/* UART2 响应缓冲放在静态区，避免 BLE CLI 占用任务栈。 */
static char s_response[APP_BLE_DEBUG_CLI_RESPONSE_MAX];

/** 跳过命令首部空格。 */
static char *cli_skip_spaces(char *text)
{
    while ((*text == ' ') || (*text == '\t')) {
        text++;
    }
    return text;
}

/** 删除命令末尾空格。 */
static void cli_trim_end(char *text)
{
    size_t len = strlen(text);
    while ((len > 0U) &&
           ((text[len - 1U] == ' ') || (text[len - 1U] == '\t'))) {
        text[--len] = '\0';
    }
}

/** 判断是否是不区分大小写的 ble 前缀。 */
static bool cli_has_ble_prefix(const char *text)
{
    char a;
    char b;
    char c;

    if ((text[0] == '\0') || (text[1] == '\0') || (text[2] == '\0')) {
        return false;
    }
    a = (text[0] >= 'A' && text[0] <= 'Z') ?
        (char)(text[0] - 'A' + 'a') : text[0];
    b = (text[1] >= 'A' && text[1] <= 'Z') ?
        (char)(text[1] - 'A' + 'a') : text[1];
    c = (text[2] >= 'A' && text[2] <= 'Z') ?
        (char)(text[2] - 'A' + 'a') : text[2];
    return (a == 'b') && (b == 'l') && (c == 'e') &&
           ((text[3] == ' ') || (text[3] == '\t') || (text[3] == '\0'));
}

/** 打印固定的最小帮助信息。 */
static void cli_print_help(void)
{
    printf("[BLE-CLI] 用法：\r\n");
    printf("  ble AT+ROLE       查询角色\r\n");
    printf("  ble AT+ROLE0      切换为从机并自动重启\r\n");
    printf("  ble AT+NAMEONB    设置从机名称\r\n");
    printf("  ble AT+LADDR      查询 MAC 地址\r\n");
    printf("  ble AT+DISC       主机已连接时先断开连接\r\n");
    printf("  ble AT+RESET      重启模块\r\n");
    printf("  也可以直接输入 AT+ROLE0，不加 ble 前缀\r\n");
    printf("  注意：已连接透传时，先执行 AT+DISC，等待1秒后再执行 AT+ROLE0\r\n");
}

/** 发送一条 UART0 输入的 AT 命令并打印响应。 */
static void cli_send_at(char *command)
{
    dx_bt311_status_t status;

    command = cli_skip_spaces(command);
    cli_trim_end(command);
    if (command[0] == '\0') {
        return;
    }

    if (cli_has_ble_prefix(command)) {
        command = cli_skip_spaces(command + 3);
    }

    if ((command[0] == '\0') ||
        ((command[0] != 'A') && (command[0] != 'a'))) {
        printf("[BLE-CLI] 只允许发送 AT 命令，请输入 ble AT+ROLE\r\n");
        return;
    }
    if ((command[1] != 'T') && (command[1] != 't')) {
        printf("[BLE-CLI] 命令必须以 AT 开头\r\n");
        return;
    }

    s_response[0] = '\0';
    printf("[BLE-CLI] TX: %s\r\n", command);
    status = app_dx_ble_send_at(command,
                                 DX_BT311_LINE_END_CRLF,
                                 s_response,
                                 (uint16_t)sizeof(s_response),
                                 APP_BLE_DEBUG_CLI_TIMEOUT_MS);
    printf("[BLE-CLI] status=%d RX=%s\r\n",
           (int)status,
           (s_response[0] != '\0') ? s_response : "<无响应>");

    /* 已连接时模块可能处于透传状态，AT 查询不会返回正常响应。 */
    if (((status == DX_BT311_ERR_TIMEOUT) ||
         (status == DX_BT311_ERR_MODULE)) &&
        ((strcmp(command, "AT+ROLE") == 0) ||
         (strncmp(command, "AT+ROLE", 7U) == 0))) {
        printf("[BLE-CLI] 提示：模块可能已进入透传连接，请先输入 ble AT+DISC，等待1秒后再重试。\r\n");
    }
}

/** 处理一条完整的 UART0 文本行。 */
static void cli_process_line(void)
{
    char *command = cli_skip_spaces(s_line);
    char *help_command = command;

    /*
     * 先打印收到的原始整行。
     * 这样可以区分“串口0没有收到命令”和“收到命令但发送到串口2失败”。
     */
    printf("[BLE-CLI] LINE=<%s>\r\n", command);

    /* 允许输入“ble help”，同时保留直接输入“help”的方式。 */
    if (cli_has_ble_prefix(help_command)) {
        help_command = cli_skip_spaces(help_command + 3);
    }

    if ((strcmp(help_command, "help") == 0) ||
        (strcmp(help_command, "?") == 0) ||
        (help_command[0] == '\0')) {
        cli_print_help();
    } else {
        /* 传原始整行，cli_send_at() 内部会处理 ble 前缀。 */
        cli_send_at(command);
    }
    s_line_len = 0U;
    s_line[0] = '\0';
}

void app_ble_debug_cli_init(void)
{
    s_line_len = 0U;
    s_line[0] = '\0';
    s_initialized = true;
    printf("[BLE-CLI] UART0 控制 UART2 已启用\r\n");
    printf("[BLE-CLI] 输入 ble help 查看命令，输入 ble AT+ROLE0 切换从机模式\r\n");
}

void app_ble_debug_cli_process(void)
{
    uint8_t data;

    if (!s_initialized) {
        return;
    }

    while (bsp_debug_getc(&data) == BSP_OK) {
        if ((data == '\r') || (data == '\n')) {
            if (s_line_len > 0U) {
                cli_process_line();
            }
        } else if ((data == '\b') || (data == 0x7FU)) {
            if (s_line_len > 0U) {
                s_line[--s_line_len] = '\0';
            }
        } else if (data >= 0x20U) {
            if (s_line_len < (APP_BLE_DEBUG_CLI_LINE_MAX - 1U)) {
                s_line[s_line_len++] = (char)data;
                s_line[s_line_len] = '\0';
            } else {
                printf("[BLE-CLI] 命令过长，已丢弃当前行\r\n");
                s_line_len = 0U;
                s_line[0] = '\0';
            }
        }
    }
}