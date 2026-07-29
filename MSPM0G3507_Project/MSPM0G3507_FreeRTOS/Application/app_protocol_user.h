/**
 * @file    app_protocol_user.h
 * @brief   Board A 十个通用自定义业务模板。
 *
 * 使用方法：
 * 1. 上位机使用 msg_class=COMMAND，当前模板 opcode 使用 0x20~0x29，0x2A~0x3F 可继续扩展。
 * 2. 在 app_protocol_user.c 对应的模板函数中填写自己的 payload 格式和业务逻辑。
 * 3. 新增业务时，复制一个模板函数，并在 app_protocol_user_execute() 的 switch 中增加一个 case。
 *    payload 长度直接在业务函数内检查。
 *
 * 注意：模板默认只返回 COMPLETED，不会自动驱动电机，也不会修改 Board A 看门狗。
 */
#ifndef APP_PROTOCOL_USER_H
#define APP_PROTOCOL_USER_H

#include <stdint.h>
#include <stdbool.h>
#include "proto_dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 十个通用自定义模板的 Opcode。0x01~0x09 保留给标准协议命令。 */
#define APP_USER_OP_CUSTOM_01  0x20U
#define APP_USER_OP_CUSTOM_02  0x21U
#define APP_USER_OP_CUSTOM_03  0x22U
#define APP_USER_OP_CUSTOM_04  0x23U
#define APP_USER_OP_CUSTOM_05  0x24U
#define APP_USER_OP_CUSTOM_06  0x25U
#define APP_USER_OP_CUSTOM_07  0x26U
#define APP_USER_OP_CUSTOM_08  0x27U
#define APP_USER_OP_CUSTOM_09  0x28U
#define APP_USER_OP_CUSTOM_10  0x29U

/* 兼容之前已经使用的名称；后续建议统一使用 APP_USER_OP_CUSTOM_xx。 */
#define APP_USER_OP_01_SPEED_CONTROL      APP_USER_OP_CUSTOM_01
#define APP_USER_OP_02_POSITION_CONTROL   APP_USER_OP_CUSTOM_02
#define APP_USER_OP_03_ANGLE_CONTROL      APP_USER_OP_CUSTOM_03
#define APP_USER_OP_04_LINE_TRACK         APP_USER_OP_CUSTOM_04
#define APP_USER_OP_04_PID_CONFIG         APP_USER_OP_CUSTOM_04  /* 旧名称兼容 */
#define APP_USER_OP_05_TRAJECTORY_START   APP_USER_OP_CUSTOM_05
#define APP_USER_OP_06_TRAJECTORY_STOP     APP_USER_OP_CUSTOM_06
#define APP_USER_OP_07_MULTI_MOTOR        APP_USER_OP_CUSTOM_07
#define APP_USER_OP_08_CALIBRATION         APP_USER_OP_CUSTOM_08
#define APP_USER_OP_09_PARAMETER_SAVE      APP_USER_OP_CUSTOM_09
#define APP_USER_OP_10_CUSTOM_CONTROL      APP_USER_OP_CUSTOM_10

/* 自定义模块4：循迹控制命令的 payload 定义。 */
#define APP_USER_LINE_TRACK_STOP_RESET   0x00U
#define APP_USER_LINE_TRACK_START        0x01U
#define APP_USER_LINE_TRACK_RESET        0x02U

/* 查询自定义命令的 payload 长度范围和幂等属性。 */
bool app_protocol_user_get_info(uint8_t opcode,
                                uint16_t *min_payload_len,
                                uint16_t *max_payload_len,
                                bool *idempotent);

/* 执行自定义命令。detail_code 只在返回 BAD_PARAM 时填写。 */
proto_user_result_t app_protocol_user_execute(uint8_t opcode,
                                              const uint8_t *payload,
                                              uint16_t payload_len,
                                              uint16_t *detail_code);

/* 查询循迹开关。实际算法在控制任务中按固定周期执行。 */
bool app_protocol_user_line_track_is_enabled(void);

/**
 * 请求控制任务启动循迹。
 * @note 只更新软件请求标志，不直接读取传感器或操作电机。
 */
void app_protocol_user_line_track_request_start(void);

/* 取出一次待处理的循迹复位请求，必须在控制任务中调用。 */
bool app_protocol_user_line_track_take_reset_request(void);

/*
 * 由控制任务在过流等安全故障下强制撤销循迹请求，防止下一周期自动重启。
 * 该函数只修改软件请求标志，不直接操作电机硬件。
 */
void app_protocol_user_line_track_force_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_PROTOCOL_USER_H */
