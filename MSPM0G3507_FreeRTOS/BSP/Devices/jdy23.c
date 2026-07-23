/**
 * @file    jdy23.c
 * @brief   JDY-23 AT 命令和透明数据通道的驱动实现。
 * @details
 * 本文件只实现协议和事务状态机，不直接访问 UART 外设。所有底层 I/O
 * 均通过 jdy23_transport_t 回调完成，因此可以直接复用到硬件工程和主机测试。
 * 命令事务的完成条件为：收到至少一个字节后连续约 30 ms 无新数据，或达到
 * 调用者指定的总超时时间。响应过长时会保留已收集的前缀并返回错误码。
 */
#include "jdy23.h"
#include <stddef.h>
#include <string.h>

/**
 * @brief 判断响应结束所需的静默时间。
 * @details 收到响应后若连续该时长没有新字节，则认为本次响应已经结束。
 */
#define JDY23_RESPONSE_QUIET_MS (30U)

/**
 * @brief 内置命令元数据表。
 * @note MAJOR/MINOR 的固件响应前缀分别为 +IBMAJOR: 和 +IBMINOR:，
 *       与发送的命令名不完全相同。
 */
static const jdy23_command_info_t s_command_table[JDY23_COMMAND_COUNT] = {
    /* name, AT text, expected response prefix, kind, prefix verified */
    {"VER",     "AT+VER",     "+VER:",     JDY23_COMMAND_KIND_QUERY,  true},
    {"RST",     "AT+RST",     NULL,         JDY23_COMMAND_KIND_ACTION, false},
    {"DISC",    "AT+DISC",    NULL,         JDY23_COMMAND_KIND_ACTION, false},
    {"STAT",    "AT+STAT",    "+STAT:",    JDY23_COMMAND_KIND_QUERY,  true},
    {"MAC",     "AT+MAC",     "+MAC:",     JDY23_COMMAND_KIND_QUERY,  true},
    {"BAUD",    "AT+BAUD",    "+BAUD:",    JDY23_COMMAND_KIND_QUERY,  true},
    {"SLEEP",   "AT+SLEEP",   NULL,         JDY23_COMMAND_KIND_ACTION, false},
    {"NAME",    "AT+NAME",    "+NAME:",    JDY23_COMMAND_KIND_QUERY,  true},
    {"STARTEN", "AT+STARTEN", "+STARTEN:", JDY23_COMMAND_KIND_QUERY,  true},
    {"ADVIN",   "AT+ADVIN",   "+ADVIN:",   JDY23_COMMAND_KIND_QUERY,  true},
    {"HOSTEN",  "AT+HOSTEN",  "+HOSTEN:",  JDY23_COMMAND_KIND_QUERY,  true},
    {"IBUUID",  "AT+IBUUID",  "+IBUUID:",  JDY23_COMMAND_KIND_QUERY,  true},
    {"MAJOR",   "AT+MAJOR",   "+IBMAJOR:", JDY23_COMMAND_KIND_QUERY,  true},
    {"MINOR",   "AT+MINOR",   "+IBMINOR:", JDY23_COMMAND_KIND_QUERY,  true},
};

/**
 * @brief 将 ASCII 小写字母转换为大写。
 * @param value 待转换字符。
 * @return ASCII 小写字母对应的大写字符；其他字符原样返回。
 */
static char ascii_upper(char value)
{
    return ((value >= 'a') && (value <= 'z')) ?
           (char)(value - ('a' - 'A')) : value;
}

/**
 * @brief 按 ASCII 规则比较两个字符串，忽略字母大小写。
 * @param lhs 左侧字符串。
 * @param rhs 右侧字符串。
 * @return 两个非空字符串内容相同返回 true，否则返回 false。
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
 * @brief 获取指定命令的元数据。
 * @param command 命令索引。
 * @return 命令元数据指针；索引越界时返回 NULL。
 */
const jdy23_command_info_t *jdy23_get_command_info(jdy23_command_t command)
{
    return ((uint32_t)command < (uint32_t)JDY23_COMMAND_COUNT) ?
           &s_command_table[command] : NULL;
}

/**
 * @brief 根据短名称或完整 AT 命令查找命令索引。
 * @param name 待查找名称，比较时忽略 ASCII 字母大小写。
 * @param[out] command 查找到的命令索引。
 * @return 找到命令返回 true，否则返回 false。
 */
bool jdy23_find_command(const char *name, jdy23_command_t *command)
{
    uint32_t i;

    if ((name == NULL) || (command == NULL)) {
        return false;
    }

    for (i = 0U; i < (uint32_t)JDY23_COMMAND_COUNT; i++) {
        if (ascii_equal_ignore_case(name, s_command_table[i].name) ||
            ascii_equal_ignore_case(name, s_command_table[i].at_command)) {
            *command = (jdy23_command_t)i;
            return true;
        }
    }
    return false;
}

/**
 * @brief 执行一条内置 AT 命令。
 * @details 该接口自动从命令表获取 AT 文本，并追加 CRLF 后调用通用发送接口。
 * @param dev 已初始化的驱动实例。
 * @param command 内置命令索引。
 * @param[out] response 原始响应缓冲区。
 * @param response_size 响应缓冲区容量。
 * @param timeout_ms 总超时时间，单位为毫秒。
 * @return 发送和接收状态。
 */
jdy23_status_t jdy23_execute_command(jdy23_t *dev, jdy23_command_t command,
                                     char *response, uint16_t response_size,
                                     uint32_t timeout_ms)
{
    const jdy23_command_info_t *info = jdy23_get_command_info(command);

    if (info == NULL) {
        return JDY23_ERR_INVALID_PARAM;
    }
    return jdy23_send_at(dev, info->at_command, JDY23_LINE_END_CRLF,
                         response, response_size, timeout_ms);
}

/**
 * @brief 从原始响应中提取去除前缀和首尾空白后的值。
 * @details 如果配置的响应前缀未出现，则回退为提取整段响应文本，供未知固件
 *          格式的诊断场景保留原始可读值。
 * @param command 命令索引。
 * @param response 原始响应字符串。
 * @param[out] value 提取结果缓冲区。
 * @param value_size 结果缓冲区容量。
 * @param[out] prefix_matched 可选输出，表示是否实际匹配到预期前缀。
 * @return 提取成功或具体错误码。
 */
jdy23_status_t jdy23_extract_response_value(jdy23_command_t command,
                                             const char *response,
                                             char *value,
                                             uint16_t value_size,
                                             bool *prefix_matched)
{
    const jdy23_command_info_t *info = jdy23_get_command_info(command);
    const char *start;
    const char *end;
    uint16_t length;
    bool matched = false;

    if ((info == NULL) || (response == NULL) || (value == NULL) ||
        (value_size < 2U)) {
        return JDY23_ERR_INVALID_PARAM;
    }

    start = response;
    if (info->response_prefix != NULL) {
        const char *prefixed = strstr(response, info->response_prefix);
        if (prefixed != NULL) {
            start = prefixed + strlen(info->response_prefix);
            matched = true;
        }
    }

    while ((*start == ' ') || (*start == '\r') || (*start == '\n') ||
           (*start == '\t')) {
        start++;
    }
    end = start;
    while ((*end != '\0') && (*end != '\r') && (*end != '\n')) {
        end++;
    }
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
        return JDY23_ERR_UNEXPECTED_RESPONSE;
    }
    if (length >= value_size) {
        value[0] = '\0';
        return JDY23_ERR_RESPONSE_TOO_LONG;
    }

    (void)memcpy(value, start, length);
    value[length] = '\0';
    if (prefix_matched != NULL) {
        *prefix_matched = matched;
    }
    return JDY23_OK;
}

/**
 * @brief 检查底层传输回调是否完整。
 * @param transport 待检查的传输回调集合。
 * @return 所有必需回调均存在返回 true，否则返回 false。
 */
static bool transport_valid(const jdy23_transport_t *transport)
{
    return (transport != NULL) &&
           (transport->write != NULL) &&
           (transport->read_byte != NULL) &&
           (transport->flush_rx != NULL) &&
           (transport->time_ms != NULL) &&
           (transport->delay_ms != NULL);
}

/**
 * @brief 使用无符号差值判断超时，兼容毫秒计数器回绕。
 * @param start 起始时间戳。
 * @param now 当前时间戳。
 * @param timeout_ms 超时时间。
 * @return 已经过指定时间返回 true，否则返回 false。
 */
static bool timeout_elapsed(uint32_t start, uint32_t now, uint32_t timeout_ms)
{
    return ((uint32_t)(now - start) >= timeout_ms);
}

/**
 * @brief 初始化 JDY-23 驱动实例并绑定底层传输。
 * @param[out] dev 驱动实例。
 * @param transport 底层传输回调集合。
 * @return 初始化成功返回 JDY23_OK，参数非法返回 JDY23_ERR_INVALID_PARAM。
 */
jdy23_status_t jdy23_init(jdy23_t *dev, const jdy23_transport_t *transport)
{
    if ((dev == NULL) || !transport_valid(transport)) {
        return JDY23_ERR_INVALID_PARAM;
    }

    dev->transport = *transport;
    dev->initialized = true;
    dev->detected = false;
    return JDY23_OK;
}

/**
 * @brief 发送任意 AT 命令并收集原始响应。
 * @details 发送前会清空残留接收数据；收到数据后连续 30 ms 无新字节即完成。
 *          如果在总超时内收到部分数据，函数将返回 JDY23_OK 并保留部分响应。
 * @param dev 已初始化的驱动实例。
 * @param command AT 命令文本，不含行结束符。
 * @param line_end 行结束符策略。
 * @param[out] response 响应缓冲区。
 * @param response_size 响应缓冲区容量。
 * @param timeout_ms 总超时时间，单位为毫秒。
 * @return 传输、超时或响应长度状态。
 */
jdy23_status_t jdy23_send_at(jdy23_t *dev, const char *command,
                             jdy23_line_end_t line_end, char *response,
                             uint16_t response_size, uint32_t timeout_ms)
{
    uint16_t response_len = 0U;
    uint32_t start;
    uint32_t last_rx;
    bool received_any = false;
    const uint8_t crlf[2] = {'\r', '\n'};

    if ((dev == NULL) || (command == NULL) || (response == NULL) ||
        (response_size < 2U) || (timeout_ms == 0U)) {
        return JDY23_ERR_INVALID_PARAM;
    }
    if (!dev->initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    if ((line_end != JDY23_LINE_END_NONE) &&
        (line_end != JDY23_LINE_END_CRLF)) {
        return JDY23_ERR_INVALID_PARAM;
    }

    response[0] = '\0';
    dev->transport.flush_rx(dev->transport.context);

    if (!dev->transport.write(dev->transport.context,
                              (const uint8_t *)command,
                              (uint16_t)strlen(command))) {
        return JDY23_ERR_IO;
    }
    if ((line_end == JDY23_LINE_END_CRLF) &&
        !dev->transport.write(dev->transport.context, crlf, sizeof(crlf))) {
        return JDY23_ERR_IO;
    }

    start = dev->transport.time_ms(dev->transport.context);
    last_rx = start;

    for (;;) {
        uint8_t byte;
        uint32_t now;

        while (dev->transport.read_byte(dev->transport.context, &byte)) {
            received_any = true;
            last_rx = dev->transport.time_ms(dev->transport.context);
            if (response_len >= (uint16_t)(response_size - 1U)) {
                response[response_len] = '\0';
                return JDY23_ERR_RESPONSE_TOO_LONG;
            }
            response[response_len++] = (char)byte;
            response[response_len] = '\0';
        }

        now = dev->transport.time_ms(dev->transport.context);
        if (received_any && timeout_elapsed(last_rx, now,
                                            JDY23_RESPONSE_QUIET_MS)) {
            return JDY23_OK;
        }
        if (timeout_elapsed(start, now, timeout_ms)) {
            return received_any ? JDY23_OK : JDY23_ERR_TIMEOUT;
        }
        dev->transport.delay_ms(dev->transport.context, 1U);
    }
}

/**
 * @brief 判断响应文本是否包含指定令牌。
 * @param response 原始响应文本。
 * @param token 待查找令牌。
 * @return 找到且参数非空返回 true，否则返回 false。
 */
static bool response_contains(const char *response, const char *token)
{
    return (response != NULL) && (token != NULL) &&
           (strstr(response, token) != NULL);
}

/**
 * @brief 校验版本探测响应。
 * @param response AT+VER 的原始响应。
 * @return 响应包含已知 JDY-23 标识时返回 true。
 */
static bool version_response_valid(const char *response)
{
    return response_contains(response, "+VER:") ||
           response_contains(response, "JDY-23") ||
           response_contains(response, "JDY23");
}

/**
 * @brief 校验兼容性 AT 探测响应。
 * @param response 基础 AT 命令的原始响应。
 * @return 响应包含 OK 或 JDY-23 标识时返回 true。
 */
static bool basic_at_response_valid(const char *response)
{
    return response_contains(response, "OK") ||
           response_contains(response, "JDY-23") ||
           response_contains(response, "JDY23");
}

/**
 * @brief 探测模块是否在线并更新检测状态。
 * @details 首先发送 AT+VER\r\n；失败后回退到 AT\r\n，以兼容不同固件版本。
 * @param dev 已初始化的驱动实例。
 * @param[out] response 探测响应缓冲区。
 * @param response_size 响应缓冲区容量。
 * @param timeout_ms 单次命令超时时间，单位为毫秒。
 * @return 探测成功或具体错误码。
 */
jdy23_status_t jdy23_probe(jdy23_t *dev, char *response,
                           uint16_t response_size, uint32_t timeout_ms)
{
    jdy23_status_t status;

    if (dev == NULL) {
        return JDY23_ERR_INVALID_PARAM;
    }

    dev->detected = false;

    /* Current JDY-23 firmware documents CRLF-terminated AT commands. */
    status = jdy23_send_at(dev, "AT+VER", JDY23_LINE_END_CRLF,
                           response, response_size, timeout_ms);
    if ((status == JDY23_OK) && version_response_valid(response)) {
        dev->detected = true;
        return JDY23_OK;
    }

    /* Compatibility fallback for variants accepting the basic AT probe. */
    status = jdy23_send_at(dev, "AT", JDY23_LINE_END_CRLF,
                           response, response_size, timeout_ms);
    if ((status == JDY23_OK) && basic_at_response_valid(response)) {
        dev->detected = true;
        return JDY23_OK;
    }

    return (status == JDY23_OK) ? JDY23_ERR_UNEXPECTED_RESPONSE : status;
}

/**
 * @brief 通过透明数据通道发送一段字节。
 * @param dev 已初始化的驱动实例。
 * @param data 待发送数据；len 为 0 时允许为 NULL。
 * @param len 数据长度，单位为字节。
 * @return 发送成功返回 JDY23_OK，否则返回参数或 I/O 错误。
 */
jdy23_status_t jdy23_send(jdy23_t *dev, const uint8_t *data, uint16_t len)
{
    if ((dev == NULL) || ((data == NULL) && (len != 0U))) {
        return JDY23_ERR_INVALID_PARAM;
    }
    if (!dev->initialized) {
        return JDY23_ERR_NOT_INIT;
    }
    if (len == 0U) {
        return JDY23_OK;
    }

    return dev->transport.write(dev->transport.context, data, len) ?
           JDY23_OK : JDY23_ERR_IO;
}

/**
 * @brief 读取透明数据通道当前已经到达的字节。
 * @details 该接口不会等待数据，读取到当前无数据即返回；最多读取 capacity 字节。
 * @param dev 已初始化的驱动实例。
 * @param[out] data 接收缓冲区。
 * @param capacity 接收缓冲区容量。
 * @param[out] received 实际读取字节数。
 * @return 读取接口调用成功返回 JDY23_OK，否则返回参数或初始化错误。
 */
jdy23_status_t jdy23_receive(jdy23_t *dev, uint8_t *data, uint16_t capacity,
                             uint16_t *received)
{
    uint16_t count = 0U;

    if ((dev == NULL) || (data == NULL) || (received == NULL) ||
        (capacity == 0U)) {
        return JDY23_ERR_INVALID_PARAM;
    }
    if (!dev->initialized) {
        return JDY23_ERR_NOT_INIT;
    }

    while ((count < capacity) &&
           dev->transport.read_byte(dev->transport.context, &data[count])) {
        count++;
    }
    *received = count;
    return JDY23_OK;
}

/**
 * @brief 查询最近一次模块探测结果。
 * @param dev 驱动实例。
 * @return 已初始化且最近一次探测成功返回 true，否则返回 false。
 */
bool jdy23_is_detected(const jdy23_t *dev)
{
    return (dev != NULL) && dev->initialized && dev->detected;
}
