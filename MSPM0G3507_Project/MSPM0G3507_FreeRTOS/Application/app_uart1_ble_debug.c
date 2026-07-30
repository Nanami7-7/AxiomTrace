/**
 * @file    app_uart1_ble_debug.c
 * @brief   Board A UART1 蓝牙循迹调试接口。
 *
 * 设计原则：
 * 1. 2ms 控制任务只复制快照，不格式化字符串、不等待 UART；
 * 2. 本低优先级任务统一解析 BLE 文本命令并发送诊断日志；
 * 3. 9600 波特率下每次只发送一页紧凑日志，ALL 模式按页轮询，避免串口长期拥塞；
 * 4. 调试接口不提供启动电机命令，运行参数只允许在循迹停止时修改。
 */
#include "app_uart1_ble_debug.h"
#include "osal_api.h"
#include "project_config.h"
#include "proto_uart1_a.h"
#include "bsp_motor.h"
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_UART1_BLE_DEBUG_TASK_NAME       "uart1_ble"
#define APP_UART1_BLE_DEBUG_RX_LINE_SIZE    (96U)
#define APP_UART1_BLE_DEBUG_PERIOD_MIN_MS   (300U)
#define APP_UART1_BLE_DEBUG_PERIOD_MAX_MS   (5000U)
#define APP_UART1_BLE_DEBUG_POLL_MS         (20U)
#define APP_UART1_BLE_DEBUG_SNAPSHOT_OLD_MS (100U)

typedef enum {
    APP_BLE_VIEW_CORE = 0,
    APP_BLE_VIEW_MODEL,
    APP_BLE_VIEW_IMU,
    APP_BLE_VIEW_MOTOR,
    APP_BLE_VIEW_SYSTEM,
    APP_BLE_VIEW_ALL,
    APP_BLE_VIEW_COUNT
} app_ble_debug_view_t;

static osal_task_handle_t s_task_handle;
static volatile bool s_initialized;
static volatile bool s_snapshot_valid;
static app_state_snapshot_t s_app_snapshot;
static line_track_output_t s_line_snapshot;
static bool s_line_running;
static uint32_t s_snapshot_time_ms;

static bool s_log_enabled;
static bool s_log_once;
static uint32_t s_log_period_ms;
static app_ble_debug_view_t s_log_view;
static app_ble_debug_view_t s_all_next_view;
static uint32_t s_sequence;
static uint32_t s_command_count;
static uint32_t s_command_error_count;
static uint32_t s_rx_invalid_bytes;
static uint32_t s_rx_line_overflow;
static uint32_t s_tx_error_count;
static uint32_t s_tx_truncated_count;
static char s_rx_line[APP_UART1_BLE_DEBUG_RX_LINE_SIZE];
static uint16_t s_rx_line_length;
static bool s_rx_line_discard;
static char s_tx_buffer[PRJ_UART1_BLE_DEBUG_TX_BUF_SIZE];

static const char *view_name(app_ble_debug_view_t view)
{
    switch (view) {
    case APP_BLE_VIEW_CORE:   return "CORE";
    case APP_BLE_VIEW_MODEL:  return "MODEL";
    case APP_BLE_VIEW_IMU:    return "IMU";
    case APP_BLE_VIEW_MOTOR:  return "MOTOR";
    case APP_BLE_VIEW_SYSTEM: return "SYS";
    case APP_BLE_VIEW_ALL:    return "ALL";
    default:                  return "UNKNOWN";
    }
}

static bool parse_view(const char *text, app_ble_debug_view_t *view)
{
    if (text == NULL || view == NULL) {
        return false;
    }
    if (strcmp(text, "CORE") == 0) {
        *view = APP_BLE_VIEW_CORE;
    } else if (strcmp(text, "MODEL") == 0) {
        *view = APP_BLE_VIEW_MODEL;
    } else if (strcmp(text, "IMU") == 0) {
        *view = APP_BLE_VIEW_IMU;
    } else if (strcmp(text, "MOTOR") == 0) {
        *view = APP_BLE_VIEW_MOTOR;
    } else if (strcmp(text, "SYS") == 0 || strcmp(text, "SYSTEM") == 0) {
        *view = APP_BLE_VIEW_SYSTEM;
    } else if (strcmp(text, "ALL") == 0) {
        *view = APP_BLE_VIEW_ALL;
    } else {
        return false;
    }
    return true;
}

static void count_u32(uint32_t *value)
{
    if (value != NULL && *value < UINT32_MAX) {
        (*value)++;
    }
}

static void tx_text(const char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }
    length = strlen(text);
    if (length == 0U) {
        return;
    }
    if (length > UINT16_MAX) {
        length = UINT16_MAX;
    }
    if (proto_uart1_a_write((const uint8_t *)text,
                            (uint16_t)length) != BSP_OK) {
        count_u32(&s_tx_error_count);
    }
}

static void tx_printf(const char *format, ...)
{
    va_list args;
    int length;

    if (format == NULL) {
        return;
    }
    va_start(args, format);
    length = vsnprintf(s_tx_buffer, sizeof(s_tx_buffer), format, args);
    va_end(args);

    if (length <= 0) {
        return;
    }
    if ((size_t)length >= sizeof(s_tx_buffer)) {
        count_u32(&s_tx_truncated_count);
        length = (int)sizeof(s_tx_buffer) - 1;
        if (length >= 2) {
            s_tx_buffer[length - 2] = '\r';
            s_tx_buffer[length - 1] = '\n';
        }
    }
    if (proto_uart1_a_write((const uint8_t *)s_tx_buffer,
                            (uint16_t)length) != BSP_OK) {
        count_u32(&s_tx_error_count);
    }
}

static void copy_snapshot(uint32_t now_ms,
                          bool running,
                          const line_track_output_t *line,
                          const app_shared_ctx_t *ctx)
{
    app_state_snapshot_t app_snapshot;
    line_track_output_t line_snapshot;

    (void)memset(&line_snapshot, 0, sizeof(line_snapshot));
    if (line != NULL) {
        line_snapshot = *line;
    }
    (void)memset(&app_snapshot, 0, sizeof(app_snapshot));
    if (ctx != NULL) {
        (void)app_state_snapshot_read(ctx, &app_snapshot);
    }

    OSAL_CRITICAL_SECTION {
        s_line_snapshot = line_snapshot;
        s_app_snapshot = app_snapshot;
        s_line_running = running;
        s_snapshot_time_ms = now_ms;
        s_snapshot_valid = true;
    }
}

static bool read_snapshot(app_state_snapshot_t *app_snapshot,
                          line_track_output_t *line_snapshot,
                          bool *running,
                          uint32_t *snapshot_time_ms)
{
    if (!s_snapshot_valid || app_snapshot == NULL || line_snapshot == NULL ||
        running == NULL || snapshot_time_ms == NULL) {
        return false;
    }
    OSAL_CRITICAL_SECTION {
        *line_snapshot = s_line_snapshot;
        *app_snapshot = s_app_snapshot;
        *running = s_line_running;
        *snapshot_time_ms = s_snapshot_time_ms;
    }
    return true;
}

static void black_mask_text(uint8_t black_mask, char *text, size_t size)
{
    uint32_t count = BSP_IR_CHANNEL_COUNT;

    if (text == NULL || size < (count + 1U)) {
        return;
    }
    for (uint32_t i = 0U; i < count; i++) {
        const uint32_t shift = (count - 1U) - i;
        text[i] = ((black_mask & (uint8_t)(1U << shift)) != 0U) ? '1' : '0';
    }
    text[count] = '\0';
}

static bool output_is_saturated(const app_state_snapshot_t *app_snapshot)
{
    const float limit = 0.95f * (float)bsp_motor_get_command_max();

    if (app_snapshot == NULL || limit <= 0.0f) {
        return false;
    }
    return fabsf((float)app_snapshot->control.output[BSP_MOTOR_D]) >= limit ||
           fabsf((float)app_snapshot->control.output[BSP_MOTOR_A]) >= limit;
}

static const char *diagnose(const app_state_snapshot_t *app_snapshot,
                            const line_track_output_t *line_snapshot,
                            bool running,
                            uint32_t snapshot_age_ms,
                            bool raw_imu_valid,
                            uint32_t raw_imu_age_ms)
{
    if (!running) {
        return "TRACK_OFF";
    }
    if (snapshot_age_ms > APP_UART1_BLE_DEBUG_SNAPSHOT_OLD_MS) {
        return "SNAP_OLD";
    }
    if (line_snapshot->lost_cycles != 0U || line_snapshot->black_mask == 0U) {
        return "IR_LOST";
    }
    if (line_snapshot->model_enabled && !line_snapshot->model_valid) {
        return "MODEL_BAD";
    }
#if ((LINE_TRACK_MODEL_CONTROL_ENABLE != 0U) &&      (LINE_TRACK_MODEL_GYRO_RATE_ENABLE != 0U))
    if (line_snapshot->model_enabled && !raw_imu_valid) {
        return "IMU_NONE";
    }
    if (line_snapshot->model_enabled &&
        raw_imu_age_ms > LINE_TRACK_IMU_MAX_AGE_MS) {
        return "IMU_STALE";
    }
#else
    (void)raw_imu_valid;
    (void)raw_imu_age_ms;
#endif
    if (!app_snapshot->motor[BSP_MOTOR_D].enabled ||
        !app_snapshot->motor[BSP_MOTOR_A].enabled) {
        return "MOTOR_OFF";
    }
    if (output_is_saturated(app_snapshot)) {
        return "OUT_SAT";
    }
    return "OK";
}

static void send_core(uint32_t sequence,
                      uint32_t now_ms,
                      uint32_t snapshot_time_ms,
                      const app_state_snapshot_t *app_snapshot,
                      const line_track_output_t *line_snapshot,
                      bool running,
                      const char *reason)
{
    char ir_text[BSP_IR_CHANNEL_COUNT + 1U];

    black_mask_text(line_snapshot->black_mask, ir_text, sizeof(ir_text));
    tx_printf(
        "LT,C,n=%lu,t=%lu,lag=%lu,run=%u,why=%s,st=%s,bm=%02X,ir=%s,e=%.3f,lost=%u,base=%.1f,turn=%.1f,tgt=%.1f/%.1f,rpm=%ld/%ld,out=%ld/%ld,en=%u/%u\r\n",
        (unsigned long)sequence,
        (unsigned long)snapshot_time_ms,
        (unsigned long)(now_ms - snapshot_time_ms),
        running ? 1U : 0U,
        reason,
        app_line_track_state_name(line_snapshot->current_state),
        (unsigned int)line_snapshot->black_mask,
        ir_text,
        (double)line_snapshot->line_error,
        (unsigned int)line_snapshot->lost_cycles,
        (double)line_snapshot->base_target_rpm,
        (double)line_snapshot->turn_diff_rpm,
        (double)line_snapshot->left_target_rpm,
        (double)line_snapshot->right_target_rpm,
        (long)app_snapshot->control.rpm[BSP_MOTOR_D],
        (long)app_snapshot->control.rpm[BSP_MOTOR_A],
        (long)app_snapshot->control.output[BSP_MOTOR_D],
        (long)app_snapshot->control.output[BSP_MOTOR_A],
        app_snapshot->motor[BSP_MOTOR_D].enabled ? 1U : 0U,
        app_snapshot->motor[BSP_MOTOR_A].enabled ? 1U : 0U);
}

static void send_model(uint32_t sequence,
                       uint32_t snapshot_time_ms,
                       const line_track_output_t *line_snapshot,
                       const char *reason)
{
    tx_printf(
        "LT,M,n=%lu,t=%lu,why=%s,on=%u,valid=%u,e=%.3f,em=%.4f,kr=%.3f,k=%.3f,req=%.1f,plan=%.1f,acc=%.1f,yr=%.2f,ym=%.2f,ye=%.2f,ff=%.1f,fb=%.1f,turn=%.1f\r\n",
        (unsigned long)sequence,
        (unsigned long)snapshot_time_ms,
        reason,
        line_snapshot->model_enabled ? 1U : 0U,
        line_snapshot->model_valid ? 1U : 0U,
        (double)line_snapshot->line_error,
        (double)line_snapshot->line_error_m,
        (double)line_snapshot->curvature_raw_m_inv,
        (double)line_snapshot->curvature_m_inv,
        (double)line_snapshot->base_request_rpm,
        (double)line_snapshot->base_planned_rpm,
        (double)line_snapshot->base_accel_rpm_s,
        (double)line_snapshot->yaw_rate_ref_dps,
        (double)line_snapshot->yaw_rate_measured_dps,
        (double)line_snapshot->yaw_rate_error_dps,
        (double)line_snapshot->turn_feedforward_rpm,
        (double)line_snapshot->turn_feedback_rpm,
        (double)line_snapshot->turn_diff_rpm);
}

static void send_imu(uint32_t sequence,
                     uint32_t snapshot_time_ms,
                     const app_state_snapshot_t *app_snapshot,
                     const line_track_output_t *line_snapshot,
                     bool raw_imu_valid,
                     uint32_t raw_imu_age_ms,
                     const char *reason)
{
    tx_printf(
        "LT,I,n=%lu,t=%lu,why=%s,rawV=%u,age=%lu,yaw=%.2f,gz=%.2f,ax=%.3f,ay=%.3f,az=%.3f,an=%.3f,iv=%u,rateV=%u,yr=%.2f,ym=%.2f,ye=%.2f,fb=%.1f\r\n",
        (unsigned long)sequence,
        (unsigned long)snapshot_time_ms,
        reason,
        raw_imu_valid ? 1U : 0U,
        (unsigned long)raw_imu_age_ms,
        (double)app_snapshot->imu.yaw,
        (double)app_snapshot->imu.gyro_z_dps,
        (double)app_snapshot->imu.accel_x_g,
        (double)app_snapshot->imu.accel_y_g,
        (double)app_snapshot->imu.accel_z_g,
        (double)line_snapshot->imu_accel_norm_g,
        line_snapshot->imu_valid ? 1U : 0U,
        line_snapshot->model_imu_rate_valid ? 1U : 0U,
        (double)line_snapshot->yaw_rate_ref_dps,
        (double)line_snapshot->yaw_rate_measured_dps,
        (double)line_snapshot->yaw_rate_error_dps,
        (double)line_snapshot->turn_feedback_rpm);
}

static void send_motor(uint32_t sequence,
                       uint32_t snapshot_time_ms,
                       const app_state_snapshot_t *app_snapshot,
                       const line_track_output_t *line_snapshot,
                       const char *reason)
{
    const float error_left = app_snapshot->motor[BSP_MOTOR_D].target -
                             (float)app_snapshot->control.rpm[BSP_MOTOR_D];
    const float error_right = app_snapshot->motor[BSP_MOTOR_A].target -
                              (float)app_snapshot->control.rpm[BSP_MOTOR_A];

    tx_printf(
        "LT,D,n=%lu,t=%lu,why=%s,map=L:D/R:A,cmd=%.1f/%.1f,set=%.1f/%.1f,rpm=%ld/%ld,err=%.1f/%.1f,pid=%.1f/%.1f,out=%ld/%ld,cur=%.1f/%.1f,vbus=%lu,sat=%u\r\n",
        (unsigned long)sequence,
        (unsigned long)snapshot_time_ms,
        reason,
        (double)line_snapshot->left_target_rpm,
        (double)line_snapshot->right_target_rpm,
        (double)app_snapshot->motor[BSP_MOTOR_D].target,
        (double)app_snapshot->motor[BSP_MOTOR_A].target,
        (long)app_snapshot->control.rpm[BSP_MOTOR_D],
        (long)app_snapshot->control.rpm[BSP_MOTOR_A],
        (double)error_left,
        (double)error_right,
        (double)app_snapshot->control.pid_correction[BSP_MOTOR_D],
        (double)app_snapshot->control.pid_correction[BSP_MOTOR_A],
        (long)app_snapshot->control.output[BSP_MOTOR_D],
        (long)app_snapshot->control.output[BSP_MOTOR_A],
        (double)app_snapshot->control.current_ma[BSP_MOTOR_D],
        (double)app_snapshot->control.current_ma[BSP_MOTOR_A],
        (unsigned long)app_snapshot->control.bus_voltage_mv,
        output_is_saturated(app_snapshot) ? 1U : 0U);
}

static void send_system(uint32_t sequence,
                        uint32_t now_ms,
                        uint32_t snapshot_time_ms,
                        const char *reason)
{
    proto_uart1_a_diag_t uart_diag;

    (void)memset(&uart_diag, 0, sizeof(uart_diag));
    (void)proto_uart1_a_get_diag(&uart_diag);
    tx_printf(
        "LT,S,n=%lu,t=%lu,why=%s,lag=%lu,log=%u,period=%lu,view=%s,rx=%lu,ovf=%lu,irq=%lu,ign=%lu,cmd=%lu,cerr=%lu,bad=%lu,lineovf=%lu,txerr=%lu,trunc=%lu\r\n",
        (unsigned long)sequence,
        (unsigned long)snapshot_time_ms,
        reason,
        (unsigned long)(now_ms - snapshot_time_ms),
        s_log_enabled ? 1U : 0U,
        (unsigned long)s_log_period_ms,
        view_name(s_log_view),
        (unsigned long)uart_diag.rx_bytes,
        (unsigned long)uart_diag.rx_overflow,
        (unsigned long)uart_diag.irq_count,
        (unsigned long)uart_diag.ignored_irq_count,
        (unsigned long)s_command_count,
        (unsigned long)s_command_error_count,
        (unsigned long)s_rx_invalid_bytes,
        (unsigned long)s_rx_line_overflow,
        (unsigned long)s_tx_error_count,
        (unsigned long)s_tx_truncated_count);
}

static void send_telemetry(uint32_t now_ms)
{
    app_state_snapshot_t app_snapshot;
    line_track_output_t line_snapshot;
    bool running;
    uint32_t snapshot_time_ms;
    uint32_t raw_imu_age_ms;
    bool raw_imu_valid;
    const char *reason;
    app_ble_debug_view_t view;
    uint32_t sequence;

    if (!read_snapshot(&app_snapshot, &line_snapshot,
                       &running, &snapshot_time_ms)) {
        tx_text("#WAIT,no_snapshot\r\n");
        return;
    }

    raw_imu_valid = (app_snapshot.imu.timestamp_ms != 0U);
    raw_imu_age_ms = raw_imu_valid ?
        (snapshot_time_ms - app_snapshot.imu.timestamp_ms) : UINT32_MAX;
    reason = diagnose(&app_snapshot, &line_snapshot, running,
                      now_ms - snapshot_time_ms,
                      raw_imu_valid, raw_imu_age_ms);
    sequence = ++s_sequence;

    view = s_log_view;
    if (view == APP_BLE_VIEW_ALL) {
        view = s_all_next_view;
        s_all_next_view++;
        if (s_all_next_view >= APP_BLE_VIEW_ALL) {
            s_all_next_view = APP_BLE_VIEW_CORE;
        }
    }

    switch (view) {
    case APP_BLE_VIEW_CORE:
        send_core(sequence, now_ms, snapshot_time_ms,
                  &app_snapshot, &line_snapshot, running, reason);
        break;
    case APP_BLE_VIEW_MODEL:
        send_model(sequence, snapshot_time_ms, &line_snapshot, reason);
        break;
    case APP_BLE_VIEW_IMU:
        send_imu(sequence, snapshot_time_ms, &app_snapshot, &line_snapshot,
                 raw_imu_valid, raw_imu_age_ms, reason);
        break;
    case APP_BLE_VIEW_MOTOR:
        send_motor(sequence, snapshot_time_ms,
                   &app_snapshot, &line_snapshot, reason);
        break;
    case APP_BLE_VIEW_SYSTEM:
        send_system(sequence, now_ms, snapshot_time_ms, reason);
        break;
    default:
        break;
    }
}

static void send_params(void)
{
    const line_track_params_t *params = app_line_track_get_params();

    if (params == NULL) {
        tx_text("#ERR,param=UNAVAILABLE\r\n");
        return;
    }
    tx_printf(
        "#PARAM,running=%u,speed=%.2f,fwd=%.2f,tmin=%.2f,tmid=%.2f,tmax=%.2f,t90=%.2f\r\n",
        app_line_track_is_running() ? 1U : 0U,
        (double)params->base_speed,
        (double)params->forward_limit,
        (double)params->turn_min_angle,
        (double)params->turn_mid_angle,
        (double)params->turn_max_angle,
        (double)params->turn90_angle);
}

static void send_model_config(void)
{
    tx_printf(
        "#CFG,model=%u,scurve=%u,gyro=%u,Ls=%.4f,errU=%.5f,errS=%.1f,gyroS=%.1f,D=%.4f,b=%.4f,kmax=%.2f,alat=%.2f,vmin=%.2f,acc=%.1f,jerk=%.1f,krate=%.2f,fbmax=%.1f,tslew=%.1f\r\n",
        (unsigned int)LINE_TRACK_MODEL_CONTROL_ENABLE,
        (unsigned int)LINE_TRACK_MODEL_SPEED_SCURVE_ENABLE,
        (unsigned int)LINE_TRACK_MODEL_GYRO_RATE_ENABLE,
        (double)LINE_TRACK_MODEL_SENSOR_FORWARD_M,
        (double)LINE_TRACK_MODEL_ERROR_UNIT_M,
        (double)LINE_TRACK_MODEL_ERROR_SIGN,
        (double)LINE_TRACK_MODEL_GYRO_SIGN,
        (double)LINE_TRACK_MODEL_WHEEL_DIAMETER_M,
        (double)LINE_TRACK_MODEL_EFFECTIVE_WHEEL_BASE_M,
        (double)LINE_TRACK_MODEL_MAX_CURVATURE_M_INV,
        (double)LINE_TRACK_MODEL_MAX_LATERAL_ACCEL_M_S2,
        (double)LINE_TRACK_MODEL_MIN_SPEED_RATIO,
        (double)LINE_TRACK_MODEL_MAX_ACCEL_RPM_S,
        (double)LINE_TRACK_MODEL_MAX_JERK_RPM_S2,
        (double)LINE_TRACK_MODEL_YAW_RATE_KP_RPM_PER_RAD_S,
        (double)LINE_TRACK_MODEL_YAW_RATE_FB_MAX_RPM,
        (double)LINE_TRACK_MODEL_TURN_SLEW_RPM_PER_S);
}

static void send_status(void)
{
    proto_uart1_a_diag_t uart_diag;

    (void)memset(&uart_diag, 0, sizeof(uart_diag));
    (void)proto_uart1_a_get_diag(&uart_diag);
    tx_printf(
        "#STATUS,log=%u,period=%lu,view=%s,snapshot=%u,seq=%lu,rx=%lu,ovf=%lu,cmd=%lu,cerr=%lu,txerr=%lu,trunc=%lu\r\n",
        s_log_enabled ? 1U : 0U,
        (unsigned long)s_log_period_ms,
        view_name(s_log_view),
        s_snapshot_valid ? 1U : 0U,
        (unsigned long)s_sequence,
        (unsigned long)uart_diag.rx_bytes,
        (unsigned long)uart_diag.rx_overflow,
        (unsigned long)s_command_count,
        (unsigned long)s_command_error_count,
        (unsigned long)s_tx_error_count,
        (unsigned long)s_tx_truncated_count);
}

static void send_help(void)
{
    tx_text("#HELP,LOG ON|OFF|ONCE; PERIOD 300..5000; VIEW CORE|MODEL|IMU|MOTOR|SYS|ALL\r\n");
    tx_text("#HELP,STATUS; PARAM; CFG; DEFAULT; SET SPEED|FWD|TURNMIN|TURNMID|TURNMAX|TURN90 value\r\n");
    tx_text("#HELP,SET和DEFAULT仅允许循迹停止时使用；每条命令以回车或换行结束\r\n");
}

static bool parse_u32_value(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;

    if (text == NULL || value == NULL || *text == '\0') {
        return false;
    }
    parsed = strtoul(text, &end, 10);
    while (*end == ' ') {
        end++;
    }
    if (*end != '\0' || parsed > UINT32_MAX) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool parse_float_value(const char *text, float *value)
{
    char *end;
    float parsed;

    if (text == NULL || value == NULL || *text == '\0') {
        return false;
    }
    parsed = strtof(text, &end);
    while (*end == ' ') {
        end++;
    }
    if (*end != '\0' || !isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool set_line_param(const char *name, float value)
{
    line_track_params_t *params = app_line_track_get_params();
    line_track_params_t next;

    if (params == NULL || name == NULL || value < 0.0f ||
        value > PRJ_PLANNER_MAX_RPM) {
        return false;
    }
    next = *params;
    if (strcmp(name, "SPEED") == 0) {
        next.base_speed = value;
    } else if (strcmp(name, "FWD") == 0) {
        next.forward_limit = value;
    } else if (strcmp(name, "TURNMIN") == 0) {
        next.turn_min_angle = value;
    } else if (strcmp(name, "TURNMID") == 0) {
        next.turn_mid_angle = value;
    } else if (strcmp(name, "TURNMAX") == 0) {
        next.turn_max_angle = value;
    } else if (strcmp(name, "TURN90") == 0) {
        next.turn90_angle = value;
    } else {
        return false;
    }

    if (next.turn_min_angle > next.turn_mid_angle ||
        next.turn_mid_angle > next.turn_max_angle ||
        next.turn_max_angle > next.turn90_angle) {
        return false;
    }
    *params = next;
    return true;
}

static void command_error(const char *reason)
{
    count_u32(&s_command_error_count);
    tx_printf("#ERR,%s\r\n", (reason != NULL) ? reason : "BAD_CMD");
}

static void process_set_command(char *arguments)
{
    char *separator;
    float value;

    if (arguments == NULL) {
        command_error("SET_FORMAT");
        return;
    }
    separator = strchr(arguments, ' ');
    if (separator == NULL) {
        command_error("SET_FORMAT");
        return;
    }
    *separator = '\0';
    while (*(separator + 1) == ' ') {
        separator++;
    }
    if (app_line_track_is_running()) {
        command_error("STOP_TRACK_FIRST");
        return;
    }
    if (!parse_float_value(separator + 1, &value) ||
        !set_line_param(arguments, value)) {
        command_error("BAD_PARAM_OR_ORDER");
        return;
    }
    tx_printf("#OK,SET %s=%.2f\r\n", arguments, (double)value);
    send_params();
}

static void process_command(char *command)
{
    char *start = command;
    size_t length;

    while (*start == ' ') {
        start++;
    }
    length = strlen(start);
    while (length > 0U && start[length - 1U] == ' ') {
        start[--length] = '\0';
    }
    if (length == 0U) {
        return;
    }
    for (size_t i = 0U; i < length; i++) {
        start[i] = (char)toupper((unsigned char)start[i]);
    }
    count_u32(&s_command_count);

    if (strcmp(start, "HELP") == 0 || strcmp(start, "?") == 0) {
        send_help();
    } else if (strcmp(start, "LOG ON") == 0) {
        s_log_enabled = true;
        tx_text("#OK,LOG=ON\r\n");
    } else if (strcmp(start, "LOG OFF") == 0) {
        s_log_enabled = false;
        tx_text("#OK,LOG=OFF\r\n");
    } else if (strcmp(start, "LOG ONCE") == 0) {
        s_log_once = true;
        tx_text("#OK,LOG=ONCE\r\n");
    } else if (strncmp(start, "PERIOD ", 7U) == 0) {
        uint32_t period_ms;
        if (!parse_u32_value(start + 7, &period_ms) ||
            period_ms < APP_UART1_BLE_DEBUG_PERIOD_MIN_MS ||
            period_ms > APP_UART1_BLE_DEBUG_PERIOD_MAX_MS) {
            command_error("PERIOD_RANGE=300..5000");
        } else {
            s_log_period_ms = period_ms;
            tx_printf("#OK,PERIOD=%lu\r\n", (unsigned long)period_ms);
        }
    } else if (strncmp(start, "VIEW ", 5U) == 0) {
        app_ble_debug_view_t view;
        if (!parse_view(start + 5, &view)) {
            command_error("VIEW=CORE|MODEL|IMU|MOTOR|SYS|ALL");
        } else {
            s_log_view = view;
            s_all_next_view = APP_BLE_VIEW_CORE;
            tx_printf("#OK,VIEW=%s\r\n", view_name(view));
        }
    } else if (strcmp(start, "STATUS") == 0) {
        send_status();
    } else if (strcmp(start, "PARAM") == 0) {
        send_params();
    } else if (strcmp(start, "CFG") == 0) {
        send_model_config();
    } else if (strcmp(start, "DEFAULT") == 0) {
        if (app_line_track_is_running()) {
            command_error("STOP_TRACK_FIRST");
        } else {
            app_line_track_restore_default_params();
            tx_text("#OK,PARAM=DEFAULT\r\n");
            send_params();
        }
    } else if (strncmp(start, "SET ", 4U) == 0) {
        process_set_command(start + 4);
    } else {
        command_error("UNKNOWN,use=HELP");
    }
}

static void process_rx(void)
{
    uint8_t byte;

    while (proto_uart1_a_getc(&byte) == BSP_OK) {
        if (byte == '\r' || byte == '\n') {
            if (s_rx_line_discard) {
                count_u32(&s_rx_line_overflow);
                count_u32(&s_command_error_count);
                tx_text("#ERR,LINE_TOO_LONG\r\n");
            } else if (s_rx_line_length > 0U) {
                s_rx_line[s_rx_line_length] = '\0';
                process_command(s_rx_line);
            }
            s_rx_line_length = 0U;
            s_rx_line_discard = false;
        } else if (byte >= 0x20U && byte <= 0x7EU) {
            if (!s_rx_line_discard &&
                s_rx_line_length < (sizeof(s_rx_line) - 1U)) {
                s_rx_line[s_rx_line_length++] = (char)byte;
            } else {
                s_rx_line_discard = true;
            }
        } else {
            count_u32(&s_rx_invalid_bytes);
        }
    }
}

static void app_uart1_ble_debug_task(void *param)
{
    uint32_t last_send_ms;

    (void)param;
    last_send_ms = osal_ticks_to_ms(osal_get_tick_count());
    tx_printf("#READY,ble_debug=2,baud=%lu,period=%lu,view=%s,map=L:D/R:A,use=HELP\r\n",
              (unsigned long)PRJ_UART1_BLE_DEBUG_UART_BAUDRATE,
              (unsigned long)s_log_period_ms,
              view_name(s_log_view));

    for (;;) {
        uint32_t now_ms;
        process_rx();
        now_ms = osal_ticks_to_ms(osal_get_tick_count());
        if (s_log_once ||
            (s_log_enabled && (uint32_t)(now_ms - last_send_ms) >=
                              s_log_period_ms)) {
            s_log_once = false;
            last_send_ms = now_ms;
            send_telemetry(now_ms);
        }
        osal_task_delay_ms(APP_UART1_BLE_DEBUG_POLL_MS);
    }
}

int32_t app_uart1_ble_debug_init(void)
{
    if (s_initialized) {
        return 0;
    }
    if (proto_uart1_a_init() != BSP_OK) {
        return -1;
    }

    s_log_enabled = true;
    s_log_once = false;
    s_log_period_ms = PRJ_UART1_BLE_DEBUG_PERIOD_MS;
    if (s_log_period_ms < APP_UART1_BLE_DEBUG_PERIOD_MIN_MS) {
        s_log_period_ms = APP_UART1_BLE_DEBUG_PERIOD_MIN_MS;
    } else if (s_log_period_ms > APP_UART1_BLE_DEBUG_PERIOD_MAX_MS) {
        s_log_period_ms = APP_UART1_BLE_DEBUG_PERIOD_MAX_MS;
    }
    s_log_view = APP_BLE_VIEW_ALL;
    s_all_next_view = APP_BLE_VIEW_CORE;
    s_sequence = 0U;
    s_command_count = 0U;
    s_command_error_count = 0U;
    s_rx_invalid_bytes = 0U;
    s_rx_line_overflow = 0U;
    s_tx_error_count = 0U;
    s_tx_truncated_count = 0U;
    s_rx_line_length = 0U;
    s_rx_line_discard = false;
    s_snapshot_valid = false;
    s_initialized = true;

    s_task_handle = osal_task_create(app_uart1_ble_debug_task,
        APP_UART1_BLE_DEBUG_TASK_NAME,
        PRJ_TASK_STACK_UART1_BLE_DEBUG,
        NULL,
        PRJ_TASK_PRIORITY_UART1_BLE_DEBUG);
    if (s_task_handle == NULL) {
        s_initialized = false;
        proto_uart1_a_deinit();
        return -2;
    }
    return 0;
}

void app_uart1_ble_debug_irq_handler(void)
{
    proto_uart1_a_irq_handler();
}

void app_uart1_ble_debug_update_line(uint32_t now_ms,
                                     bool running,
                                     const line_track_output_t *line,
                                     const app_shared_ctx_t *ctx)
{
    if (s_initialized) {
        copy_snapshot(now_ms, running, line, ctx);
    }
}
