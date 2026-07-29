/**
 * @file    dx_bt311.h
 * @brief   DX-BT311-10C02S BLE 模块的 AT 命令驱动接口。
 * @details
 * 本驱动将 DX-BT311 的 AT 命令、响应解析和透明数据收发与底层 UART 解耦，
 * 架构与 jdy23.h 对齐，调用者通过 @ref dx_bt311_transport_t 提供写入、逐字节
 * 读取、清空接收缓存、毫秒计时和延时回调。
 *
 * 与 JDY-23 的关键差异：
 * - 响应分隔符为 '=' 而非 ':'（如 +VERSION=V1.0）
 * - 查询响应末尾追加 \r\nOK
 * - 支持设置命令 AT+CMD<value>，返回 +CMD=value\r\nOK 或 OK
 * - 错误格式 EEROR=<code>（手册原文如此），101~104
 * - RESET/DEFAULT 后输出 PowerOn- 标志重启完成
 * - 主机模式 AT+INQ 返回多行设备列表
 * - 连接命令 AT+CONN/CONA/BIND 返回 +Connecting>> + +Connected>>
 *
 * 使用流程：
 * 1. 填写 @ref dx_bt311_transport_t 回调集合；
 * 2. 调用 @ref dx_bt311_init() 绑定实例；
 * 3. 调用 @ref dx_bt311_probe() 检测模块在线；
 * 4. 查询用 @ref dx_bt311_query()，设置用 @ref dx_bt311_set()；
 * 5. 透传数据用 @ref dx_bt311_send() / @ref dx_bt311_receive()。
 *
 * @note  本模块不依赖 DriverLib 或 FreeRTOS，可移植到测试桩。
 * @warning 驱动非线程安全；同一实例不要在多任务中并发发起 AT 事务。
 */
#ifndef DX_BT311_H
#define DX_BT311_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* ======================== 状态码 ======================== */

/**
 * @enum dx_bt311_status_t
 * @brief DX-BT311 驱动操作返回状态。
 */
typedef enum {
    DX_BT311_OK = 0,                         /**< 操作成功。 */
    DX_BT311_ERR_INVALID_PARAM = -1,         /**< 参数为空、缓冲区过小或取值非法。 */
    DX_BT311_ERR_NOT_INIT = -2,              /**< 驱动尚未通过 dx_bt311_init() 初始化。 */
    DX_BT311_ERR_IO = -3,                    /**< 底层传输写入失败。 */
    DX_BT311_ERR_TIMEOUT = -4,               /**< 在规定时间内没有收到任何响应字节。 */
    DX_BT311_ERR_RESPONSE_TOO_LONG = -5,     /**< 响应超过调用者提供的缓冲区容量。 */
    DX_BT311_ERR_UNEXPECTED_RESPONSE = -6,   /**< 收到响应但内容不符合预期格式。 */
    DX_BT311_ERR_MODULE = -7,                /**< 模块返回 EEROR 错误码。 */
    DX_BT311_ERR_CONNECT_FAILED = -8,        /**< 连接流程失败（未收到 +Connected）。 */
    DX_BT311_ERR_NOT_CONFIGURED = -9,         /**< Project has no master target configured. */
} dx_bt311_status_t;

/**
 * @enum dx_bt311_error_code_t
 * @brief DX-BT311 模块返回的错误码（EEROR=<code>）。
 * @note  手册原文拼写为 EEROR，非 ERROR。
 */
typedef enum {
    DX_BT311_ERROR_NONE = 0,                 /**< 无错误。 */
    DX_BT311_ERROR_PARAM_LEN = 101,          /**< 参数长度错误。 */
    DX_BT311_ERROR_PARAM_FORMAT = 102,       /**< 参数格式错误。 */
    DX_BT311_ERROR_PARAM_DATA = 103,         /**< 参数数据异常。 */
    DX_BT311_ERROR_CMD = 104,                /**< 指令错误。 */
    DX_BT311_ERROR_UNKNOWN = 999,            /**< 未知错误码。 */
} dx_bt311_error_code_t;

/* ======================== 行结束符 ======================== */

/**
 * @enum dx_bt311_line_end_t
 * @brief AT 命令末尾的行结束符策略。
 */
typedef enum {
    DX_BT311_LINE_END_NONE = 0,  /**< 不追加任何行结束符。 */
    DX_BT311_LINE_END_CRLF,      /**< 追加回车换行符 \r\n。 */
} dx_bt311_line_end_t;

/* ======================== 命令类型 ======================== */

/**
 * @enum dx_bt311_command_kind_t
 * @brief DX-BT311 命令的语义类别。
 */
typedef enum {
    DX_BT311_KIND_QUERY = 0,    /**< 只读查询：AT+CMD → +CMD=value\r\nOK */
    DX_BT311_KIND_SET,          /**< 设置命令：AT+CMD<value> → +CMD=value\r\nOK 或 OK */
    DX_BT311_KIND_ACTION,       /**< 动作命令：AT+CMD → +CMD\r\nOK 或 OK */
    DX_BT311_KIND_ASYNC,        /**< 异步多行：AT+INQ → OK\r\n+INQS...\r\n+INQEND */
    DX_BT311_KIND_CONNECT,      /**< 连接流程：AT+CONN<n> → +Connecting>>\r\n+Connected>> */
} dx_bt311_command_kind_t;

/**
 * @enum dx_bt311_command_t
 * @brief 驱动内置的 DX-BT311 命令索引。
 * @details
 * 命令按 BLE 模块工作模式分为三类：
 * - 主从通用：ROLE=0 从机和 ROLE=1 主机都可以使用；
 * - 从机专用：只有 ROLE=0 从机模式可以使用；
 * - 主机专用：只有 ROLE=1 主机模式可以使用。
 *
 * 注意：这里的“可以使用”指模块允许执行对应 AT 命令，仍然需要满足
 * AT 指令模式、透传连接状态等额外条件。比如 DISC 虽然属于基础指令，
 * 但手册规定只能在透传模式且已经建立连接后由串口端使用。
 * RESET、DEFAULT、ROLE、以及部分参数设置命令可能触发模块自动重启，
 * 具体以命令注释和 DX-BT311 手册为准。
 */
typedef enum {
    /* --- 主从通用指令：ROLE=0 和 ROLE=1 都可用 --- */
    DX_BT311_CMD_AT = 0,        /**< 通用：测试串口，AT → OK。 */
    DX_BT311_CMD_VERSION,       /**< 通用：查询固件版本，AT+VERSION → +VERSION=<ver>。 */
    DX_BT311_CMD_LADDR,         /**< 通用：查询 MAC 地址，AT+LADDR → +LADDR=<mac>。 */
    DX_BT311_CMD_BAUD,          /**< 通用：查询/设置串口波特率，设置后需重启。 */
    DX_BT311_CMD_POWE,          /**< 通用：查询/设置发射功率，设置后需重启。 */
    DX_BT311_CMD_ROLE,          /**< 通用：查询/设置主从模式；设置后自动重启。 */
    DX_BT311_CMD_RESET,         /**< 通用：软件重启，AT+RESET → +RESET\r\nOK\r\nPowerOn-。 */
    DX_BT311_CMD_DEFAULT,       /**< 通用：恢复出厂设置，执行后自动重启。 */
    DX_BT311_CMD_DISC,          /**< 通用但有条件：仅透传模式且已连接时可用。 */

    /* --- 从机专用指令：只有 ROLE=0 可以使用 --- */
    DX_BT311_CMD_NAME,          /**< 从机专用：设置/查询蓝牙名称。 */
    DX_BT311_CMD_NAMAC,         /**< 从机专用：设置/查询名称是否追加 MAC 后缀。 */
    DX_BT311_CMD_UUID,           /**< 从机专用：设置/查询服务 UUID。 */
    DX_BT311_CMD_CHAR,           /**< 从机专用：设置/查询通知/写入 UUID。 */
    DX_BT311_CMD_WRITE,          /**< 从机专用：设置/查询写入 UUID。 */
    DX_BT311_CMD_PWRM,           /**< 从机专用：进入冬眠模式。 */
    DX_BT311_CMD_NOTI,           /**< 从机专用：设置/查询连接状态通知。 */
    DX_BT311_CMD_ADVI,           /**< 从机专用：设置/查询广播间隔。 */
    DX_BT311_CMD_CLOSEADV,       /**< 从机专用：设置/查询广播开关。 */

    /* --- 主机专用指令：只有 ROLE=1 可以使用 --- */
    DX_BT311_CMD_MUUID,          /**< 主机专用：设置/查询主机服务 UUID。 */
    DX_BT311_CMD_INQ,            /**< 主机专用：搜索附近蓝牙设备。 */
    DX_BT311_CMD_CONN,           /**< 主机专用：按搜索序号连接设备。 */
    DX_BT311_CMD_SCANRSSI,       /**< 主机专用：设置/查询搜索 RSSI 过滤范围。 */
    DX_BT311_CMD_TIMEINQ,        /**< 主机专用：设置/查询搜索时间。 */
    DX_BT311_CMD_CONA,           /**< 主机专用：按 MAC 地址连接设备。 */
    DX_BT311_CMD_BIND,           /**< 主机专用：绑定并自动连接指定 MAC 地址。 */
    DX_BT311_CMD_CLEAR,          /**< 主机专用：清除已绑定的 MAC 地址。 */
    DX_BT311_CMD_COUNT,          /**< 命令数量，边界检查用，不可发送。 */
} dx_bt311_command_t;

/* ======================== 元数据 ======================== */

/**
 * @struct dx_bt311_command_info_t
 * @brief 一条内置命令的元数据。
 */
typedef struct {
    const char *name;                    /**< CLI 短名称，如 "NAME"。 */
    const char *at_command;              /**< AT 命令文本，不含行结束符，如 "AT+NAME"。 */
    const char *response_prefix;         /**< 期望的响应前缀，如 "+NAME="；动作命令可为 NULL。 */
    dx_bt311_command_kind_t kind;        /**< 命令类别。 */
    bool needs_reboot;                   /**< 设置后是否需要重启生效。 */
    bool response_prefix_verified;       /**< 前缀是否已通过目标硬件实测确认。 */
} dx_bt311_command_info_t;

/* ======================== 传输层 ======================== */

/**
 * @struct dx_bt311_transport_t
 * @brief 底层传输回调集合。
 * @details
 * @p read_byte 为非阻塞读取：有字节时写入 @p data 并返回 true，无字节返回 false。
 * @p time_ms 应提供可回绕的无符号毫秒计数，驱动使用无符号减法判断超时。
 */
typedef struct {
    void *context;                                      /**< 回调上下文。 */
    bool (*write)(void *context, const uint8_t *data,
                  uint16_t len);                       /**< 写入一段数据。 */
    bool (*read_byte)(void *context, uint8_t *data);    /**< 非阻塞读取一个字节。 */
    void (*flush_rx)(void *context);                    /**< 丢弃事务前的残留接收数据。 */
    uint32_t (*time_ms)(void *context);                 /**< 返回当前毫秒计数。 */
    void (*delay_ms)(void *context, uint32_t delay_ms); /**< 延时指定毫秒数。 */
} dx_bt311_transport_t;

/* ======================== 驱动实例 ======================== */

/**
 * @struct dx_bt311_t
 * @brief DX-BT311 驱动运行时实例。
 * @note  实例由调用者分配并保持有效，驱动不申请动态内存。
 */
typedef struct {
    dx_bt311_transport_t transport;  /**< 底层传输回调副本。 */
    bool initialized;                 /**< 是否已完成初始化。 */
    bool detected;                    /**< 最近一次探测是否识别到 DX-BT311。 */
} dx_bt311_t;

/* ======================== 主机搜索结果 ======================== */

/**
 * @struct dx_bt311_inq_device_t
 * @brief AT+INQ 搜索到的单个蓝牙设备信息。
 */
typedef struct {
    uint8_t seq;          /**< AT+INQ 列表中的序号（从 1 开始）。 */
    char name[32];        /**< 设备名称。 */
    char mac[13];         /**< MAC 地址（12 字符十六进制 + '\0'）。 */
    int8_t rssi;          /**< 信号强度（负值，越接近 0 越强）。 */
} dx_bt311_inq_device_t;

/* ======================== 公共接口 ======================== */

/**
 * @brief 获取内置命令的元数据。
 * @param command 命令索引，合法范围 [0, DX_BT311_CMD_COUNT)。
 * @return 只读元数据指针；越界返回 NULL。
 */
const dx_bt311_command_info_t *dx_bt311_get_command_info(
    dx_bt311_command_t command);

/**
 * @brief 按名称或完整 AT 文本查找命令。
 * @param name 短名称（如 "NAME"）或 AT 文本（如 "AT+NAME"），大小写不敏感。
 * @param[out] command 输出命令索引。
 * @return 找到返回 true；参数非法或未找到返回 false。
 */
bool dx_bt311_find_command(const char *name,
                            dx_bt311_command_t *command);

/**
 * @brief 初始化驱动实例并绑定底层传输。
 * @param[out] dev 待初始化实例。
 * @param transport 完整的传输回调集合，所有回调不可为 NULL。
 * @return 成功返回 DX_BT311_OK，否则返回参数错误。
 */
dx_bt311_status_t dx_bt311_init(dx_bt311_t *dev,
                                 const dx_bt311_transport_t *transport);

/**
 * @brief 探测模块是否在线。
 * @details 先发送 AT\r\n；若失败，回退发送 AT+VERSION\r\n。
 * @param dev 已初始化实例。
 * @param[out] response 原始响应缓冲区。
 * @param response_size 缓冲区容量，至少 2 字节。
 * @param timeout_ms 单次探测超时，必须 > 0。
 * @return 识别成功返回 DX_BT311_OK。
 */
dx_bt311_status_t dx_bt311_probe(dx_bt311_t *dev, char *response,
                                  uint16_t response_size,
                                  uint32_t timeout_ms);

/**
 * @brief 发送任意 AT 命令并收集原始响应。
 * @param dev 已初始化实例。
 * @param command AT 命令文本，不含行结束符。
 * @param line_end 行结束符策略。
 * @param[out] response 响应缓冲区；返回时始终以 '\0' 结尾。
 * @param response_size 缓冲区容量，至少 2 字节。
 * @param timeout_ms 最大等待时间，必须 > 0。
 * @return 收到部分响应后静默超过约 30ms 视为完成。
 */
dx_bt311_status_t dx_bt311_send_at(dx_bt311_t *dev, const char *command,
                                    dx_bt311_line_end_t line_end,
                                    char *response, uint16_t response_size,
                                    uint32_t timeout_ms);

/**
 * @brief 执行一条查询命令并获取原始响应。
 * @param dev 已初始化实例。
 * @param command 内置命令索引（必须是查询类）。
 * @param[out] response 原始响应缓冲区。
 * @param response_size 缓冲区容量。
 * @param timeout_ms 总超时。
 * @return 传输和收集结果。
 */
dx_bt311_status_t dx_bt311_query(dx_bt311_t *dev,
                                  dx_bt311_command_t command,
                                  char *response, uint16_t response_size,
                                  uint32_t timeout_ms);

/**
 * @brief 执行一条设置命令。
 * @details 自动构建 AT+CMD<value>\r\n 并发送。
 * @param dev 已初始化实例。
 * @param command 内置命令索引（必须是设置类）。
 * @param value 参数值字符串（如 "1234" 或 "0xffe0"）。
 * @param[out] response 可选响应缓冲区，可为 NULL。
 * @param response_size 缓冲区容量。
 * @param timeout_ms 总超时。
 * @return 传输结果；模块返回 EEROR 时为 DX_BT311_ERR_MODULE。
 */
dx_bt311_status_t dx_bt311_set(dx_bt311_t *dev,
                                dx_bt311_command_t command,
                                const char *value,
                                char *response, uint16_t response_size,
                                uint32_t timeout_ms);

/**
 * @brief 从原始响应中提取去前缀后的值。
 * @details 优先按已知前缀（如 "+NAME="）提取等号后的内容；
 *          未找到前缀时回退为提取首行有效文本。
 * @param command 命令索引。
 * @param response 原始响应字符串。
 * @param[out] value 提取结果缓冲区。
 * @param value_size 缓冲区容量，至少 2 字节。
 * @param[out] prefix_matched 可选输出，是否匹配预期前缀。
 * @return 成功返回 DX_BT311_OK。
 */
dx_bt311_status_t dx_bt311_extract_value(dx_bt311_command_t command,
                                          const char *response,
                                          char *value,
                                          uint16_t value_size,
                                          bool *prefix_matched);

/**
 * @brief 从响应文本中解析模块错误码。
 * @param response 原始响应字符串。
 * @return 错误码枚举；无 EEROR 时返回 DX_BT311_ERROR_NONE。
 */
dx_bt311_error_code_t dx_bt311_parse_error(const char *response);

/**
 * @brief 检查响应是否包含 OK 确认。
 * @param response 原始响应字符串。
 * @return 包含 OK 返回 true。
 */
bool dx_bt311_response_has_ok(const char *response);

/**
 * @brief 检查响应是否包含重启完成标志 PowerOn-。
 * @param response 原始响应字符串。
 * @return 包含 PowerOn- 返回 true。
 */
bool dx_bt311_response_has_poweron(const char *response);

/**
 * @brief 搜索蓝牙设备（主机模式）。
 * @param dev 已初始化实例。
 * @param[out] devices 搜索结果数组。
 * @param max_count 数组容量。
 * @param[out] found_count 实际找到的设备数。
 * @param timeout_ms 总超时，建议 ≥ 搜索时间×100ms + 3000ms 余量。
 * @return 传输成功返回 DX_BT311_OK。
 */
dx_bt311_status_t dx_bt311_inquire(dx_bt311_t *dev,
                                    dx_bt311_inq_device_t *devices,
                                    uint8_t max_count,
                                    uint8_t *found_count,
                                    uint32_t timeout_ms);

/**
 * @brief 获取最近一次 AT+INQ 搜索的原始响应文本。
 * @return 指向内部静态缓冲区的指针；未执行过搜索时返回空字符串。
 * @note   返回的指针在下次调用 dx_bt311_inquire() 前一直有效。
 */
const char *dx_bt311_get_last_inq_response(void);

/**
 * @brief 获取最近一次 AT+CONN/CONA 连接的原始响应文本（用于诊断）。
 * @return 指向内部静态缓冲区的指针；未执行过连接时返回空字符串。
 */
const char *dx_bt311_get_last_connect_response(void);

/**
 * @brief 通过序号连接蓝牙设备（主机模式）。
 * @param dev 已初始化实例。
 * @param seq AT+INQ 返回的设备序号。
 * @param[out] mac_out 连接成功的 MAC 地址输出。
 * @param mac_size mac_out 缓冲区容量，至少 13 字节。
 * @param timeout_ms 连接超时。
 * @return 连接成功返回 DX_BT311_OK。
 */
dx_bt311_status_t dx_bt311_connect_by_seq(dx_bt311_t *dev, uint8_t seq,
                                           char *mac_out, uint16_t mac_size,
                                           uint32_t timeout_ms);

/**
 * @brief 通过 MAC 地址连接蓝牙设备（主机模式）。
 * @param dev 已初始化实例。
 * @param mac 12 字符 MAC 地址（如 "112233aabbcc"）。
 * @param[out] mac_out 连接成功的 MAC 地址输出。
 * @param mac_size mac_out 缓冲区容量。
 * @param timeout_ms 连接超时。
 * @return 连接成功返回 DX_BT311_OK。
 */
dx_bt311_status_t dx_bt311_connect_by_addr(dx_bt311_t *dev,
                                            const char *mac,
                                            char *mac_out, uint16_t mac_size,
                                            uint32_t timeout_ms);

/**
 * @brief 通过透明传输通道发送数据。
 * @param dev 已初始化实例。
 * @param data 待发送数据；len 为 0 时可为 NULL。
 * @param len 数据长度。
 * @return 发送成功返回 DX_BT311_OK。
 */
dx_bt311_status_t dx_bt311_send(dx_bt311_t *dev, const uint8_t *data,
                                 uint16_t len);

/**
 * @brief 从透明传输通道读取当前可用数据。
 * @param dev 已初始化实例。
 * @param[out] data 接收缓冲区。
 * @param capacity 缓冲区容量，必须 > 0。
 * @param[out] received 实际读取字节数。
 * @return 读取成功返回 DX_BT311_OK。
 */
dx_bt311_status_t dx_bt311_receive(dx_bt311_t *dev, uint8_t *data,
                                    uint16_t capacity,
                                    uint16_t *received);

/**
 * @brief 返回最近一次探测结果。
 * @param dev 驱动实例；NULL 返回 false。
 * @return 已初始化且最近一次探测成功返回 true。
 */
bool dx_bt311_is_detected(const dx_bt311_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DX_BT311_H */
