/**
 * @file    app_ble_service.h
 * @brief   面向应用层的 JDY-23 BLE 服务接口。
 * @details
 * 本模块将 JDY-23 协议驱动绑定到 UART1 板级传输层，向菜单、诊断和上层业务
 * 提供统一的 AT 命令、透明数据和状态查询接口。应用层不直接访问 DriverLib、
 * UART1 寄存器或底层环形缓冲区。
 * @note 该服务使用一个全局 JDY-23 实例；同一时刻不要由多个任务并发发起 AT 事务。
 * @warning BLE 接收数据默认不会转发到电机或 VOFA 命令解析器，避免误控风险。
 */
#ifndef APP_BLE_SERVICE_H
#define APP_BLE_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_ble_uart.h"
#include "jdy23.h"

/**
 * @brief 应用层 AT 响应缓冲区容量。
 * @details 同时用于原始响应和解析后的值，单位为字节，包含结尾的 '\0'。
 */
#define APP_BLE_RESPONSE_MAX (128U)

/**
 * @brief 应用层 BLE 服务运行状态快照。
 */
typedef struct {
    bool initialized;                 /**< 服务和 UART1 是否初始化成功。 */
    bool detected;                    /**< 最近一次探测是否识别到 JDY-23。 */
    uint32_t baud_rate;              /**< UART1 当前配置波特率。 */
    uint32_t rx_pending;             /**< UART1 接收环形缓冲区中的待处理字节数。 */
    bsp_ble_uart_diag_t uart_diag;   /**< UART1 接收统计及溢出诊断信息。 */
} app_ble_status_t;

/**
 * @brief AT 响应值的解析格式。
 */
typedef enum {
    APP_BLE_RESPONSE_FORMAT_NONE = 0,       /**< 尚未解析或没有响应。 */
    APP_BLE_RESPONSE_FORMAT_PREFIX_MATCHED, /**< 匹配已知响应前缀。 */
    APP_BLE_RESPONSE_FORMAT_RAW_FALLBACK,   /**< 前缀未知，保留原始文本作为回退值。 */
} app_ble_response_format_t;

/**
 * @brief 一次内置 AT 命令事务的完整结果。
 */
typedef struct {
    jdy23_status_t transfer_status;          /**< 发送/接收事务状态。 */
    jdy23_status_t parse_status;             /**< 响应值解析状态。 */
    app_ble_response_format_t format;        /**< 响应值的来源格式。 */
    char raw_response[APP_BLE_RESPONSE_MAX]; /**< 未修改的原始响应。 */
    char value[APP_BLE_RESPONSE_MAX];        /**< 去除前缀和空白后的值。 */
} app_ble_command_result_t;

/**
 * @brief 初始化 UART1 BLE 传输层和 JDY-23 协议服务。
 * @details 该函数只初始化本地资源，不主动发送 AT 命令或阻塞等待模块上线。
 * @return JDY23_OK 表示初始化成功；否则返回底层 I/O 或参数错误。
 */
jdy23_status_t app_ble_service_init(void);

/**
 * @brief 探测 JDY-23 模块是否在线。
 * @param[out] response 原始探测响应缓冲区。
 * @param response_size 缓冲区容量，至少为 2 字节。
 * @param timeout_ms 单次探测超时时间，单位为毫秒，必须大于 0。
 * @return 探测状态，成功返回 JDY23_OK。
 */
jdy23_status_t app_ble_probe(char *response, uint16_t response_size,
                             uint32_t timeout_ms);

/**
 * @brief 发送任意 AT 命令并收集原始响应。
 * @param command 不含行结束符的命令文本。
 * @param line_end 行结束策略，可选 JDY23_LINE_END_NONE 或 JDY23_LINE_END_CRLF。
 * @param[out] response 原始响应缓冲区。
 * @param response_size 响应缓冲区容量，至少为 2 字节。
 * @param timeout_ms 最大等待时间，单位为毫秒，必须大于 0。
 * @return 传输状态。
 */
jdy23_status_t app_ble_send_at(const char *command, jdy23_line_end_t line_end,
                               char *response, uint16_t response_size,
                               uint32_t timeout_ms);

/**
 * @brief 根据短名称或完整 AT 文本查找内置命令。
 * @param name 待查找名称，例如 "VER" 或 "AT+VER"。
 * @param[out] command 输出命令枚举值。
 * @return 找到命令返回 true，否则返回 false。
 */
bool app_ble_find_command(const char *name, jdy23_command_t *command);

/**
 * @brief 获取内置命令的只读元数据。
 * @param command 命令枚举值。
 * @return 命令元数据指针；命令越界时返回 NULL。
 */
const jdy23_command_info_t *app_ble_get_command_info(jdy23_command_t command);

/**
 * @brief 执行内置 AT 命令并完成响应值解析。
 * @param command 要执行的内置命令。
 * @param[out] result 输出传输状态、解析状态、原始响应和解析值。
 * @param timeout_ms 命令事务超时时间，单位为毫秒，必须大于 0。
 * @return 传输状态；解析结果通过 @p result->parse_status 单独报告。
 */
jdy23_status_t app_ble_execute_command(jdy23_command_t command,
                                       app_ble_command_result_t *result,
                                       uint32_t timeout_ms);

/**
 * @brief 通过 JDY-23 透明通道发送数据。
 * @param data 待发送数据；当 len 为 0 时可为 NULL。
 * @param len 数据长度，单位为字节。
 * @return 发送状态。
 */
jdy23_status_t app_ble_send(const uint8_t *data, uint16_t len);

/**
 * @brief 读取当前已缓存的透明通道数据。
 * @param[out] data 接收缓冲区。
 * @param capacity 接收缓冲区容量，必须大于 0。
 * @param[out] received 实际读取字节数。
 * @return 读取状态；没有数据时返回 JDY23_OK 且 received 为 0。
 */
jdy23_status_t app_ble_receive(uint8_t *data, uint16_t capacity,
                               uint16_t *received);

/**
 * @brief 丢弃 UART1 接收缓冲区中的所有待处理数据。
 */
void app_ble_flush_rx(void);

/**
 * @brief 获取应用层 BLE 服务和 UART1 诊断状态。
 * @param[out] status 输出状态快照；传入 NULL 时函数不执行任何操作。
 */
void app_ble_get_status(app_ble_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* APP_BLE_SERVICE_H */
