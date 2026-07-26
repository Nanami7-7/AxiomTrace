/**
 * @file    jdy23.h
 * @brief   JDY-23 BLE 模块的传输无关驱动接口。
 * @details
 * 本驱动将 JDY-23 的 AT 命令、响应解析和透明数据收发与底层 UART 解耦。
 * 调用者通过 @ref jdy23_transport_t 提供写入、逐字节读取、清空接收缓存、
 * 毫秒计时和延时回调，即可在不同 MCU、RTOS 或测试桩上复用本驱动。
 *
 * 使用流程通常为：
 * 1. 准备并填写 @ref jdy23_transport_t；
 * 2. 调用 @ref jdy23_init() 完成驱动绑定；
 * 3. 调用 @ref jdy23_probe() 检测模块；
 * 4. 使用 @ref jdy23_execute_command() 查询配置，或使用
 *    @ref jdy23_send() / @ref jdy23_receive() 进行透明数据通信。
 *
 * @note  AT 命令的响应会被保留在调用者提供的缓冲区中，并始终保证以 '\0' 结尾。
 * @note  本模块不直接依赖 TI DriverLib，也不固定 UART 实例。
 * @warning  驱动接口默认由上层保证串口访问的互斥；不要在多个任务中并发调用
 *            同一个 @ref jdy23_t 实例的命令事务接口。
 */
#ifndef JDY23_H
#define JDY23_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @enum jdy23_status_t
 * @brief JDY-23 驱动操作返回状态。
 */
typedef enum {
    JDY23_OK = 0,                         /**< 操作成功。 */
    JDY23_ERR_INVALID_PARAM = -1,        /**< 参数为空、缓冲区过小或取值非法。 */
    JDY23_ERR_NOT_INIT = -2,             /**< 驱动尚未通过 @ref jdy23_init() 初始化。 */
    JDY23_ERR_IO = -3,                   /**< 底层传输写入失败。 */
    JDY23_ERR_TIMEOUT = -4,              /**< 在规定时间内没有收到任何响应字节。 */
    JDY23_ERR_RESPONSE_TOO_LONG = -5,    /**< 响应超过调用者提供的缓冲区容量。 */
    JDY23_ERR_UNEXPECTED_RESPONSE = -6,  /**< 收到响应，但内容不符合预期格式。 */
} jdy23_status_t;

/**
 * @enum jdy23_line_end_t
 * @brief AT 命令末尾的行结束符策略。
 */
typedef enum {
    JDY23_LINE_END_NONE = 0,  /**< 不追加任何行结束符。 */
    JDY23_LINE_END_CRLF,      /**< 追加回车换行符 "\\r\\n"。 */
} jdy23_line_end_t;

/**
 * @enum jdy23_command_t
 * @brief 驱动内置的 JDY-23 命令索引。
 * @details
 * 查询命令可通过 @ref jdy23_execute_command() 执行并解析；动作命令
 * （RST、DISC、SLEEP）可能导致模块复位、断开或停止响应，建议由上层显式确认后调用。
 * MAJOR 和 MINOR 的命令文本分别为 AT+MAJOR、AT+MINOR，但当前模块固件的响应前缀
 * 通常为 +IBMAJOR: 和 +IBMINOR:。
 */
typedef enum {
    JDY23_COMMAND_VER = 0,  /**< 查询固件版本：AT+VER。 */
    JDY23_COMMAND_RST,      /**< 复位模块：AT+RST。 */
    JDY23_COMMAND_DISC,     /**< 断开当前连接：AT+DISC。 */
    JDY23_COMMAND_STAT,     /**< 查询连接状态：AT+STAT。 */
    JDY23_COMMAND_MAC,      /**< 查询 MAC 地址：AT+MAC。 */
    JDY23_COMMAND_BAUD,     /**< 查询串口波特率配置：AT+BAUD。 */
    JDY23_COMMAND_SLEEP,    /**< 进入休眠：AT+SLEEP。 */
    JDY23_COMMAND_NAME,     /**< 查询模块名称：AT+NAME。 */
    JDY23_COMMAND_STARTEN,  /**< 查询启动广播配置：AT+STARTEN。 */
    JDY23_COMMAND_ADVIN,    /**< 查询广播使能配置：AT+ADVIN。 */
    JDY23_COMMAND_HOSTEN,   /**< 查询主机模式配置：AT+HOSTEN。 */
    JDY23_COMMAND_IBUUID,   /**< 查询 iBeacon UUID：AT+IBUUID。 */
    JDY23_COMMAND_MAJOR,    /**< 查询 iBeacon Major：AT+MAJOR。 */
    JDY23_COMMAND_MINOR,    /**< 查询 iBeacon Minor：AT+MINOR。 */
    JDY23_COMMAND_COUNT,    /**< 命令数量，用于边界检查，不是可发送命令。 */
} jdy23_command_t;

/**
 * @enum jdy23_command_kind_t
 * @brief JDY-23 命令的语义类别。
 */
typedef enum {
    JDY23_COMMAND_KIND_QUERY = 0,  /**< 只读查询，不应改变模块状态。 */
    JDY23_COMMAND_KIND_ACTION,      /**< 有副作用的动作命令。 */
} jdy23_command_kind_t;

/**
 * @struct jdy23_command_info_t
 * @brief 一条内置命令的元数据。
 */
typedef struct {
    const char *name;                    /**< CLI 使用的短名称，例如 "VER"。 */
    const char *at_command;              /**< 实际发送的 AT 命令文本，不含行结束符。 */
    const char *response_prefix;         /**< 期望的响应前缀；动作命令可为 NULL。 */
    jdy23_command_kind_t kind;           /**< 命令类别。 */
    bool response_prefix_verified;       /**< 前缀是否已经通过目标硬件实测确认。 */
} jdy23_command_info_t;

/**
 * @struct jdy23_transport_t
 * @brief JDY-23 驱动使用的底层传输回调集合。
 * @details
 * @p read_byte 应为非阻塞读取：有字节时写入 @p data 并返回 true，
 * 无字节时立即返回 false。@p time_ms 应提供可回绕的无符号毫秒计数，
 * 驱动使用无符号减法进行超时判断。
 */
typedef struct {
    void *context;                                      /**< 回调上下文，由上层传入。 */
    bool (*write)(void *context, const uint8_t *data,
                  uint16_t len);                       /**< 写入一段数据。 */
    bool (*read_byte)(void *context, uint8_t *data);    /**< 非阻塞读取一个字节。 */
    void (*flush_rx)(void *context);                    /**< 丢弃事务开始前的残留接收数据。 */
    uint32_t (*time_ms)(void *context);                 /**< 返回当前毫秒计数。 */
    void (*delay_ms)(void *context, uint32_t delay_ms); /**< 延时指定毫秒数。 */
} jdy23_transport_t;

/**
 * @struct jdy23_t
 * @brief JDY-23 驱动运行时实例。
 * @note  实例由调用者分配并保持有效，驱动不申请动态内存。
 */
typedef struct {
    jdy23_transport_t transport;  /**< 底层传输回调副本。 */
    bool initialized;              /**< 是否已完成初始化。 */
    bool detected;                 /**< 最近一次探测是否识别到 JDY-23。 */
} jdy23_t;

/**
 * @brief 获取内置命令的元数据。
 * @param command 命令索引，合法范围为 [0, JDY23_COMMAND_COUNT)。
 * @return 成功返回只读元数据指针；索引越界返回 NULL。
 */
const jdy23_command_info_t *jdy23_get_command_info(jdy23_command_t command);

/**
 * @brief 按名称或完整 AT 文本查找命令。
 * @param name 命令短名称（如 "VER"）或 AT 文本（如 "AT+VER"），大小写不敏感。
 * @param[out] command 查找到的命令索引。
 * @return 找到命令返回 true；参数非法或未找到返回 false。
 */
bool jdy23_find_command(const char *name, jdy23_command_t *command);

/**
 * @brief 执行一条内置命令并获取原始响应。
 * @param dev 已初始化的驱动实例。
 * @param command 要执行的内置命令。
 * @param[out] response 原始响应缓冲区；成功或部分成功时以 '\0' 结尾。
 * @param response_size 响应缓冲区容量，至少为 2 字节。
 * @param timeout_ms 从发送开始计算的最大等待时间，必须大于 0。
 * @return 详见 @ref jdy23_status_t；无响应超时返回 JDY23_ERR_TIMEOUT。
 */
jdy23_status_t jdy23_execute_command(jdy23_t *dev, jdy23_command_t command,
                                     char *response, uint16_t response_size,
                                     uint32_t timeout_ms);

/**
 * @brief 从原始响应中提取命令值。
 * @details
 * 若命令配置了响应前缀，则优先提取前缀之后至行尾的内容；若未找到前缀，
 * 则回退为提取整段响应的首个有效文本，并通过 @p prefix_matched 报告是否匹配。
 * 函数会去除首尾空白、回车和换行，但不会修改原始响应。
 * @param command 命令索引。
 * @param response 以 '\0' 结尾的原始响应字符串。
 * @param[out] value 提取结果缓冲区。
 * @param value_size 结果缓冲区容量，至少为 2 字节。
 * @param[out] prefix_matched 可选输出；非 NULL 时返回是否匹配预期前缀。
 * @return 成功返回 JDY23_OK；值为空或缓冲区不足时返回对应错误码。
 */
jdy23_status_t jdy23_extract_response_value(jdy23_command_t command,
                                             const char *response,
                                             char *value,
                                             uint16_t value_size,
                                             bool *prefix_matched);

/**
 * @brief 初始化 JDY-23 驱动实例。
 * @param[out] dev 待初始化的实例，生命周期由调用者管理。
 * @param transport 完整的底层传输回调集合，所有回调均不可为 NULL。
 * @return 参数有效并完成绑定返回 JDY23_OK，否则返回 JDY23_ERR_INVALID_PARAM。
 */
jdy23_status_t jdy23_init(jdy23_t *dev, const jdy23_transport_t *transport);

/**
 * @brief 探测 JDY-23 模块是否在线。
 * @details 先发送 AT+VER\\r\\n；若失败，再兼容性回退发送 AT\\r\\n。
 * @param dev 已初始化的驱动实例。
 * @param[out] response 探测命令的原始响应缓冲区。
 * @param response_size 响应缓冲区容量，至少为 2 字节。
 * @param timeout_ms 单次探测命令的超时时间，必须大于 0。
 * @return 识别成功返回 JDY23_OK；无响应、响应不匹配或参数非法时返回错误码。
 */
jdy23_status_t jdy23_probe(jdy23_t *dev, char *response,
                           uint16_t response_size, uint32_t timeout_ms);

/**
 * @brief 发送任意 AT 命令并收集响应。
 * @param dev 已初始化的驱动实例。
 * @param command AT 命令文本，不包含由 @p line_end 指定的行结束符。
 * @param line_end 行结束符策略，目前支持无结束符或 CRLF。
 * @param[out] response 响应缓冲区；函数返回时始终尽力保证以 '\0' 结尾。
 * @param response_size 响应缓冲区容量，至少为 2 字节。
 * @param timeout_ms 最大等待时间，必须大于 0。
 * @return 传输和收集结果；收到部分响应后静默超过约 30 ms 时视为完成。
 */
jdy23_status_t jdy23_send_at(jdy23_t *dev, const char *command,
                             jdy23_line_end_t line_end, char *response,
                             uint16_t response_size, uint32_t timeout_ms);

/**
 * @brief 通过透明传输通道发送数据。
 * @param dev 已初始化的驱动实例。
 * @param data 待发送数据；当 @p len 为 0 时可为 NULL。
 * @param len 数据长度，单位为字节。
 * @return 发送成功返回 JDY23_OK；底层写失败返回 JDY23_ERR_IO。
 */
jdy23_status_t jdy23_send(jdy23_t *dev, const uint8_t *data, uint16_t len);

/**
 * @brief 从透明传输通道读取当前可用数据。
 * @param dev 已初始化的驱动实例。
 * @param[out] data 接收缓冲区。
 * @param capacity 缓冲区容量，必须大于 0。
 * @param[out] received 实际读取字节数，可为 0。
 * @return 读取接口调用成功返回 JDY23_OK；参数非法或未初始化返回错误码。
 */
jdy23_status_t jdy23_receive(jdy23_t *dev, uint8_t *data, uint16_t capacity,
                             uint16_t *received);

/**
 * @brief 返回最近一次探测结果。
 * @param dev 驱动实例；传入 NULL 时返回 false。
 * @return 实例已初始化且最近一次探测成功返回 true，否则返回 false。
 */
bool jdy23_is_detected(const jdy23_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* JDY23_H */
