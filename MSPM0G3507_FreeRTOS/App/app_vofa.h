/**
 * @file    app_vofa.h
 * @brief   VOFA+ 通信协议接口
 * @note    支持 VOFA+ 的两种数据帧格式:
 *          1. FireWater (文本): "ch0:val0 ch1:val1 ... chN:valN\n"
 *             - 可读性好, 适合调试
 *             - 支持下行命令解析(VOFA+ 控件发送)
 *          2. JustFloat (二进制): float float ... float 0x00 0x00 0x80 0x7F
 *             - 传输效率高, 适合高速数据流
 *             - 帧尾 0x0000807F 标识帧结束
 *
 *          使用方式:
 *          - 在 RUNNING 状态下周期调用 app_vofa_send_*() 输出数据
 *          - 通过 app_vofa_parse_cmd() 解析下行命令实现远程调参
 */
#ifndef APP_VOFA_H
#define APP_VOFA_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include <stdint.h>
#include <stdbool.h>
#include "app_main.h"

/* ======================== 常量定义 ======================== */

/** VOFA+ JustFloat 帧尾标记 (IEEE 754: 0x7F800000 的小端表示) */
#define VOFA_JUSTFLOAT_TAIL_0   (0x00U)
#define VOFA_JUSTFLOAT_TAIL_1   (0x00U)
#define VOFA_JUSTFLOAT_TAIL_2   (0x80U)
#define VOFA_JUSTFLOAT_TAIL_3   (0x7FU)

/** FireWater 最大单通道数值字符串长度 */
#define VOFA_FIREWATER_CH_MAX_LEN  (32U)

/** 下行命令最大长度 */
#define VOFA_CMD_MAX_LEN           (64U)

/* ======================== 类型定义 ======================== */

/** VOFA+ 帧格式 */
typedef enum {
    VOFA_FORMAT_FIREWATER = 0,  /**< FireWater 文本格式 */
    VOFA_FORMAT_JUSTFLOAT,       /**< JustFloat 二进制格式 */
} vofa_format_t;

/** VOFA+ 下行命令类型 */
typedef enum {
    VOFA_CMD_NONE = 0,      /**< 无命令 */
    VOFA_CMD_SET_KP,        /**< 设置比例增益: Kp=x */
    VOFA_CMD_SET_KI,        /**< 设置积分增益: Ki=x */
    VOFA_CMD_SET_KD,        /**< 设置微分增益: Kd=x */
    VOFA_CMD_SET_TARGET,    /**< 设置目标RPM: Target=x */
    VOFA_CMD_SET_MOTOR,     /**< 选择电机: Motor=x (0-3) */
    VOFA_CMD_RUN,           /**< 启动电机: Run 或 Run=0 */
    VOFA_CMD_STOP,          /**< 停止电机: Stop 或 Stop=0 */
    VOFA_CMD_STOP_ALL,      /**< 停止所有电机: StopAll */
} vofa_cmd_type_t;

/** VOFA+ 解析后的命令结构 */
typedef struct {
    vofa_cmd_type_t type;       /**< 命令类型 */
    float           value;      /**< 数值参数(如有) */
    uint32_t        motor_id;   /**< 目标电机索引(如有) */
    bool            has_value;  /**< 是否携带数值 */
    bool            has_motor;  /**< 是否指定电机 */
} vofa_cmd_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  发送 VOFA+ FireWater 格式数据
 * @note   输出格式: "ch0:val0 ch1:val1 ... chN:valN\n"
 *         通过 printf -> fputc -> bsp_uart_putc 发送
 * @param  channels  通道数值数组
 * @param  count     通道数量
 */
void app_vofa_send_firewater(const float channels[], uint32_t count);

/**
 * @brief  发送 VOFA+ JustFloat 格式数据
 * @note   输出格式: float[N] + 0x00 0x00 0x80 0x7F
 *         直接通过 bsp_uart_putc 逐字节发送(二进制)
 * @param  channels  通道数值数组(IEEE 754 单精度)
 * @param  count     通道数量
 */
void app_vofa_send_justfloat(const float channels[], uint32_t count);

/**
 * @brief  解析 VOFA+ 下行命令
 * @note   支持的命令格式:
 *         - "Kp=1.5"     → 设置当前电机比例增益
 *         - "Ki=0.3"     → 设置当前电机积分增益
 *         - "Kd=0.1"     → 设置当前电机微分增益
 *         - "Target=200" → 设置目标RPM
 *         - "Motor=0"    → 选择电机(0-3)
 *         - "Run"        → 启动当前电机
 *         - "Stop"       → 停止当前电机
 *         - "StopAll"    → 停止所有电机
 * @param  line  输入命令行(以\0结尾)
 * @param  cmd   输出: 解析结果
 * @retval true  解析成功
 * @retval false 未知命令或解析失败
 */
bool app_vofa_parse_cmd(const char *line, vofa_cmd_t *cmd);

/**
 * @brief  应用解析后的 VOFA+ 命令
 * @note   根据命令类型修改共享上下文中的PID参数/目标值/电机状态
 * @param  cmd   解析后的命令
 * @param  ctx   共享上下文指针
 * @param  current_motor  当前选中的电机索引(输入/输出)
 */
void app_vofa_apply_cmd(const vofa_cmd_t *cmd,
                         app_shared_ctx_t *ctx,
                         uint32_t *current_motor);

#ifdef __cplusplus
}
#endif

#endif /* APP_VOFA_H */