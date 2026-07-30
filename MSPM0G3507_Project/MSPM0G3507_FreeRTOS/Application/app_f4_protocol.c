/**
 * @file app_f4_protocol.c
 * @brief STM32F4 上位机协议解析、业务命令和周期遥测。
 *
 * 帧格式：A5 5A | version | message_id | payload_len(u16 LE) |
 * sequence(u16 LE) | timestamp_us(u32 LE) | payload | crc16(u16 LE)。
 * CRC 为 CRC-16/CCITT-FALSE，覆盖 version 到 payload。
 */
#include "app_f4_protocol.h"
#include "app_main.h"
#include "bsp_motor.h"
#include "osal_api.h"
#include "project_config.h"
#include "proto_uart1_a.h"
#include <limits.h>
#include <string.h>

#define F4_SOF0                 (0xA5U)
#define F4_SOF1                 (0x5AU)
#define F4_VERSION              (0x01U)
#define F4_HEADER_SIZE          (12U)
#define F4_CRC_SIZE             (2U)
#define F4_MIN_FRAME_SIZE       (F4_HEADER_SIZE + F4_CRC_SIZE)

typedef enum {
    F4_PARSE_SYNC0 = 0U,
    F4_PARSE_SYNC1,
    F4_PARSE_BODY
} f4_parse_state_t;

static uint8_t s_frame[PRJ_F4_MAX_FRAME];
static uint8_t s_tx_frame[PRJ_F4_MAX_FRAME];
static uint16_t s_frame_len;
static uint16_t s_expected_len;
static f4_parse_state_t s_parse_state;
static uint32_t s_last_byte_ms;
static uint32_t s_last_rx_ms;
static uint32_t s_last_status_ms;
static uint32_t s_last_heartbeat_ms;
static uint32_t s_last_imu_ms;
static uint16_t s_tx_sequence;
static uint16_t s_last_command_sequence;
static uint16_t s_last_rx_sequence;
static uint16_t s_fault_code;
static bool s_sequence_valid;
static bool s_link_seen;
static bool s_link_timeout_reported;
static bool s_estop_latched;
static app_f4_protocol_stats_t s_stats;

static uint32_t f4_now_ms(void)
{
    return osal_ticks_to_ms(osal_get_tick_count());
}

static uint16_t f4_read_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8U);
}

static uint32_t f4_read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) |
           ((uint32_t)p[3] << 24U);
}

static void f4_put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

static void f4_put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8U) & 0xFFU);
    p[2] = (uint8_t)((value >> 16U) & 0xFFU);
    p[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static uint16_t f4_crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t bit;
    for (i = 0U; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8U;
        for (bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            } else {
                crc <<= 1U;
            }
        }
    }
    return crc;
}

static void f4_parser_reset(void)
{
    s_parse_state = F4_PARSE_SYNC0;
    s_frame_len = 0U;
    s_expected_len = 0U;
}

static void f4_parser_resync(uint8_t byte)
{
    f4_parser_reset();
    if (byte == F4_SOF0) {
        s_parse_state = F4_PARSE_SYNC1;
        s_frame[0] = F4_SOF0;
        s_frame_len = 1U;
    }
}

static uint8_t f4_get_state(void)
{
    app_state_snapshot_t snapshot;
    uint8_t state = APP_F4_STATE_IDLE;
    if (s_estop_latched || s_fault_code != APP_F4_FAULT_NONE) {
        return APP_F4_STATE_FAULT;
    }
    if (app_state_snapshot_read(app_protocol_get_context(), &snapshot)) {
        uint8_t i;
        for (i = 0U; i < BSP_MOTOR_COUNT; i++) {
            if (snapshot.motor[i].enabled) {
                state = APP_F4_STATE_RUNNING;
                break;
            }
        }
    }
    return state;
}

static bool f4_send_frame(uint8_t message_id, const uint8_t *payload,
                          uint16_t payload_len, uint32_t now_ms)
{
    uint16_t idx = 0U;
    uint16_t crc;
    uint16_t total_len;
    uint32_t timestamp_us;

    if (payload_len > PRJ_F4_MAX_PAYLOAD ||
        (payload == NULL && payload_len != 0U)) {
        s_stats.tx_errors++;
        return false;
    }
    total_len = (uint16_t)(F4_HEADER_SIZE + F4_CRC_SIZE + payload_len);
    if (total_len > PRJ_F4_MAX_FRAME) {
        s_stats.tx_errors++;
        return false;
    }

    s_tx_frame[idx++] = F4_SOF0;
    s_tx_frame[idx++] = F4_SOF1;
    s_tx_frame[idx++] = F4_VERSION;
    s_tx_frame[idx++] = message_id;
    f4_put_u16(&s_tx_frame[idx], payload_len);
    idx = (uint16_t)(idx + 2U);
    f4_put_u16(&s_tx_frame[idx], s_tx_sequence++);
    idx = (uint16_t)(idx + 2U);
    timestamp_us = now_ms * 1000U;
    f4_put_u32(&s_tx_frame[idx], timestamp_us);
    idx = (uint16_t)(idx + 4U);
    if (payload_len != 0U) {
        (void)memcpy(&s_tx_frame[idx], payload, payload_len);
        idx = (uint16_t)(idx + payload_len);
    }
    crc = f4_crc16(&s_tx_frame[2], (uint16_t)(10U + payload_len));
    f4_put_u16(&s_tx_frame[idx], crc);
    idx = (uint16_t)(idx + 2U);
    if (proto_uart1_a_write(s_tx_frame, idx) != BSP_OK) {
        s_stats.tx_errors++;
        return false;
    }
    return true;
}

static void f4_send_status(uint32_t now_ms)
{
    uint8_t payload[5];
    payload[0] = f4_get_state();
    f4_put_u16(&payload[1], s_fault_code);
    f4_put_u16(&payload[3], s_last_command_sequence);
    (void)f4_send_frame(APP_F4_MSG_CHASSIS_STATUS, payload,
                        (uint16_t)sizeof(payload), now_ms);
}

static void f4_send_heartbeat(uint32_t now_ms)
{
    (void)f4_send_frame(APP_F4_MSG_CHASSIS_HEARTBEAT, NULL, 0U, now_ms);
}

static int32_t f4_g_to_mm_s2(float value)
{
    float scaled = value * 9806.65f;
    if (scaled > 2147483647.0f) {
        return INT32_MAX;
    }
    if (scaled < -2147483648.0f) {
        return INT32_MIN;
    }
    return (int32_t)scaled;
}

static void f4_send_imu(uint32_t now_ms)
{
    app_state_snapshot_t snapshot;
    uint8_t payload[13];
    bool valid = app_state_snapshot_read(app_protocol_get_context(), &snapshot);
    if (!valid) {
        (void)memset(payload, 0, sizeof(payload));
    } else {
        f4_put_u32(&payload[0], (uint32_t)f4_g_to_mm_s2(snapshot.imu.accel_x_g));
        f4_put_u32(&payload[4], (uint32_t)f4_g_to_mm_s2(snapshot.imu.accel_y_g));
        f4_put_u32(&payload[8], (uint32_t)f4_g_to_mm_s2(snapshot.imu.accel_z_g));
        payload[12] = (snapshot.imu.timestamp_ms != 0U) ? 0xFFU : 0U;
    }
    (void)f4_send_frame(APP_F4_MSG_CHASSIS_IMU, payload,
                        (uint16_t)sizeof(payload), now_ms);
}

static void f4_handle_command(const uint8_t *payload, uint16_t payload_len,
                              uint16_t sequence)
{
    uint8_t command;
    if (payload_len < 2U) {
        s_stats.unsupported_commands++;
        return;
    }
    command = payload[0];
    s_last_command_sequence = sequence;
    switch (command) {
    case APP_F4_CMD_STOP:
        app_motor_stop_all(app_protocol_get_context());
        if (!s_estop_latched) {
            s_fault_code = APP_F4_FAULT_NONE;
        }
        break;
    case APP_F4_CMD_START:
        /* START 不凭空生成速度目标，只解除通信侧的正常停止状态。 */
        if (!s_estop_latched) {
            s_fault_code = APP_F4_FAULT_NONE;
            s_link_timeout_reported = false;
        }
        break;
    case APP_F4_CMD_ESTOP:
        app_motor_stop_all(app_protocol_get_context());
        bsp_motor_power_disable();
        s_estop_latched = true;
        s_fault_code = APP_F4_FAULT_ESTOP;
        break;
    default:
        s_stats.unsupported_commands++;
        break;
    }
}

static void f4_process_frame(void)
{
    uint16_t payload_len;
    uint16_t received_crc;
    uint16_t calculated_crc;
    uint16_t sequence;

    if (s_frame_len < F4_MIN_FRAME_SIZE) {
        return;
    }
    if (s_frame[2] != F4_VERSION) {
        s_stats.version_errors++;
        return;
    }
    payload_len = f4_read_u16(&s_frame[4]);
    if (payload_len > PRJ_F4_MAX_PAYLOAD ||
        s_frame_len != (uint16_t)(F4_HEADER_SIZE + F4_CRC_SIZE + payload_len)) {
        s_stats.length_errors++;
        return;
    }
    received_crc = f4_read_u16(&s_frame[F4_HEADER_SIZE + payload_len]);
    calculated_crc = f4_crc16(&s_frame[2], (uint16_t)(10U + payload_len));
    if (received_crc != calculated_crc) {
        s_stats.crc_errors++;
        return;
    }

    sequence = f4_read_u16(&s_frame[6]);
    if (s_sequence_valid && (uint16_t)(sequence - s_last_rx_sequence) != 1U) {
        s_stats.sequence_gaps++;
    }
    s_last_rx_sequence = sequence;
    s_sequence_valid = true;
    s_stats.last_rx_sequence = sequence;
    s_stats.sequence_valid = true;
    s_stats.valid_frames++;
    s_last_rx_ms = f4_now_ms();
    s_link_seen = true;
    s_link_timeout_reported = false;

    switch (s_frame[3]) {
    case APP_F4_MSG_CHASSIS_CMD:
        f4_handle_command(&s_frame[12], payload_len, sequence);
        break;
    case APP_F4_MSG_CHASSIS_HEARTBEAT:
        break;
    default:
        s_stats.unsupported_commands++;
        break;
    }
}

void app_f4_protocol_init(void)
{
    (void)memset(&s_stats, 0, sizeof(s_stats));
    f4_parser_reset();
    s_last_byte_ms = 0U;
    s_last_rx_ms = 0U;
    s_last_status_ms = 0U;
    s_last_heartbeat_ms = 0U;
    s_last_imu_ms = 0U;
    s_tx_sequence = 0U;
    s_last_command_sequence = 0U;
    s_last_rx_sequence = 0U;
    s_fault_code = APP_F4_FAULT_NONE;
    s_sequence_valid = false;
    s_link_seen = false;
    s_link_timeout_reported = false;
    s_estop_latched = false;
}

void app_f4_protocol_feed_byte(uint8_t byte)
{
    uint16_t payload_len;
    uint32_t now_ms = f4_now_ms();
    s_last_byte_ms = now_ms;

    switch (s_parse_state) {
    case F4_PARSE_SYNC0:
        if (byte == F4_SOF0) {
            s_frame[0] = byte;
            s_frame_len = 1U;
            s_parse_state = F4_PARSE_SYNC1;
        }
        break;
    case F4_PARSE_SYNC1:
        if (byte == F4_SOF1) {
            s_frame[1] = byte;
            s_frame_len = 2U;
            s_parse_state = F4_PARSE_BODY;
        } else if (byte == F4_SOF0) {
            s_frame[0] = byte;
            s_frame_len = 1U;
        } else {
            f4_parser_reset();
        }
        break;
    case F4_PARSE_BODY:
        if (s_frame_len >= PRJ_F4_MAX_FRAME) {
            s_stats.length_errors++;
            f4_parser_resync(byte);
            break;
        }
        s_frame[s_frame_len++] = byte;
        if (s_frame_len == 6U) {
            payload_len = f4_read_u16(&s_frame[4]);
            if (payload_len > PRJ_F4_MAX_PAYLOAD) {
                s_stats.length_errors++;
                f4_parser_resync(byte);
                break;
            }
            s_expected_len = (uint16_t)(F4_HEADER_SIZE + F4_CRC_SIZE + payload_len);
            if (s_expected_len > PRJ_F4_MAX_FRAME) {
                s_stats.length_errors++;
                f4_parser_reset();
            }
        }
        if (s_expected_len != 0U && s_frame_len >= s_expected_len) {
            f4_process_frame();
            f4_parser_reset();
        }
        break;
    default:
        f4_parser_reset();
        break;
    }
}

void app_f4_protocol_tick(uint32_t now_ms)
{
    if (s_parse_state != F4_PARSE_SYNC0 &&
        (uint32_t)(now_ms - s_last_byte_ms) > PRJ_F4_FRAME_TIMEOUT_MS) {
        s_stats.timeout_errors++;
        f4_parser_reset();
    }

    if (s_link_seen && !s_link_timeout_reported &&
        (uint32_t)(now_ms - s_last_rx_ms) > PRJ_F4_LINK_TIMEOUT_MS) {
        app_motor_stop_all(app_protocol_get_context());
        if (!s_estop_latched) {
            s_fault_code = APP_F4_FAULT_LINK_TIMEOUT;
        }
        s_link_timeout_reported = true;
    }

    if ((uint32_t)(now_ms - s_last_heartbeat_ms) >= PRJ_F4_HEARTBEAT_PERIOD_MS) {
        s_last_heartbeat_ms = now_ms;
        f4_send_heartbeat(now_ms);
    }
    if ((uint32_t)(now_ms - s_last_status_ms) >= PRJ_F4_STATUS_PERIOD_MS) {
        s_last_status_ms = now_ms;
        f4_send_status(now_ms);
    }
    if ((uint32_t)(now_ms - s_last_imu_ms) >= PRJ_F4_IMU_PERIOD_MS) {
        s_last_imu_ms = now_ms;
        f4_send_imu(now_ms);
    }
}

void app_f4_protocol_get_stats(app_f4_protocol_stats_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = s_stats;
    out->last_command_sequence = s_last_command_sequence;
    out->link_seen = s_link_seen;
}