/**
 * @file    app_protocol_a.c
 * @brief   Board A UART1 COBS 协议任务和应用适配。
 *
 * 本文件只做三件事：读取 UART1 字节、调用协议层、把协议层命令
 * 映射到已有应用/BSP接口。后续增加业务时优先修改本文件的适配回调。
 */
#include "app_protocol_a.h"
#include "app_protocol_user.h"
#include "app_main.h"
#include "proto_dispatch.h"
#include "proto_stream.h"
#include "proto_uart1_a.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "bsp_adc.h"
#include "osal_api.h"
#include "project_config.h"
#include "Test/test_ab_protocol.h"
#include <string.h>

#define APP_PROTOCOL_A_TASK_STACK_WORDS (384U)
#define APP_PROTOCOL_A_TASK_PRIORITY    (3U)
#define APP_PROTOCOL_A_TICK_MS          (2U)
#define APP_PROTOCOL_A_STATUS_PERIOD_MS (200U)

static proto_stream_t s_stream;
static proto_dispatch_ctx_t s_dispatch;
static proto_stats_t s_stats;
static uint8_t s_decoded[PROTO_MAX_DECODED];

static uint32_t protocol_now_ms(void)
{
    return osal_ticks_to_ms(osal_get_tick_count());
}

static bool protocol_tx_send(const uint8_t *wire, size_t len)
{
    if (wire == NULL || len > UINT16_MAX) {
        return false;
    }
    return proto_uart1_a_write(wire, (uint16_t)len) == BSP_OK;
}

static bool protocol_command_allowed(uint8_t opcode)
{
    switch (opcode) {
    case PROTO_OP_STOP:
    case PROTO_OP_STOP_ALL:
    case PROTO_OP_ABORT:
    case PROTO_OP_DISABLE:
    case PROTO_OP_CLEAR_FAULT:
        return true;
    default:
        /* 自定义命令由 app_protocol_user.c 管理；模板默认不驱动电机。 */
        return PROTO_IS_USER_COMMAND_OPCODE(opcode) &&
               app_protocol_user_get_info(opcode, NULL, NULL, NULL);
    }
}

static bool protocol_user_command_get_info(uint8_t opcode,
                                           uint16_t *min_payload_len,
                                           uint16_t *max_payload_len,
                                           bool *idempotent)
{
    return app_protocol_user_get_info(opcode, min_payload_len,
                                      max_payload_len, idempotent);
}

static proto_user_result_t protocol_user_command_execute(uint8_t opcode,
                                                          const uint8_t *payload,
                                                          uint16_t payload_len,
                                                          uint16_t *detail_code)
{
    return app_protocol_user_execute(opcode, payload, payload_len, detail_code);
}
static void protocol_motor_stop(uint8_t motor_id, uint8_t stop_type)
{
    (void)stop_type;
    if (motor_id < BSP_MOTOR_COUNT) {
        app_motor_stop(app_protocol_get_context(), motor_id);
    }
}

static void protocol_motor_stop_all(void)
{
    app_motor_stop_all(app_protocol_get_context());
}

static void protocol_motor_abort(void)
{
    /* 当前应用没有单独的动作队列，ABORT 安全降级为全部停止。 */
    app_motor_stop_all(app_protocol_get_context());
}

static void protocol_motor_disable_output(void)
{
    app_motor_stop_all(app_protocol_get_context());
    bsp_motor_power_disable();
}

static void protocol_motor_enable_output(void)
{
    /* P1 被 command_allowed 拦截，保留空适配以便后续直接启用。 */
    (void)bsp_motor_power_enable();
}

static bool protocol_motor_run(void)
{
    /* P1 被 command_allowed 拦截；后续实现 RUN 时在这里接入控制状态。 */
    return false;
}

static void protocol_motor_set_target(uint8_t motor_id, uint8_t mode,
                                      int32_t target_value, int32_t limit_value)
{
    (void)motor_id;
    (void)mode;
    (void)target_value;
    (void)limit_value;
}

static void protocol_motor_set_mode(uint8_t motor_id, uint8_t mode)
{
    (void)motor_id;
    (void)mode;
}

static uint8_t protocol_motor_get_mode(uint8_t motor_id)
{
    app_state_snapshot_t snapshot;
    if (motor_id >= BSP_MOTOR_COUNT ||
        !app_state_snapshot_read(app_protocol_get_context(), &snapshot)) {
        return 0xFFU;
    }
    return (uint8_t)snapshot.mode;
}

static bool protocol_fault_clear(uint16_t fault_code)
{
    (void)fault_code;
    /* 当前工程没有独立故障锁存管理器，先允许协议层清除软件故障。 */
    return true;
}

static bool protocol_fault_has_active_hardware(void)
{
    return false;
}

static uint8_t protocol_motor_state(const app_state_snapshot_t *snapshot,
                                    uint8_t motor_id)
{
    if (!bsp_motor_power_is_enabled()) {
        return PROTO_MOTOR_DISABLED;
    }
    if (snapshot == NULL || motor_id >= BSP_MOTOR_COUNT) {
        return PROTO_MOTOR_FAULT;
    }
    if (!snapshot->motor[motor_id].enabled) {
        return PROTO_MOTOR_STOPPED;
    }
    if (snapshot->motor[motor_id].rpm == 0) {
        return PROTO_MOTOR_READY;
    }
    return PROTO_MOTOR_RUNNING;
}

static void protocol_status_get_summary(proto_snapshot_summary_t *out)
{
    app_state_snapshot_t snapshot;
    uint16_t active_mask = 0U;
    if (out == NULL) {
        return;
    }
    (void)memset(out, 0, sizeof(*out));
    if (app_state_snapshot_read(app_protocol_get_context(), &snapshot)) {
        for (uint8_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
            if (snapshot.motor[i].enabled) {
                active_mask |= (uint16_t)(1U << i);
            }
        }
    }
    out->state = (uint8_t)proto_dispatch_get_state(&s_dispatch);
    out->motor_count = BSP_MOTOR_COUNT;
    out->active_motor_mask = active_mask;
    out->safety_flags = 0U;
    if (bsp_motor_power_is_enabled()) {
        out->safety_flags |= PROTO_SAFETY_MOTOR_OUT_EN;
    }
    if (proto_dispatch_link_age_ms(&s_dispatch) < PROTO_WATCHDOG_TIMEOUT_MS) {
        out->safety_flags |= PROTO_SAFETY_WATCHDOG_ARMED;
    }
    out->uptime_ms = protocol_now_ms();
    out->link_age_ms = (uint16_t)((proto_dispatch_link_age_ms(&s_dispatch) > UINT16_MAX) ?
                                  UINT16_MAX : proto_dispatch_link_age_ms(&s_dispatch));
    out->status_generation = proto_dispatch_get_status_generation(&s_dispatch);
}

static void protocol_status_get_motor(uint8_t motor_id,
                                      proto_snapshot_motor_t *out)
{
    app_state_snapshot_t snapshot;
    int32_t counts[BSP_ENCODER_COUNT] = {0};
    float currents[BSP_MOTOR_COUNT] = {0.0f};
    if (out == NULL) {
        return;
    }
    (void)memset(out, 0, sizeof(*out));
    out->motor_id = motor_id;
    out->mode = PROTO_MODE_SPEED;
    if (motor_id >= BSP_MOTOR_COUNT ||
        !app_state_snapshot_read(app_protocol_get_context(), &snapshot)) {
        out->motor_state = PROTO_MOTOR_FAULT;
        return;
    }
    (void)bsp_encoder_get_all_counts(counts);
    bsp_adc_get_all_currents_ma(currents);
    out->motor_state = protocol_motor_state(&snapshot, motor_id);
    out->mode = (uint8_t)snapshot.mode;
    out->power_enabled = bsp_motor_power_is_enabled() ? 1U : 0U;
    out->target_value = (int32_t)snapshot.motor[motor_id].target;
    out->speed_x100_rpm = snapshot.motor[motor_id].rpm * 100;
    out->position_count = counts[motor_id];
    out->current_ma = (int32_t)currents[motor_id];
    out->voltage_mv = (uint16_t)((bsp_adc_get_bus_voltage_mv() > UINT16_MAX) ?
                                 UINT16_MAX : bsp_adc_get_bus_voltage_mv());
    out->temperature_x100_c = (int16_t)(snapshot.imu.temperature * 100.0f);
}

static void protocol_status_get_sensor(proto_snapshot_sensor_t *out)
{
    app_state_snapshot_t snapshot;
    float currents[BSP_MOTOR_COUNT] = {0.0f};
    uint32_t total_current = 0U;
    if (out == NULL) {
        return;
    }
    (void)memset(out, 0, sizeof(*out));
    if (!app_state_snapshot_read(app_protocol_get_context(), &snapshot)) {
        return;
    }
    bsp_adc_get_all_currents_ma(currents);
    for (uint8_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
        if (currents[i] > 0.0f) {
            total_current += (uint32_t)currents[i];
        }
    }
    out->sample_time_ms = snapshot.imu.timestamp_ms;
    out->sensor_flags = PROTO_SENSOR_GYRO_VALID |
                        PROTO_SENSOR_ACCEL_VALID |
                        PROTO_SENSOR_BUS_V_VALID |
                        PROTO_SENSOR_MOTOR_I_VALID |
                        PROTO_SENSOR_TEMP_VALID |
                        PROTO_SENSOR_SAMPLE_T_VALID;
    out->gyro_x_x1000_dps = (int32_t)(snapshot.imu.gyro_x_dps * 1000.0f);
    out->gyro_y_x1000_dps = (int32_t)(snapshot.imu.gyro_y_dps * 1000.0f);
    out->gyro_z_x1000_dps = (int32_t)(snapshot.imu.gyro_z_dps * 1000.0f);
    out->accel_x_x1000_mg = (int32_t)(snapshot.imu.accel_x_g * 1000.0f);
    out->accel_y_x1000_mg = (int32_t)(snapshot.imu.accel_y_g * 1000.0f);
    out->accel_z_x1000_mg = (int32_t)(snapshot.imu.accel_z_g * 1000.0f);
    out->bus_voltage_mv = (uint16_t)((bsp_adc_get_bus_voltage_mv() > UINT16_MAX) ?
                                     UINT16_MAX : bsp_adc_get_bus_voltage_mv());
    out->motor_current_ma = (uint16_t)((total_current > UINT16_MAX) ?
                                       UINT16_MAX : total_current);
    out->temperature_x100_c = (int16_t)(snapshot.imu.temperature * 100.0f);
}

static void protocol_status_get_fault(proto_snapshot_fault_t *out)
{
    if (out == NULL) {
        return;
    }
    (void)memset(out, 0, sizeof(*out));
    out->fault_code = PROTO_FAULT_NONE;
}

static void protocol_status_get_info(proto_snapshot_info_t *out)
{
    if (out == NULL) {
        return;
    }
    (void)memset(out, 0, sizeof(*out));
    out->device_type = PROTO_DEVICE_MOTOR_CONTROLLER;
    out->board_role = PROTO_BOARD_ROLE_CONTROLLER;
    out->protocol_version = PROTO_VERSION;
    out->capability_flags = PROTO_CAP_SPEED_CONTROL |
                            PROTO_CAP_POSITION_CONTROL |
                            PROTO_CAP_ANGLE_CONTROL |
                            PROTO_CAP_MULTI_MOTOR |
                            PROTO_CAP_IMU |
                            PROTO_CAP_ADC |
                            PROTO_CAP_ENCODER |
                            PROTO_CAP_MOTOR_POWER_CTRL;
}

static uint32_t protocol_uptime_ms(void)
{
    return protocol_now_ms();
}

static void protocol_enter_critical(void)
{
    osal_critical_enter();
}

static void protocol_exit_critical(void)
{
    osal_critical_exit();
}

static const proto_dispatch_adapter_t s_adapter = {
    .now_ms = protocol_now_ms,
    .command_allowed = protocol_command_allowed,
    .user_command_get_info = protocol_user_command_get_info,
    .user_command_execute = protocol_user_command_execute,
    .motor_stop = protocol_motor_stop,
    .motor_stop_all = protocol_motor_stop_all,
    .motor_abort = protocol_motor_abort,
    .motor_disable_output = protocol_motor_disable_output,
    .motor_enable_output = protocol_motor_enable_output,
    .motor_run = protocol_motor_run,
    .motor_set_target = protocol_motor_set_target,
    .motor_set_mode = protocol_motor_set_mode,
    .motor_get_mode = protocol_motor_get_mode,
    .fault_clear = protocol_fault_clear,
    .fault_has_active_hardware = protocol_fault_has_active_hardware,
    .status_get_summary = protocol_status_get_summary,
    .status_get_motor = protocol_status_get_motor,
    .status_get_sensor = protocol_status_get_sensor,
    .status_get_fault = protocol_status_get_fault,
    .status_get_info = protocol_status_get_info,
    .get_uptime_ms = protocol_uptime_ms,
    .tx_send = protocol_tx_send,
    .enter_critical = protocol_enter_critical,
    .exit_critical = protocol_exit_critical,
};

void app_protocol_a_task(void *param)
{
    uint8_t byte;
    size_t decoded_len;
    (void)param;
    for (;;) {
        while (proto_uart1_a_getc(&byte) == BSP_OK) {
            proto_stream_result_t result = proto_stream_feed(
                &s_stream, byte, s_decoded, sizeof(s_decoded), &decoded_len);
            if (result == PROTO_STREAM_FRAME) {
                /* 先打印收到的帧，再交给正式协议分发器处理。 */
                test_ab_protocol_print_decoded(s_decoded, decoded_len);
                proto_dispatch_process_frame(&s_dispatch, s_decoded, decoded_len);
            }
        }
        proto_dispatch_tick(&s_dispatch);
        proto_dispatch_periodic_status(&s_dispatch, protocol_now_ms(),
                                       APP_PROTOCOL_A_STATUS_PERIOD_MS);
        osal_task_delay_ms(APP_PROTOCOL_A_TICK_MS);
    }
}

int32_t app_protocol_a_init(void)
{
    if (proto_uart1_a_init() != BSP_OK) {
        return -1;
    }
    proto_stream_init(&s_stream);
    (void)memset(&s_stats, 0, sizeof(s_stats));
    proto_dispatch_init(&s_dispatch, &s_adapter, &s_stats);
    s_dispatch.motor_count = BSP_MOTOR_COUNT;
    if (osal_task_create(app_protocol_a_task, "proto_a",
                         APP_PROTOCOL_A_TASK_STACK_WORDS, NULL,
                         APP_PROTOCOL_A_TASK_PRIORITY) == NULL) {
        return -2;
    }
    return 0;
}