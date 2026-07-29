/**
 * @file    app_dx_ble_service.h
 * @brief   面向应用层的 DX-BT311 BLE 服务接口。
 * @details
 * 本模块将 DX-BT311 协议驱动绑定到 UART2 板级传输层，向菜单、诊断和上层业务
 * 提供统一的 AT 命令、设置、搜索、连接、透明数据和状态查询接口。
 *
 * 与 app_ble_service（JDY-23）的关系：
 * - 共用同一 UART2 硬件（bsp_ble_uart），不可同时初始化；
 * - 通过 PRJ_BLE_MODULE 编译宏在两者间切换；
 * - 接口风格对齐，但 DX-BT311 额外支持 set/inquire/connect。
 *
 * @note 该服务使用一个全局 DX-BT311 实例；同一时刻不要由多个任务并发发起 AT 事务。
 * @warning BLE 接收数据默认不会转发到电机或 VOFA 命令解析器，避免误控风险。
 */
#ifndef APP_DX_BLE_SERVICE_H
#define APP_DX_BLE_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_ble_uart.h"
#include "dx_bt311.h"

/**
 * @brief 应用层 AT 响应缓冲区容量。
 */
#define APP_DX_BLE_RESPONSE_MAX (128U)

/**
 * @brief 应用层 BLE 服务运行状态快照。
 */
typedef struct {
    bool initialized;                   /**< 服务和 UART1 是否初始化成功。 */
    bool detected;                      /**< 最近一次探测是否识别到 DX-BT311。 */
    uint32_t baud_rate;                 /**< UART2 当前配置波特率。 */
    uint32_t rx_pending;                /**< UART2 接收环形缓冲区中的待处理字节数。 */
    bsp_ble_uart_diag_t uart_diag;      /**< BLE UART 诊断计数快照。 */
    uint32_t role;                      /**< 工程期望角色：0=从机，1=主机。 */
    bool config_applied;                /**< 是否已根据工程配置应用 AT 参数。 */
    dx_bt311_status_t last_config_status; /**< 最近一次配置事务状态。 */
    dx_bt311_status_t last_connect_status; /**< 最近一次连接事务状态。 */
} app_dx_ble_status_t;

/**
 * @brief AT 响应值的解析格式。
 */
typedef enum {
    APP_DX_BLE_RESPONSE_FORMAT_NONE = 0,        /**< 尚未解析或没有响应。 */
    APP_DX_BLE_RESPONSE_FORMAT_PREFIX_MATCHED,  /**< 匹配已知响应前缀。 */
    APP_DX_BLE_RESPONSE_FORMAT_RAW_FALLBACK,    /**< 前缀未知，保留原始文本作为回退值。 */
} app_dx_ble_response_format_t;

/**
 * @brief 一次内置 AT 命令事务的完整结果。
 */
typedef struct {
    dx_bt311_status_t transfer_status;                  /**< 发送/接收事务状态。 */
    dx_bt311_status_t parse_status;                     /**< 响应值解析状态。 */
    app_dx_ble_response_format_t format;                 /**< 响应值的来源格式。 */
    dx_bt311_error_code_t module_error;                  /**< 模块返回的错误码（如有）。 */
    char raw_response[APP_DX_BLE_RESPONSE_MAX];          /**< 未修改的原始响应。 */
    char value[APP_DX_BLE_RESPONSE_MAX];                 /**< 去除前缀和空白后的值。 */
} app_dx_ble_command_result_t;

/**
 * @brief 初始化 UART2 BLE 传输层和 DX-BT311 协议服务。
 * @details 该函数只初始化本地资源，不主动发送 AT 命令或阻塞等待模块上线。
 * @return DX_BT311_OK 表示初始化成功；否则返回底层 I/O 或参数错误。
 */
dx_bt311_status_t app_dx_ble_service_init(void);

/**
 * @brief 探测 DX-BT311 模块是否在线。
 * @param[out] response 原始探测响应缓冲区。
 * @param response_size 缓冲区容量，至少为 2 字节。
 * @param timeout_ms 单次探测超时时间，单位为毫秒，必须大于 0。
 * @return 探测状态，成功返回 DX_BT311_OK。
 */
dx_bt311_status_t app_dx_ble_probe(char *response, uint16_t response_size,
                                    uint32_t timeout_ms);

/**
 * @brief 发送任意 AT 命令并收集原始响应。
 * @param command 不含行结束符的命令文本。
 * @param line_end 行结束策略。
 * @param[out] response 原始响应缓冲区。
 * @param response_size 响应缓冲区容量。
 * @param timeout_ms 最大等待时间，单位为毫秒。
 * @return 传输状态。
 */
dx_bt311_status_t app_dx_ble_send_at(const char *command,
                                      dx_bt311_line_end_t line_end,
                                      char *response, uint16_t response_size,
                                      uint32_t timeout_ms);

/**
 * @brief 根据短名称或完整 AT 文本查找内置命令。
 * @param name 待查找名称。
 * @param[out] command 输出命令枚举值。
 * @return 找到命令返回 true，否则返回 false。
 */
bool app_dx_ble_find_command(const char *name,
                              dx_bt311_command_t *command);

/**
 * @brief 获取内置命令的只读元数据。
 * @param command 命令枚举值。
 * @return 命令元数据指针；命令越界时返回 NULL。
 */
const dx_bt311_command_info_t *app_dx_ble_get_command_info(
    dx_bt311_command_t command);

/**
 * @brief 执行内置查询命令并完成响应值解析。
 * @param command 要执行的内置命令。
 * @param[out] result 输出传输状态、解析状态、原始响应和解析值。
 * @param timeout_ms 命令事务超时时间，单位为毫秒。
 * @return 传输状态；解析结果通过 @p result->parse_status 单独报告。
 */
dx_bt311_status_t app_dx_ble_execute_command(dx_bt311_command_t command,
                                              app_dx_ble_command_result_t *result,
                                              uint32_t timeout_ms);

/**
 * @brief 执行内置设置命令。
 * @param command 要执行的内置命令（必须是设置类）。
 * @param value 参数值字符串。
 * @param[out] result 可选结果输出，可为 NULL。
 * @param timeout_ms 命令事务超时时间。
 * @return 传输状态；模块返回 EEROR 时为 DX_BT311_ERR_MODULE。
 */
dx_bt311_status_t app_dx_ble_set_command(dx_bt311_command_t command,
                                          const char *value,
                                          app_dx_ble_command_result_t *result,
                                          uint32_t timeout_ms);

/**
 * @brief 搜索蓝牙设备（主机模式）。
 * @param[out] devices 搜索结果数组。
 * @param max_count 数组容量。
 * @param[out] found_count 实际找到的设备数。
 * @param timeout_ms 总超时。
 * @return 传输成功返回 DX_BT311_OK。
 */
dx_bt311_status_t app_dx_ble_inquire(dx_bt311_inq_device_t *devices,
                                      uint8_t max_count,
                                      uint8_t *found_count,
                                      uint32_t timeout_ms);

/**
 * @brief 获取最近一次 AT+INQ 搜索的原始响应文本（用于诊断）。
 * @return 指向内部静态缓冲区的指针；未执行过搜索时返回空字符串。
 */
const char *app_dx_ble_get_last_inq_response(void);

/**
 * @brief 获取最近一次 AT+CONN/CONA 连接的原始响应文本（用于诊断）。
 * @return 指向内部静态缓冲区的指针；未执行过连接时返回空字符串。
 */
const char *app_dx_ble_get_last_connect_response(void);

/**
 * @brief 通过序号连接蓝牙设备（主机模式）。
 * @param seq AT+INQ 返回的设备序号。
 * @param[out] mac_out 连接成功的 MAC 地址输出。
 * @param mac_size mac_out 缓冲区容量，至少 13 字节。
 * @param timeout_ms 连接超时。
 * @return 连接成功返回 DX_BT311_OK。
 */
dx_bt311_status_t app_dx_ble_connect_by_seq(uint8_t seq,
                                             char *mac_out, uint16_t mac_size,
                                             uint32_t timeout_ms);

/**
 * @brief 通过 MAC 地址连接蓝牙设备（主机模式）。
 * @param mac 12 字符 MAC 地址。
 * @param[out] mac_out 连接成功的 MAC 地址输出。
 * @param mac_size mac_out 缓冲区容量。
 * @param timeout_ms 连接超时。
 * @return 连接成功返回 DX_BT311_OK。
 */
dx_bt311_status_t app_dx_ble_connect_by_addr(const char *mac,
                                              char *mac_out, uint16_t mac_size,
                                              uint32_t timeout_ms);

/**
 * @brief 通过透明传输通道发送数据。
 * @param data 待发送数据；len 为 0 时可为 NULL。
 * @param len 数据长度。
 * @return 发送状态。
 */
dx_bt311_status_t app_dx_ble_send(const uint8_t *data, uint16_t len);

/**
 * @brief 读取透明通道接收数据。
 * @param[out] data 接收缓冲区。
 * @param capacity 缓冲区容量。
 * @param[out] received 实际读取字节数。
 * @return 读取状态。
 */
dx_bt311_status_t app_dx_ble_receive(uint8_t *data, uint16_t capacity,
                                      uint16_t *received);

/**
 * @brief 清空应用层 BLE 接收缓存。
 */
void app_dx_ble_flush_rx(void);

/**
 * @brief 获取应用层 BLE 服务和 UART2 诊断状态。
 * @param[out] status 输出状态快照；传入 NULL 时函数不执行任何操作。
 */
void app_dx_ble_get_status(app_dx_ble_status_t *status);


/**
 * @brief 根据 project_config.h 应用 BLE 配置。
 * @details
 * 只有 PRJ_BLE_APPLY_AT_CONFIG 非 0 时才由启动任务自动调用。
 *         写入可能需要模块复位才能生效；本接口不会自动发送 AT+RESET。
 */
dx_bt311_status_t app_dx_ble_apply_project_config(void);

/**
 * @brief 按 project_config.h 连接主机目标。
 * @details 优先使用 MAC 执行 AT+CONA；未配置 MAC 时搜索名称并执行 AT+CONN。
 *         仅在主机模式下有效；名称匹配采用完整字符串比较。
 */
dx_bt311_status_t app_dx_ble_connect_project_target(void);

/**
 * @brief BLE 服务后台任务。
 * @details 在独立任务中完成 BLE UART 初始化、探测、配置和自动连接。
 *         仅在主机模式下有效；名称匹配采用完整字符串比较。
 */
void app_dx_ble_service_task(void *param);

#ifdef __cplusplus
}
#endif

#endif /* APP_DX_BLE_SERVICE_H */
