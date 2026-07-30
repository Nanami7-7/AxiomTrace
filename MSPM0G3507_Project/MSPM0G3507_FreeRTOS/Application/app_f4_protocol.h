/**
 * @file app_f4_protocol.h
 * @brief STM32F4 上位机固定帧协议业务接口。
 *
 * 本模块只运行在任务上下文：UART1 ISR 负责收字节，协议任务负责解析、
 * 执行安全命令以及周期发送状态、IMU 和心跳帧。
 */
#ifndef APP_F4_PROTOCOL_H
#define APP_F4_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_F4_MSG_CHASSIS_CMD       (0x10U)
#define APP_F4_MSG_CHASSIS_STATUS    (0x11U)
#define APP_F4_MSG_CHASSIS_IMU       (0x12U)
#define APP_F4_MSG_CHASSIS_HEARTBEAT (0x13U)

#define APP_F4_CMD_STOP               (0U)
#define APP_F4_CMD_START              (1U)
#define APP_F4_CMD_ESTOP              (2U)

typedef enum {
    APP_F4_STATE_IDLE = 0U,
    APP_F4_STATE_RUNNING = 1U,
    APP_F4_STATE_FAULT = 2U
} app_f4_state_t;

#define APP_F4_FAULT_NONE             (0U)
#define APP_F4_FAULT_LINK_TIMEOUT     (1U)
#define APP_F4_FAULT_ESTOP            (2U)

/** 协议解析和链路统计，供调试或后续诊断命令读取。 */
typedef struct {
    uint32_t valid_frames;
    uint32_t crc_errors;
    uint32_t version_errors;
    uint32_t length_errors;
    uint32_t timeout_errors;
    uint32_t sequence_gaps;
    uint32_t unsupported_commands;
    uint32_t tx_errors;
    uint16_t last_rx_sequence;
    uint16_t last_command_sequence;
    bool sequence_valid;
    bool link_seen;
} app_f4_protocol_stats_t;

void app_f4_protocol_init(void);
void app_f4_protocol_feed_byte(uint8_t byte);
void app_f4_protocol_tick(uint32_t now_ms);
void app_f4_protocol_get_stats(app_f4_protocol_stats_t *out);

#ifdef __cplusplus
}
#endif
#endif /* APP_F4_PROTOCOL_H */