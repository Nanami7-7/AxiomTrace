/**
 * @file    dx_bt311.c
 * @brief   DX-BT311-10C02S BLE 模块 AT 命令驱动实现。
 * @details
 * 本文件只实现协议和事务状态机，不直接访问 UART 外设。所有底层 I/O
 * 均通过 dx_bt311_transport_t 回调完成，架构与 jdy23.c 对齐。
 *
 * 响应完成条件：收到至少一个字节后连续约 30ms 无新数据，或达到总超时。
 *
 * 与 jdy23.c 的差异点：
 * - 响应分隔符为 '='（jdy23 为 ':'）
 * - 支持 set 命令（AT+CMD<value>）
 * - 错误码解析（EEROR=<code>）
 * - 重启完成检测（PowerOn-）
 * - 多行响应解析（AT+INQ）
 * - 连接流程状态机（AT+CONN/CONA/BIND）
 */
#include "dx_bt311.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* ======================== 私有常量 ======================== */

/** 收到响应后连续无新字节即认为响应结束的静默时间（ms）。 */
#define DX_BT311_RESPONSE_QUIET_MS (30U)

/** AT+INQ 搜索结果行最大长度。 */
#define DX_BT311_INQ_LINE_MAX (128U)

/** AT+INQ 搜索响应缓冲区大小。 */
#define DX_BT311_INQ_RESPONSE_SIZE (512U)

/** set 命令构建缓冲区大小（AT+CMD + value + CRLF + '\0'）。 */
#define DX_BT311_SET_BUF_SIZE (64U)

/** 连接命令构建缓冲区大小。 */
#define DX_BT311_CONN_BUF_SIZE (32U)

/* ======================== 命令元数据表 ======================== */

/**
 * @brief 内置命令元数据表。
 * @note  response_prefix 以 '=' 结尾（DX-BT311 用等号分隔响应名和值）。
 *        AT 命令、RESET、DEFAULT、DISC、PWRM、INQ、CONN、CONA、BIND、CLEAR
 *        无常规值前缀，设为 NULL。
 *        needs_reboot 标记设置后需要重启生效的命令。
 *        response_prefix_verified 初始为 false，待硬件实测后更新。
 *        命令模式分类：基础命令为主从通用；NAME/UUID 等为从机专用；
 *        MUUID/INQ/CONN 等为主机专用。具体分类以 dx_bt311.h 注释为准。
 */
static const dx_bt311_command_info_t s_command_table[DX_BT311_CMD_COUNT] = {
    /* name,      AT text,         prefix,          kind,            reboot, verified */
    /* --- 主从通用指令：ROLE=0 和 ROLE=1 都可用 --- */
    {"AT",       "AT",            NULL,            DX_BT311_KIND_ACTION, false, false},
    {"VERSION",  "AT+VERSION",    "+VERSION=",     DX_BT311_KIND_QUERY,  false, false},
    {"LADDR",    "AT+LADDR",      "+LADDR=",       DX_BT311_KIND_QUERY,  false, false},
    {"BAUD",     "AT+BAUD",       "+BAUD=",        DX_BT311_KIND_QUERY,  false, false},
    {"POWE",     "AT+POWE",       "+POWE=",        DX_BT311_KIND_QUERY,  false, false},
    {"ROLE",     "AT+ROLE",       "+ROLE=",        DX_BT311_KIND_QUERY,  false, false},
    {"RESET",    "AT+RESET",      "+RESET",        DX_BT311_KIND_ACTION, false, false},
    {"DEFAULT",  "AT+DEFAULT",    "+DEFAULT",      DX_BT311_KIND_ACTION, false, false},
    {"DISC",     "AT+DISC",       NULL,            DX_BT311_KIND_ACTION, false, false},

    /* --- 从机专用指令：只有 ROLE=0 可以使用 --- */
    {"NAME",     "AT+NAME",       "+NAME=",        DX_BT311_KIND_QUERY,  true,  false},
    {"NAMAC",    "AT+NAMAC",      "+NAMAC=",       DX_BT311_KIND_QUERY,  true,  false},
    {"UUID",     "AT+UUID",       "+UUID=",        DX_BT311_KIND_QUERY,  true,  false},
    {"CHAR",     "AT+CHAR",       "+CHAR=",        DX_BT311_KIND_QUERY,  true,  false},
    {"WRITE",    "AT+WRITE",      "+WRITE=",       DX_BT311_KIND_QUERY,  true,  false},
    {"PWRM",     "AT+PWRM",       NULL,            DX_BT311_KIND_ACTION, false, false},
    {"NOTI",     "AT+NOTI",       "+NOTI=",        DX_BT311_KIND_QUERY,  false, false},
    {"ADVI",     "AT+ADVI",       "+ADVI=",        DX_BT311_KIND_QUERY,  false, false},
    {"CLOSEADV", "AT+CLOSEADV",   "+CLOSEADV",     DX_BT311_KIND_QUERY,  true,  false},

    /* --- 主机专用指令：只有 ROLE=1 可以使用 --- */
    {"MUUID",    "AT+MUUID",      "+MUUID=",       DX_BT311_KIND_QUERY,  true,  false},
    {"INQ",      "AT+INQ",        NULL,            DX_BT311_KIND_ASYNC,  false, false},
    {"CONN",     "AT+CONN",       NULL,            DX_BT311_KIND_CONNECT, false, false},
    {"SCANRSSI", "AT+SCANRSSI",   "+SCANRSSI=",    DX_BT311_KIND_QUERY,  false, false},
    {"TIMEINQ",  "AT+TIMEINQ",    "+TIMEINQ=",     DX_BT311_KIND_QUERY,  false, false},
    {"CONA",     "AT+CONA",       NULL,            DX_BT311_KIND_CONNECT, false, false},
    {"BIND",     "AT+BIND",       NULL,            DX_BT311_KIND_CONNECT, false, false},
    {"CLEAR",    "AT+CLEAR",      NULL,            DX_BT311_KIND_ACTION, false, false},
};

/* ======================== 辅助函数 ======================== */

/**
 * @brief ASCII 小写转大写。
 */
static char ascii_upper(char value)
{
    return ((value >= 'a') && (value <= 'z')) ?
           (char)(value - ('a' - 'A')) : value;
}

/**
 * @brief ASCII 大小写不敏感字符串比较。
 */
static bool ascii_equal_ignore_case(const char *lhs, const char *rhs)
{
    if ((lhs == NULL) || (rhs == NULL)) {
        return false;
    }
    while ((*lhs != '\0') && (*rhs != '\0')) {
        if (ascii_upper(*lhs) != ascii_upper(*rhs)) {
            return false;
        }
        lhs++;
        rhs++;
    }
    return ((*lhs == '\0') && (*rhs == '\0'));
}

/**
 * @brief 无符号差值超时判断，兼容毫秒计数器回绕。
 */
static bool timeout_elapsed(uint32_t start, uint32_t now, uint32_t timeout_ms)
{
    return ((uint32_t)(now - start) >= timeout_ms);
}

/**
 * @brief 检查传输回调集合是否完整。
 */
static bool transport_valid(const dx_bt311_transport_t *transport)
{
    return (transport != NULL) &&
           (transport->write != NULL) &&
           (transport->read_byte != NULL) &&
           (transport->flush_rx != NULL) &&
           (transport->time_ms != NULL) &&
           (transport->delay_ms != NULL);
}

/**
 * @brief 检查响应文本是否包含指定子串。
 */
static bool response_contains(const char *response, const char *token)
{
    return (response != NULL) && (token != NULL) &&
           (strstr(response, token) != NULL);
}

/* ======================== 命令表查询 ======================== */

/**
 * @brief 获取函数 dx_bt311_get_command_info，完成对应模块的功能处理。
 * @param command 函数参数 command。
 * @return 函数执行结果。
 */
const dx_bt311_command_info_t *dx_bt311_get_command_info(
    dx_bt311_command_t command)
{
    return ((uint32_t)command < (uint32_t)DX_BT311_CMD_COUNT) ?
           &s_command_table[command] : NULL;
}

/**
 * @brief 查找函数 dx_bt311_find_command，完成对应模块的功能处理。
 * @param name 函数参数 name。
 * @param command 函数参数 command。
 * @return 函数执行结果。
 */
bool dx_bt311_find_command(const char *name,
                            dx_bt311_command_t *command)
{
    uint32_t i;

    if ((name == NULL) || (command == NULL)) {
        return false;
    }

    for (i = 0U; i < (uint32_t)DX_BT311_CMD_COUNT; i++) {
        if (ascii_equal_ignore_case(name, s_command_table[i].name) ||
            ascii_equal_ignore_case(name, s_command_table[i].at_command)) {
            *command = (dx_bt311_command_t)i;
            return true;
        }
    }
    return false;
}

/* ======================== 初始化 ======================== */

/**
 * @brief 初始化函数 dx_bt311_init，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @param transport 函数参数 transport。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_init(dx_bt311_t *dev,
                                 const dx_bt311_transport_t *transport)
{
    if ((dev == NULL) || !transport_valid(transport)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    dev->transport = *transport;
    dev->initialized = true;
    dev->detected = false;
    return DX_BT311_OK;
}

/* ======================== AT 事务引擎 ======================== */

/**
 * @brief 发送函数 dx_bt311_send_at，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @param command 函数参数 command。
 * @param line_end 函数参数 line_end。
 * @param response 函数参数 response。
 * @param response_size 函数参数 response_size。
 * @param timeout_ms 函数参数 timeout_ms。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_send_at(dx_bt311_t *dev, const char *command,
                                    dx_bt311_line_end_t line_end,
                                    char *response, uint16_t response_size,
                                    uint32_t timeout_ms)
{
    uint16_t response_len = 0U;
    uint32_t start;
    uint32_t last_rx;
    bool received_any = false;
    const uint8_t crlf[2] = {'\r', '\n'};

    if ((dev == NULL) || (command == NULL) || (response == NULL) ||
        (response_size < 2U) || (timeout_ms == 0U)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }
    if (!dev->initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }
    if ((line_end != DX_BT311_LINE_END_NONE) &&
        (line_end != DX_BT311_LINE_END_CRLF)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    response[0] = '\0';
    dev->transport.flush_rx(dev->transport.context);

    /* 发送 AT 命令文本 */
    if (!dev->transport.write(dev->transport.context,
                              (const uint8_t *)command,
                              (uint16_t)strlen(command))) {
        return DX_BT311_ERR_IO;
    }
    /* 追加行结束符 */
    if ((line_end == DX_BT311_LINE_END_CRLF) &&
        !dev->transport.write(dev->transport.context, crlf, sizeof(crlf))) {
        return DX_BT311_ERR_IO;
    }

    start = dev->transport.time_ms(dev->transport.context);
    last_rx = start;

    for (;;) {
        uint8_t byte;
        uint32_t now;

        /* 非阻塞读取所有可用字节 */
        while (dev->transport.read_byte(dev->transport.context, &byte)) {
            received_any = true;
            last_rx = dev->transport.time_ms(dev->transport.context);
            if (response_len >= (uint16_t)(response_size - 1U)) {
                response[response_len] = '\0';
                return DX_BT311_ERR_RESPONSE_TOO_LONG;
            }
            response[response_len++] = (char)byte;
            response[response_len] = '\0';
        }

        now = dev->transport.time_ms(dev->transport.context);
        /* 收到数据后静默超 30ms 认为响应结束 */
        if (received_any && timeout_elapsed(last_rx, now,
                                            DX_BT311_RESPONSE_QUIET_MS)) {
            return DX_BT311_OK;
        }
        /* 总超时 */
        if (timeout_elapsed(start, now, timeout_ms)) {
            return received_any ? DX_BT311_OK : DX_BT311_ERR_TIMEOUT;
        }
        dev->transport.delay_ms(dev->transport.context, 1U);
    }
}

/* ======================== 探测 ======================== */

/**
 * @brief 执行函数 dx_bt311_probe，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @param response 函数参数 response。
 * @param response_size 函数参数 response_size。
 * @param timeout_ms 函数参数 timeout_ms。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_probe(dx_bt311_t *dev, char *response,
                                  uint16_t response_size,
                                  uint32_t timeout_ms)
{
    dx_bt311_status_t status;

    if (dev == NULL) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    dev->detected = false;

    /* 优先发送 AT\r\n 测试指令 */
    status = dx_bt311_send_at(dev, "AT", DX_BT311_LINE_END_CRLF,
                              response, response_size, timeout_ms);
    if ((status == DX_BT311_OK) && dx_bt311_response_has_ok(response)) {
        dev->detected = true;
        return DX_BT311_OK;
    }

    /* 回退到 AT+VERSION 探测 */
    status = dx_bt311_send_at(dev, "AT+VERSION", DX_BT311_LINE_END_CRLF,
                              response, response_size, timeout_ms);
    if ((status == DX_BT311_OK) &&
        (response_contains(response, "+VERSION=") ||
         response_contains(response, "BT311"))) {
        dev->detected = true;
        return DX_BT311_OK;
    }

    return (status == DX_BT311_OK) ?
           DX_BT311_ERR_UNEXPECTED_RESPONSE : status;
}

/* ======================== 查询命令 ======================== */

/**
 * @brief 执行函数 dx_bt311_query，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @param command 函数参数 command。
 * @param response 函数参数 response。
 * @param response_size 函数参数 response_size。
 * @param timeout_ms 函数参数 timeout_ms。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_query(dx_bt311_t *dev,
                                  dx_bt311_command_t command,
                                  char *response, uint16_t response_size,
                                  uint32_t timeout_ms)
{
    const dx_bt311_command_info_t *info = dx_bt311_get_command_info(command);

    if (info == NULL) {
        return DX_BT311_ERR_INVALID_PARAM;
    }
    return dx_bt311_send_at(dev, info->at_command, DX_BT311_LINE_END_CRLF,
                            response, response_size, timeout_ms);
}

/* ======================== 设置命令 ======================== */

/**
 * @brief 设置函数 dx_bt311_set，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @param command 函数参数 command。
 * @param value 函数参数 value。
 * @param response 函数参数 response。
 * @param response_size 函数参数 response_size。
 * @param timeout_ms 函数参数 timeout_ms。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_set(dx_bt311_t *dev,
                                dx_bt311_command_t command,
                                const char *value,
                                char *response, uint16_t response_size,
                                uint32_t timeout_ms)
{
    const dx_bt311_command_info_t *info = dx_bt311_get_command_info(command);
    char cmd_buf[DX_BT311_SET_BUF_SIZE];
    uint16_t at_len;
    uint16_t val_len;
    dx_bt311_status_t status;
    char local_response[64];
    char *resp_ptr;

    if (info == NULL) {
        return DX_BT311_ERR_INVALID_PARAM;
    }
    if ((dev == NULL) || (value == NULL)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    /* 构建 AT+CMD<value> */
    at_len = (uint16_t)strlen(info->at_command);
    val_len = (uint16_t)strlen(value);
    if ((at_len + val_len + 1U) > (uint16_t)sizeof(cmd_buf)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    (void)memcpy(cmd_buf, info->at_command, at_len);
    (void)memcpy(cmd_buf + at_len, value, val_len);
    cmd_buf[at_len + val_len] = '\0';

    /* 使用本地缓冲区（调用者可能不需要响应） */
    resp_ptr = (response != NULL) ? response : local_response;
    if (response == NULL) {
        response_size = (uint16_t)sizeof(local_response);
    }

    status = dx_bt311_send_at(dev, cmd_buf, DX_BT311_LINE_END_CRLF,
                              resp_ptr, response_size, timeout_ms);
    if (status != DX_BT311_OK) {
        return status;
    }

    /* 检查模块是否返回错误 */
    if (dx_bt311_parse_error(resp_ptr) != DX_BT311_ERROR_NONE) {
        return DX_BT311_ERR_MODULE;
    }

    return DX_BT311_OK;
}

/* ======================== 响应解析 ======================== */

/**
 * @brief 执行函数 dx_bt311_extract_value，完成对应模块的功能处理。
 * @param command 函数参数 command。
 * @param response 函数参数 response。
 * @param value 函数参数 value。
 * @param value_size 函数参数 value_size。
 * @param prefix_matched 函数参数 prefix_matched。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_extract_value(dx_bt311_command_t command,
                                          const char *response,
                                          char *value,
                                          uint16_t value_size,
                                          bool *prefix_matched)
{
    const dx_bt311_command_info_t *info = dx_bt311_get_command_info(command);
    const char *start;
    const char *end;
    uint16_t length;
    bool matched = false;

    if ((info == NULL) || (response == NULL) || (value == NULL) ||
        (value_size < 2U)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    start = response;
    if (info->response_prefix != NULL) {
        const char *prefixed = strstr(response, info->response_prefix);
        if (prefixed != NULL) {
            start = prefixed + strlen(info->response_prefix);
            matched = true;
        }
    }

    /* 跳过前导空格和制表符（不跳过 \r\n，因为它们标记行尾） */
    while ((*start == ' ') || (*start == '\t')) {
        start++;
    }
    /* 提取到行尾 */
    end = start;
    while ((*end != '\0') && (*end != '\r') && (*end != '\n')) {
        end++;
    }
    /* 去除尾部空白 */
    while ((end > start) &&
           ((end[-1] == ' ') || (end[-1] == '\t'))) {
        end--;
    }

    length = (uint16_t)(end - start);
    if (length == 0U) {
        value[0] = '\0';
        if (prefix_matched != NULL) {
            *prefix_matched = matched;
        }
        return DX_BT311_ERR_UNEXPECTED_RESPONSE;
    }
    if (length >= value_size) {
        value[0] = '\0';
        return DX_BT311_ERR_RESPONSE_TOO_LONG;
    }

    (void)memcpy(value, start, length);
    value[length] = '\0';
    if (prefix_matched != NULL) {
        *prefix_matched = matched;
    }
    return DX_BT311_OK;
}

/* ======================== 错误码解析 ======================== */

/**
 * @brief 解析函数 dx_bt311_parse_error，完成对应模块的功能处理。
 * @param response 函数参数 response。
 * @return 函数执行结果。
 */
dx_bt311_error_code_t dx_bt311_parse_error(const char *response)
{
    const char *pos;
    long code;

    if (response == NULL) {
        return DX_BT311_ERROR_NONE;
    }

    /* 手册原文为 EEROR，同时兼容 ERROR 拼写 */
    pos = strstr(response, "EEROR=");
    if (pos == NULL) {
        pos = strstr(response, "ERROR=");
    }
    if (pos == NULL) {
        return DX_BT311_ERROR_NONE;
    }

    pos += 6U; /* 跳过 "EEROR=" */
    code = strtol(pos, NULL, 10);

    switch (code) {
    case 101: return DX_BT311_ERROR_PARAM_LEN;
    case 102: return DX_BT311_ERROR_PARAM_FORMAT;
    case 103: return DX_BT311_ERROR_PARAM_DATA;
    case 104: return DX_BT311_ERROR_CMD;
    default:  return DX_BT311_ERROR_UNKNOWN;
    }
}

/* ======================== 响应标志检查 ======================== */

/**
 * @brief 执行函数 dx_bt311_response_has_ok，完成对应模块的功能处理。
 * @param response 函数参数 response。
 * @return 函数执行结果。
 */
bool dx_bt311_response_has_ok(const char *response)
{
    return response_contains(response, "OK");
}

/**
 * @brief 执行函数 dx_bt311_response_has_poweron，完成对应模块的功能处理。
 * @param response 函数参数 response。
 * @return 函数执行结果。
 */
bool dx_bt311_response_has_poweron(const char *response)
{
    return response_contains(response, "PowerOn-");
}

/* ======================== 主机搜索 ======================== */

/**
 * @brief AT+INQ 响应缓冲区（static 以避免菜单任务栈溢出）。
 * @note  驱动非线程安全，同一实例不会并发调用，static 缓冲器安全。
 */
static char s_inq_response[DX_BT311_INQ_RESPONSE_SIZE];

/**
 * @brief AT+INQ 单行解析缓冲区（static 以避免菜单任务栈溢出）。
 */
static char s_inq_line_buf[DX_BT311_INQ_LINE_MAX];

/**
 * @brief 解析 AT+INQ 返回的单行设备信息。
 * @details 行格式：<seq><name> <mac><rssi> 或 <seq><name> <mac> <rssi>
 *          序号为开头数字，名称到第一个空格，MAC 为 12 位 hex，RSSI 为末尾负数。
 * @param line 单行文本（不含 \r\n）。
 * @param[out] device 输出解析结果。
 * @return 解析成功返回 true。
 */
static bool parse_inq_line(const char *line, dx_bt311_inq_device_t *device)
{
    const char *p;
    const char *name_start;
    const char *name_end;
    const char *mac_start;
    uint16_t name_len;
    int32_t rssi_val;
    uint16_t i;

    if ((line == NULL) || (device == NULL)) {
        return false;
    }

    /* 跳过 +INQ: / +INQ END: 等标记行 */
    if (line[0] == '+') {
        return false;
    }

    p = line;

    /* 提取序号（开头数字） */
    if ((*p < '0') || (*p > '9')) {
        return false;
    }
    device->seq = (uint8_t)(*p - '0');
    p++;
    while ((*p >= '0') && (*p <= '9')) {
        device->seq = (uint8_t)(device->seq * 10U + (uint8_t)(*p - '0'));
        p++;
    }

    /* 跳过序号后的分隔符（空格或 TAB） */
    while ((*p == ' ') || (*p == '\t')) {
        p++;
    }

    /* 名称：从当前位置到下一个空格或 TAB */
    name_start = p;
    name_end = p;
    while ((*name_end != '\0') && (*name_end != ' ') &&
           (*name_end != '\t') &&
           (*name_end != '\r') && (*name_end != '\n')) {
        name_end++;
    }
    name_len = (uint16_t)(name_end - name_start);
    if (name_len == 0U) {
        return false;
    }
    if (name_len >= (uint16_t)sizeof(device->name)) {
        name_len = (uint16_t)(sizeof(device->name) - 1U);
    }
    (void)memcpy(device->name, name_start, name_len);
    device->name[name_len] = '\0';

    /* 跳过空格 */
    p = name_end;
    while ((*p == ' ') || (*p == '\t')) {
        p++;
    }

    /* MAC：12 位十六进制字符 */
    mac_start = p;
    for (i = 0U; i < 12U; i++) {
        char c = p[i];
        bool is_hex = ((c >= '0') && (c <= '9')) ||
                      ((c >= 'a') && (c <= 'f')) ||
                      ((c >= 'A') && (c <= 'F'));
        if (!is_hex) {
            return false;
        }
    }
    (void)memcpy(device->mac, mac_start, 12U);
    device->mac[12] = '\0';

    /* RSSI：MAC 之后的负数 */
    p = mac_start + 12U;
    while ((*p == ' ') || (*p == '\t')) {
        p++;
    }
    if (*p == '-') {
        rssi_val = 0;
        p++;
        while ((*p >= '0') && (*p <= '9')) {
            rssi_val = rssi_val * 10 + (*p - '0');
            p++;
        }
        device->rssi = (int8_t)(-rssi_val);
    } else {
        device->rssi = 0;
    }

    return true;
}

/**
 * @brief 执行函数 dx_bt311_inquire，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @param devices 函数参数 devices。
 * @param max_count 函数参数 max_count。
 * @param found_count 函数参数 found_count。
 * @param timeout_ms 函数参数 timeout_ms。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_inquire(dx_bt311_t *dev,
                                    dx_bt311_inq_device_t *devices,
                                    uint8_t max_count,
                                    uint8_t *found_count,
                                    uint32_t timeout_ms)
{
    char *response = s_inq_response;
    dx_bt311_status_t status;
    const char *line_start;
    uint8_t count = 0U;
    uint16_t response_len = 0U;
    uint32_t start;
    uint32_t last_rx;
    bool received_any = false;
    const uint8_t crlf[2] = {'\r', '\n'};
    const char *end_marker = "+INQ END:";

    if ((dev == NULL) || (devices == NULL) || (found_count == NULL) ||
        (max_count == 0U)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }
    *found_count = 0U;
    response[0] = '\0';

    /*
     * AT+INQ 是异步多行响应，不能用 send_at 的 30ms 静默判定。
     * 响应流程（实测格式）：
     *   1. 模块立即回复 OK\r\n
     *   2. 搜索期间静默（TIMEINQ × 100ms，默认 1 秒）
     *   3. 搜索完成后发送 +INQ:\r\n + 设备列表 + +INQ END:\r\n
     *   设备行格式：序号\t名称\t\tMAC\tRSSI（TAB 分隔，名称与MAC间有双TAB）
     * 因此需要持续读取直到收到 +INQ END: 终止标记。
     */
    dev->transport.flush_rx(dev->transport.context);

    if (!dev->transport.write(dev->transport.context,
                              (const uint8_t *)"AT+INQ", 6U)) {
        return DX_BT311_ERR_IO;
    }
    if (!dev->transport.write(dev->transport.context, crlf, sizeof(crlf))) {
        return DX_BT311_ERR_IO;
    }

    start = dev->transport.time_ms(dev->transport.context);
    last_rx = start;

    for (;;) {
        uint8_t byte;
        uint32_t now;

        while (dev->transport.read_byte(dev->transport.context, &byte)) {
            received_any = true;
            last_rx = dev->transport.time_ms(dev->transport.context);
            if (response_len >= (uint16_t)(DX_BT311_INQ_RESPONSE_SIZE - 1U)) {
                response[response_len] = '\0';
                /* 缓冲区满，用已收到的部分解析 */
                goto parse_inq;
            }
            response[response_len++] = (char)byte;
            response[response_len] = '\0';

            /* 收到 +INQ END: 表示搜索结果完整 */
            if (strstr(response, end_marker) != NULL) {
                goto parse_inq;
            }
            /* 收到 EEROR 表示命令失败 */
            if (strstr(response, "EEROR=") != NULL) {
                goto parse_inq;
            }
        }

        now = dev->transport.time_ms(dev->transport.context);

        /*
         * 静默判定：收到 OK 后搜索期间会有较长静默。
         * 只有在收到 +INQ END: 之后才用 30ms 静默判定结束。
         * 在此之前只用总超时。
         */
        if (received_any && timeout_elapsed(last_rx, now,
                                            DX_BT311_RESPONSE_QUIET_MS)) {
            /* 如果已经收到 +INQ END: 或 EEROR=，静默是正常的 */
            if ((strstr(response, end_marker) != NULL) ||
                (strstr(response, "EEROR=") != NULL)) {
                goto parse_inq;
            }
            /*
             * 没有 +INQ END: 但静默超过 30ms：
             * 如果已经收到数据（至少 OK），可能是搜索中。
             * 继续等待直到总超时。
             */
        }

        if (timeout_elapsed(start, now, timeout_ms)) {
            status = received_any ? DX_BT311_OK : DX_BT311_ERR_TIMEOUT;
            goto parse_inq;
        }
        dev->transport.delay_ms(dev->transport.context, 1U);
    }

parse_inq:
    status = received_any ? DX_BT311_OK : DX_BT311_ERR_TIMEOUT;

    /* 检查是否有 EEROR */
    if (dx_bt311_parse_error(response) != DX_BT311_ERROR_NONE) {
        return DX_BT311_ERR_MODULE;
    }

    /* 逐行解析设备列表 */
    line_start = response;
    while (*line_start != '\0' && count < max_count) {
        const char *line_end = line_start;

        /* 找到行尾 */
        while ((*line_end != '\0') && (*line_end != '\r') &&
               (*line_end != '\n')) {
            line_end++;
        }

        /* 尝试解析设备行 */
        {
            uint16_t line_len = (uint16_t)(line_end - line_start);
            char *line_buf = s_inq_line_buf;

            if (line_len > 0U &&
                line_len < (uint16_t)DX_BT311_INQ_LINE_MAX) {
                (void)memcpy(line_buf, line_start, line_len);
                line_buf[line_len] = '\0';

                if (parse_inq_line(line_buf, &devices[count])) {
                    count++;
                }
            }
        }

        /* 跳过行尾的 \r\n */
        line_start = line_end;
        while ((*line_start == '\r') || (*line_start == '\n')) {
            line_start++;
        }
    }

    *found_count = count;
    return status;
}

/**
 * @brief 获取函数 dx_bt311_get_last_inq_response，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
const char *dx_bt311_get_last_inq_response(void)
{
    return s_inq_response;
}

/* ======================== 连接流程 ======================== */

/** AT+CONN/CONA 响应缓冲区大小。 */
#define DX_BT311_CONN_RESPONSE_SIZE (256U)

/** 连接响应静态缓冲区（避免栈消耗，同时用于诊断）。 */
static char s_conn_response[DX_BT311_CONN_RESPONSE_SIZE];

/**
 * @brief 从连接响应中提取 MAC 地址。
 * @details 查找 +Connected>> 后的 MAC 地址，兼容 0x 前缀（实测格式）。
 *          实测响应格式：+Connected>>0x98eaa020bae4
 * @param response 原始响应。
 * @param[out] mac_out MAC 输出缓冲区。
 * @param mac_size 缓冲区容量。
 * @return 提取成功返回 true。
 */
static bool extract_connected_mac(const char *response,
                                   char *mac_out, uint16_t mac_size)
{
    const char *pos;
    uint16_t i;

    if ((response == NULL) || (mac_out == NULL) || (mac_size < 13U)) {
        return false;
    }

    pos = strstr(response, "+Connected>>");
    if (pos == NULL) {
        return false;
    }
    pos += 12U; /* 跳过 "+Connected>>" */

    /* 跳过空白 */
    while ((*pos == ' ') || (*pos == '\r') || (*pos == '\n')) {
        pos++;
    }

    /* 兼容 0x 前缀（实测格式：+Connected>>0x98eaa020bae4） */
    if ((pos[0] == '0') && ((pos[1] == 'x') || (pos[1] == 'X'))) {
        pos += 2U;
    }

    /* 提取 12 位 hex MAC */
    for (i = 0U; i < 12U; i++) {
        char c = pos[i];
        bool is_hex = ((c >= '0') && (c <= '9')) ||
                      ((c >= 'a') && (c <= 'f')) ||
                      ((c >= 'A') && (c <= 'F'));
        if (!is_hex) {
            return false;
        }
        mac_out[i] = c;
    }
    mac_out[12] = '\0';
    return true;
}

/**
 * @brief 发送连接命令并持续读取响应直到收到终止标记。
 * @details AT+CONN/CONA 是异步流程：
 *          1. 模块回复 +Connecting>>\r\n
 *          2. 静默数秒等待 BLE 连接建立
 *          3. 连接成功回复 +Connected>>\r\n，或失败回复 +ConnectFailed>>\r\n
 *          send_at 的 30ms 静默判定会在 +Connecting>> 后提前返回，
 *          因此必须用自定义循环等待 +Connected>> / +ConnectFailed>>。
 * @param dev 已初始化实例。
 * @param command 完整 AT 命令文本（不含 CRLF）。
 * @param timeout_ms 总超时。
 * @return 传输状态。
 */
static dx_bt311_status_t dx_bt311_send_connect(dx_bt311_t *dev,
                                                 const char *command,
                                                 uint32_t timeout_ms)
{
    char *response = s_conn_response;
    uint16_t response_len = 0U;
    uint32_t start;
    uint32_t last_rx;
    bool received_any = false;
    bool marker_found = false;
    const uint8_t crlf[2] = {'\r', '\n'};

    if ((dev == NULL) || (command == NULL) || (timeout_ms == 0U)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    response[0] = '\0';
    dev->transport.flush_rx(dev->transport.context);

    /* 发送命令 */
    if (!dev->transport.write(dev->transport.context,
                              (const uint8_t *)command,
                              (uint16_t)strlen(command))) {
        return DX_BT311_ERR_IO;
    }
    if (!dev->transport.write(dev->transport.context, crlf, sizeof(crlf))) {
        return DX_BT311_ERR_IO;
    }

    start = dev->transport.time_ms(dev->transport.context);
    last_rx = start;

    for (;;) {
        uint8_t byte;
        uint32_t now;

        while (dev->transport.read_byte(dev->transport.context, &byte)) {
            received_any = true;
            last_rx = dev->transport.time_ms(dev->transport.context);
            if (response_len >= (uint16_t)(DX_BT311_CONN_RESPONSE_SIZE - 1U)) {
                response[response_len] = '\0';
                return DX_BT311_ERR_RESPONSE_TOO_LONG;
            }
            response[response_len++] = (char)byte;
            response[response_len] = '\0';

            /* 检测终止标记（不立即返回，继续读取完整行） */
            if (!marker_found) {
                if ((strstr(response, "+Connected>>") != NULL) ||
                    (strstr(response, "+ConnectFailed") != NULL) ||
                    (strstr(response, "EEROR=") != NULL)) {
                    marker_found = true;
                }
            }

            /* 标记找到后，等到行尾 \n 再返回，确保 MAC 等后续数据完整 */
            if (marker_found && (byte == '\n')) {
                return DX_BT311_OK;
            }
        }

        now = dev->transport.time_ms(dev->transport.context);

        /* 标记找到后静默 30ms 也认为响应完整 */
        if (marker_found && received_any &&
            timeout_elapsed(last_rx, now, DX_BT311_RESPONSE_QUIET_MS)) {
            return DX_BT311_OK;
        }

        if (timeout_elapsed(start, now, timeout_ms)) {
            return received_any ? DX_BT311_OK : DX_BT311_ERR_TIMEOUT;
        }
        dev->transport.delay_ms(dev->transport.context, 1U);
    }
}

/**
 * @brief 获取函数 dx_bt311_get_last_connect_response，完成对应模块的功能处理。
 * @return 函数执行结果。
 */
const char *dx_bt311_get_last_connect_response(void)
{
    return s_conn_response;
}

/**
 * @brief 执行函数 dx_bt311_connect_by_seq，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @param seq 函数参数 seq。
 * @param mac_out 函数参数 mac_out。
 * @param mac_size 函数参数 mac_size。
 * @param timeout_ms 函数参数 timeout_ms。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_connect_by_seq(dx_bt311_t *dev, uint8_t seq,
                                           char *mac_out, uint16_t mac_size,
                                           uint32_t timeout_ms)
{
    char cmd_buf[DX_BT311_CONN_BUF_SIZE];
    dx_bt311_status_t status;

    if ((dev == NULL) || (mac_out == NULL)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    /* 构建 AT+CONN<seq> */
    {
        uint16_t len = 0U;
        const char *prefix = "AT+CONN";
        (void)memcpy(cmd_buf, prefix, 7U);
        len = 7U;

        if (seq >= 10U) {
            cmd_buf[len++] = (char)('0' + (seq / 10U));
        }
        cmd_buf[len++] = (char)('0' + (seq % 10U));
        cmd_buf[len] = '\0';
    }

    status = dx_bt311_send_connect(dev, cmd_buf, timeout_ms);
    if (status != DX_BT311_OK) {
        return status;
    }

    /* 检查错误 */
    if (dx_bt311_parse_error(s_conn_response) != DX_BT311_ERROR_NONE) {
        return DX_BT311_ERR_MODULE;
    }

    /* 检查连接失败 */
    if (strstr(s_conn_response, "+ConnectFailed") != NULL) {
        return DX_BT311_ERR_CONNECT_FAILED;
    }

    /* 检查连接成功 */
    if (!extract_connected_mac(s_conn_response, mac_out, mac_size)) {
        return DX_BT311_ERR_CONNECT_FAILED;
    }

    return DX_BT311_OK;
}

/**
 * @brief 执行函数 dx_bt311_connect_by_addr，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @param mac 函数参数 mac。
 * @param mac_out 函数参数 mac_out。
 * @param mac_size 函数参数 mac_size。
 * @param timeout_ms 函数参数 timeout_ms。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_connect_by_addr(dx_bt311_t *dev,
                                            const char *mac,
                                            char *mac_out, uint16_t mac_size,
                                            uint32_t timeout_ms)
{
    char cmd_buf[DX_BT311_CONN_BUF_SIZE];
    dx_bt311_status_t status;
    uint16_t mac_len;

    if ((dev == NULL) || (mac == NULL) || (mac_out == NULL)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }
    mac_len = (uint16_t)strlen(mac);
    if (mac_len != 12U) {
        return DX_BT311_ERR_INVALID_PARAM;
    }

    /* 构建 AT+CONA<mac> */
    (void)memcpy(cmd_buf, "AT+CONA", 7U);
    (void)memcpy(cmd_buf + 7U, mac, 12U);
    cmd_buf[19U] = '\0';

    status = dx_bt311_send_connect(dev, cmd_buf, timeout_ms);
    if (status != DX_BT311_OK) {
        return status;
    }

    if (dx_bt311_parse_error(s_conn_response) != DX_BT311_ERROR_NONE) {
        return DX_BT311_ERR_MODULE;
    }

    if (strstr(s_conn_response, "+ConnectFailed") != NULL) {
        return DX_BT311_ERR_CONNECT_FAILED;
    }

    if (!extract_connected_mac(s_conn_response, mac_out, mac_size)) {
        return DX_BT311_ERR_CONNECT_FAILED;
    }

    return DX_BT311_OK;
}

/* ======================== 透明传输 ======================== */

/**
 * @brief 发送函数 dx_bt311_send，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @param data 函数参数 data。
 * @param len 函数参数 len。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_send(dx_bt311_t *dev, const uint8_t *data,
                                 uint16_t len)
{
    if ((dev == NULL) || ((data == NULL) && (len != 0U))) {
        return DX_BT311_ERR_INVALID_PARAM;
    }
    if (!dev->initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }
    if (len == 0U) {
        return DX_BT311_OK;
    }

    return dev->transport.write(dev->transport.context, data, len) ?
           DX_BT311_OK : DX_BT311_ERR_IO;
}

/**
 * @brief 接收函数 dx_bt311_receive，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @param data 函数参数 data。
 * @param capacity 函数参数 capacity。
 * @param received 函数参数 received。
 * @return 函数执行结果。
 */
dx_bt311_status_t dx_bt311_receive(dx_bt311_t *dev, uint8_t *data,
                                    uint16_t capacity,
                                    uint16_t *received)
{
    uint16_t count = 0U;

    if ((dev == NULL) || (data == NULL) || (received == NULL) ||
        (capacity == 0U)) {
        return DX_BT311_ERR_INVALID_PARAM;
    }
    if (!dev->initialized) {
        return DX_BT311_ERR_NOT_INIT;
    }

    while ((count < capacity) &&
           dev->transport.read_byte(dev->transport.context, &data[count])) {
        count++;
    }
    *received = count;
    return DX_BT311_OK;
}

/* ======================== 状态查询 ======================== */

/**
 * @brief 判断函数 dx_bt311_is_detected，完成对应模块的功能处理。
 * @param dev 函数参数 dev。
 * @return 函数执行结果。
 */
bool dx_bt311_is_detected(const dx_bt311_t *dev)
{
    return (dev != NULL) && dev->initialized && dev->detected;
}
