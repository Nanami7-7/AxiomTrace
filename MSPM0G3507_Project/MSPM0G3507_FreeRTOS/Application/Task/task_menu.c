/**
 * @file    task_menu.c
 * 说明：菜单命令相关处理。
 * 说明：菜单命令相关处理。
 * 说明：菜单命令相关处理。
 * 说明：菜单命令相关处理。
 */
#include "task_menu.h"
#include "app_main.h"
#include "app_pid.h"
#include "app_feedforward.h"
#include "app_vofa.h"
#include "app_complementary_filter.h"
#include "osal_api.h"
#include "bsp_led.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "bsp_uart.h"
#include "app_debug.h"
#include "app_encoder_telemetry.h"
#include "project_config.h"
#include "app_test_runner.h"
#include "axiomtrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================== DRV 示波器控制 ======================== */

/* 说明：菜单命令相关处理。 */
static const char *s_motor_names[BSP_MOTOR_COUNT] = {
    "A", "B", "C", "D"
};

/** LED 闪烁周期(ms) */
#define MENU_LED_PERIOD_MS  (100U)

/* 说明：菜单命令相关处理。 */
#define LED_TOGGLE_THRESH   (500U)

/* 说明：菜单命令相关处理。 */

/**
 * 说明：菜单命令相关处理。
 * @param  line_buf  命令行缓冲区
 * 说明：菜单命令相关处理。
 * 说明：菜单命令相关处理。
 */
typedef enum {
    MENU_DRVSCOPE_START = 0,
    MENU_DRVSCOPE_SET_ONE,
    MENU_DRVSCOPE_SET_ALL,
    MENU_DRVSCOPE_STATUS,
    MENU_DRVSCOPE_OFF,
} menu_drvscope_action_t;

typedef struct {
    menu_drvscope_action_t action;
    uint32_t motor_id;
    uint32_t duty_permille; /* 0..1000 = 0.0..100.0% */
} menu_drvscope_cmd_t;

static char menu_ascii_lower(char ch);

/** Continuous IMU stream state; accessed only by the menu task. */
static bool s_imu_stream_enabled = false;
static uint32_t s_imu_stream_next_ms = 0U;

/**
 * @brief Send one IMU frame through UART0 DMA.
 * @param ctx Shared application context.
 * @param report_errors Print one-shot diagnostic messages on failure.
 * @return true when a DMA transfer was accepted.
 * @details Exactly ten CSV fields are sent: ax, ay, az (g), gx, gy, gz (dps),
 *          roll, pitch, yaw (deg), and temperature (degC). Continuous output
 *          intentionally drops a frame when DMA is busy instead of blocking
 *          the menu task or allowing frames to queue indefinitely.
 */
static bool menu_send_imu_frame_dma(const app_shared_ctx_t *ctx,
                                    bool report_errors)
{
    static char tx_buf[PRJ_IMU_SNAPSHOT_DMA_BUF_SIZE];
    app_imu_data_t sample;
    int len;
    bsp_status_t status;

    if (ctx == NULL) {
        return false;
    }

    OSAL_CRITICAL_SECTION {
        sample = ctx->imu;
    }

    if (!bsp_uart_tx_idle()) {
        if (report_errors) {
            (void)printf("[IMU] UART0 DMA busy; snapshot dropped\r\n");
        }
        return false;
    }

    len = snprintf(tx_buf, sizeof(tx_buf),
        "%.5f,%.5f,%.5f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f\r\n",
        (double)sample.accel_x_g,
        (double)sample.accel_y_g,
        (double)sample.accel_z_g,
        (double)sample.gyro_x_dps,
        (double)sample.gyro_y_dps,
        (double)sample.gyro_z_dps,
        (double)sample.roll,
        (double)sample.pitch,
        (double)sample.yaw,
        (double)sample.temperature);

    if (len <= 0 || (uint32_t)len >= sizeof(tx_buf)) {
        if (report_errors) {
            (void)printf("[IMU] snapshot formatting failed\r\n");
        }
        return false;
    }

    status = bsp_uart_send_dma((const uint8_t *)tx_buf, (uint16_t)len);
    if (status != BSP_OK) {
        if (report_errors) {
            (void)printf("[IMU] DMA send failed: %d\r\n", (int)status);
        }
        return false;
    }

    return true;
}

static void menu_process_imu_stream(const app_shared_ctx_t *ctx,
                                    uint32_t now_ms)
{
    if (!s_imu_stream_enabled) {
        return;
    }

    if ((int32_t)(now_ms - s_imu_stream_next_ms) < 0) {
        return;
    }

    /* Schedule from the current time so a delayed menu task never bursts. */
    s_imu_stream_next_ms = now_ms + PRJ_IMU_STREAM_PERIOD_MS;
    (void)menu_send_imu_frame_dma(ctx, false);
}

/** Parse: encdiag cap [A|B|C|D|ALL] [window_ms]. */
static bool menu_parse_encdiag_capture(const char *line,
                                       uint32_t *motor_id,
                                       uint32_t *duration_ms)
{
    if ((line == NULL) || (motor_id == NULL) || (duration_ms == NULL)) {
        return false;
    }

    if (strcmp(line, "encdiag cap") == 0) {
        *motor_id = BSP_ENCODER_COUNT;
        *duration_ms = 1000U;
        return true;
    }

    char target[8] = {0};
    unsigned long window = 1000UL;
    int parsed = sscanf(line, "encdiag cap %7s %lu", target, &window);
    if (parsed < 1) {
        return false;
    }

    for (uint32_t i = 0U; target[i] != '\0'; i++) {
        target[i] = menu_ascii_lower(target[i]);
    }

    if (strcmp(target, "all") == 0) {
        *motor_id = BSP_ENCODER_COUNT;
    } else if ((target[0] >= 'a') && (target[0] <= 'd') &&
               (target[1] == '\0')) {
        *motor_id = (uint32_t)(target[0] - 'a');
    } else {
        return false;
    }

    if (parsed >= 2) {
        if (window < 100UL) {
            window = 100UL;
        }
        if (window > 10000UL) {
            window = 10000UL;
        }
    }
    *duration_ms = (uint32_t)window;
    return true;
}

static void menu_print_encdiag_capture_usage(void)
{
    (void)printf("Usage: encdiag cap [A|B|C|D|ALL] [100..10000_ms]\r\n");
    (void)printf("Example: encdiag cap A 1000\r\n");
}

static char menu_ascii_lower(char ch)
{
    if ((ch >= 'A') && (ch <= 'Z')) {
        return (char)(ch + ('a' - 'A'));
    }
    return ch;
}

static bool menu_token_equal_ci(const char *lhs, const char *rhs)
{
    if ((lhs == NULL) || (rhs == NULL)) {
        return false;
    }
    while ((*lhs != '\0') && (*rhs != '\0')) {
        if (menu_ascii_lower(*lhs) != menu_ascii_lower(*rhs)) {
            return false;
        }
        lhs++;
        rhs++;
    }
    return (*lhs == '\0') && (*rhs == '\0');
}

static bool menu_next_token(const char **cursor, char *token,
                            uint32_t token_size)
{
    const char *p;
    uint32_t length = 0U;

    if ((cursor == NULL) || (*cursor == NULL) ||
        (token == NULL) || (token_size < 2U)) {
        return false;
    }
    p = *cursor;
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0') {
        *cursor = p;
        return false;
    }
    while ((*p != '\0') && (*p != ' ')) {
        if (length >= (token_size - 1U)) {
            return false;
        }
        token[length++] = *p++;
    }
    token[length] = '\0';
    *cursor = p;
    return true;
}

static bool menu_parse_duty_permille(const char *token,
                                      uint32_t *duty_permille)
{
    uint32_t whole = 0U;
    uint32_t decimal = 0U;
    const char *p = token;

    if ((token == NULL) || (duty_permille == NULL) ||
        (*p < '0') || (*p > '9')) {
        return false;
    }
    while ((*p >= '0') && (*p <= '9')) {
        whole = (whole * 10U) + (uint32_t)(*p - '0');
        if (whole > 100U) {
            return false;
        }
        p++;
    }
    if (*p == '.') {
        p++;
        if ((*p < '0') || (*p > '9')) {
            return false;
        }
        decimal = (uint32_t)(*p - '0');
        p++;
    }
    if ((*p != '\0') || ((whole == 100U) && (decimal != 0U))) {
        return false;
    }

    *duty_permille = (whole * 10U) + decimal;
    return true;
}

static bool menu_parse_drvscope(const char *line_buf,
                                menu_drvscope_cmd_t *cmd)
{
    const char *cursor = line_buf;
    char command[12];
    char arg1[12];
    char arg2[12];
    char extra[PRJ_MENU_LINE_BUF_SIZE];

    if ((line_buf == NULL) || (cmd == NULL) ||
        !menu_next_token(&cursor, command, sizeof(command)) ||
        !menu_token_equal_ci(command, "drvscope") ||
        !menu_next_token(&cursor, arg1, sizeof(arg1))) {
        return false;
    }

    if (menu_token_equal_ci(arg1, "start") ||
        menu_token_equal_ci(arg1, "status") ||
        menu_token_equal_ci(arg1, "off") ||
        menu_token_equal_ci(arg1, "stop")) {
        if (menu_next_token(&cursor, extra, sizeof(extra))) {
            return false;
        }
        if (menu_token_equal_ci(arg1, "start")) {
            cmd->action = MENU_DRVSCOPE_START;
        } else if (menu_token_equal_ci(arg1, "status")) {
            cmd->action = MENU_DRVSCOPE_STATUS;
        } else {
            cmd->action = MENU_DRVSCOPE_OFF;
        }
        return true;
    }

    if (!menu_next_token(&cursor, arg2, sizeof(arg2)) ||
        menu_next_token(&cursor, extra, sizeof(extra)) ||
        !menu_parse_duty_permille(arg2, &cmd->duty_permille)) {
        return false;
    }
    if (menu_token_equal_ci(arg1, "all")) {
        cmd->action = MENU_DRVSCOPE_SET_ALL;
        return true;
    }
    if ((arg1[1] == '\0') &&
        (menu_ascii_lower(arg1[0]) >= 'a') &&
        (menu_ascii_lower(arg1[0]) <= 'd')) {
        cmd->action = MENU_DRVSCOPE_SET_ONE;
        cmd->motor_id = (uint32_t)(menu_ascii_lower(arg1[0]) - 'a');
        return true;
    }
    return false;
}

static void menu_print_drvscope_usage(void)
{
    (void)printf("Usage:\r\n");
    (void)printf("  drvscope start       - all PWM=50.0%%, then PB19 ON (persistent)\r\n");
    (void)printf("  drvscope A 80        - A/M1 direct PWM duty=80.0%%\r\n");
    (void)printf("  drvscope B 20.5      - B/M2 direct PWM duty=20.5%%\r\n");
    (void)printf("  drvscope all 50      - set all four channels to 50.0%%\r\n");
    (void)printf("  drvscope status      - print PB19 and all compare values\r\n");
    (void)printf("  drvscope off|stop    - all PWM neutral, PB19 OFF, release lock\r\n");
}

static bool menu_read_line(char *line_buf, uint32_t buf_size,
                             uint32_t *line_pos)
{
    uint8_t ch;

    while (bsp_uart_getc(&ch) == BSP_OK) {
        if (ch == '\r' || ch == '\n') {
            line_buf[*line_pos] = '\0';
            (void)printf("\r\n");
            *line_pos = 0U;
            return true;
        } else if (ch == 0x7FU || ch == 0x08U) {
            if (*line_pos > 0U) {
                (*line_pos)--;
                (void)printf("\b \b");
            }
        } else if (ch >= 0x20U && ch < 0x7FU) {
            if (*line_pos < (buf_size - 1U)) {
                line_buf[*line_pos] = (char)ch;
                (*line_pos)++;
                (void)bsp_uart_putc(ch);
            }
        }
    }

    return false;
}

/* 说明：菜单命令相关处理。 */

/**
 * 说明：菜单命令相关处理。
 */
static void menu_print_status(const app_shared_ctx_t *ctx,
                               uint32_t motor)
{
    (void)printf("\r\n=== Motor: %s ===\r\n", s_motor_names[motor]);

    /* 当前目标RPM */
    {
        float sp;
        OSAL_CRITICAL_SECTION {
            sp = ctx->pid[motor].setpoint;
        }
        (void)printf("Target: %.0f RPM\r\n", (double)sp);
    }

    /* 说明：菜单命令相关处理。 */
    for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
        float kp, ki, kd;
        OSAL_CRITICAL_SECTION {
            kp = ctx->pid[i].kp;
            ki = ctx->pid[i].ki;
            kd = ctx->pid[i].kd;
        }
        (void)printf("%c: KP=%.2f KI=%.2f KD=%.2f\r\n",
            'A' + (int)i,
            (double)kp, (double)ki, (double)kd);
    }

    /* 说明：菜单命令相关处理。 */
    {
        float ff_k, ff_b, ff_kp, ff_ki, ff_kd;
        bool ff_en;
        OSAL_CRITICAL_SECTION {
            ff_k = ctx->ff[motor].k;
            ff_b = ctx->ff[motor].b;
            ff_en = ctx->ff[motor].enabled;
            ff_kp = ctx->pid[motor].ff_kp;
            ff_ki = ctx->pid[motor].ff_ki;
            ff_kd = ctx->pid[motor].ff_kd;
        }
        if (ff_en) {
            (void)printf("FF: ON  k=%.3f  b=%.1f\r\n",
                (double)ff_k, (double)ff_b);
            (void)printf("FF_PID: KP=%.2f KI=%.2f KD=%.2f\r\n",
                (double)ff_kp, (double)ff_ki, (double)ff_kd);
        } else {
            (void)printf("FF: OFF\r\n");
        }
    }

    /* 说明：菜单命令相关处理。 */
    {
        bool en;
        int32_t rpm;
        float current_ma;
        OSAL_CRITICAL_SECTION {
            en = ctx->motor_enabled[motor];
            rpm = ctx->status.rpm[motor];
            current_ma = ctx->status.current_ma[motor];
        }
        if (en) {
            (void)printf("Motor: running (%ld RPM) | I=%.0fmA\r\n",
                (long)rpm, (double)current_ma);
        } else {
            (void)printf("Motor: stopped | I=%.0fmA\r\n",
                (double)current_ma);
        }
    }

    /* 说明：菜单命令相关处理。 */
    {
        float roll, pitch, yaw, heading, vx;
        OSAL_CRITICAL_SECTION {
            roll  = ctx->imu.roll;
            pitch = ctx->imu.pitch;
            yaw   = ctx->imu.yaw;
        }
        heading = app_cf_get_heading();
        vx      = app_cf_get_velocity_x();
        (void)printf("IMU: R=%.1f P=%.1f Y=%.1f H=%.1f V=%.3f\r\n",
            (double)roll, (double)pitch, (double)yaw,
            (double)heading, (double)vx);
    }
    /* Print the latest power snapshot after copying it under the critical section. */
    {
        uint32_t bus_mv;
        float currents[BSP_MOTOR_COUNT];
        OSAL_CRITICAL_SECTION {
            bus_mv = ctx->status.bus_voltage_mv;
            for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                currents[i] = ctx->status.current_ma[i];
            }
        }
        (void)printf("PWR: VBUS=%lumV I=[", (unsigned long)bus_mv);
        for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
            (void)printf("%.0f%s", (double)currents[i],
                (i + 1U < BSP_MOTOR_COUNT) ? " " : "");
        }
        (void)printf("]mA\r\n");
    }
}

/* 说明：菜单命令相关处理。 */

/**
 * 说明：菜单命令相关处理。
 * 说明：菜单命令相关处理。
 * 说明：菜单命令相关处理。
 */
static void menu_data_output_loop(app_shared_ctx_t *ctx,
                                   uint32_t *motor,
                                   bool *need_refresh)
{
    char line_buf[PRJ_MENU_LINE_BUF_SIZE];
    uint32_t line_pos = 0U;
    uint32_t led_cnt = 0U;

    (void)printf("[DATA] VOFA+ output started. Send Stop to exit.\r\n");

    for (;;) {
        /* 说明：菜单命令相关处理。 */
        if (menu_read_line(line_buf, PRJ_MENU_LINE_BUF_SIZE, &line_pos)) {
            vofa_cmd_t cmd;
            if (app_vofa_parse_cmd(line_buf, &cmd)) {
                app_vofa_apply_cmd(&cmd, ctx, motor, need_refresh);

                if (cmd.type == VOFA_CMD_STOP ||
                    cmd.type == VOFA_CMD_STOP_ALL ||
                    cmd.type == VOFA_CMD_ABORT ||
                    cmd.type == VOFA_CMD_STREAM_OFF) {
                    app_encoder_telemetry_stop();
                }

                /* 说明：菜单命令相关处理。 */
                if (cmd.type == VOFA_CMD_STOP ||
                    cmd.type == VOFA_CMD_STOP_ALL ||
                    cmd.type == VOFA_CMD_STREAM_OFF) {
                    (void)printf("[DATA] VOFA+ output stopped.\r\n");
                    return;
                }
                /* 说明：菜单命令相关处理。 */
            }
        }

        /* 说明：菜单命令相关处理。 */
        {
            float channels[VOFA_TELEMETRY_CHANNEL_COUNT];
            OSAL_CRITICAL_SECTION {
                for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                    channels[i] = (float)ctx->status.rpm[i];
                    channels[4 + i] = ctx->pid[i].setpoint;
                }
                /* CH8: FF duty(选中电机) */
                if (ctx->ff[*motor].enabled) {
                    channels[8] = app_ff_compute(
                        &ctx->ff[*motor], ctx->pid[*motor].setpoint);
                } else {
                    channels[8] = 0.0f;
                }
                /* 说明：菜单命令相关处理。 */
                channels[9] = ctx->status.pid_correction[*motor];
                /* 说明：菜单命令相关处理。 */
                channels[10] = (float)ctx->status.output[*motor];
            }

            /* 说明：菜单命令相关处理。 */
            char tx_buf[180];
            int len = 0;
            for (uint32_t i = 0U; i < VOFA_TELEMETRY_CHANNEL_COUNT; i++) {
                if (i > 0U) {
                    tx_buf[len] = ',';
                    len++;
                }
                int ret = snprintf(&tx_buf[len],
                    sizeof(tx_buf) - (uint32_t)len,
                    "%.6f", (double)channels[i]);
                if (ret < 0 || (uint32_t)ret >= sizeof(tx_buf) - (uint32_t)len) {
                    len = 0;  /* 说明：菜单命令相关处理。 */
                    break;
                }
                len += ret;
            }
            if (len > 0 && len < (int)sizeof(tx_buf)) {
                tx_buf[len] = '\n';
                len++;
                /* 说明：菜单命令相关处理。 */
                if (bsp_uart_tx_idle()) {
                    (void)bsp_uart_send_dma((uint8_t *)tx_buf, (uint16_t)len);
                }
            }
        }

        /* 说明：菜单命令相关处理。 */
        led_cnt += PRJ_RPM_OUTPUT_PERIOD_MS;
        if (led_cnt >= LED_TOGGLE_THRESH) {
            led_cnt = 0U;
            bsp_led_toggle();
        }

        osal_task_delay_ms(PRJ_RPM_OUTPUT_PERIOD_MS);
    }
}

/* 说明：菜单命令相关处理。 */



void app_menu_task(void *param)
{
    app_shared_ctx_t *ctx = (app_shared_ctx_t *)param;
    uint32_t selected_motor = 0U;
    char line_buf[PRJ_MENU_LINE_BUF_SIZE];
    uint32_t line_pos = 0U;
    uint32_t led_cnt = 0U;
    bool need_refresh = true;
    uint32_t now_ms;

    (void)printf("\r\n=== MSPM0G3507 Motor Control ===\r\n");
    (void)printf("Send VOFA+ commands to control.\r\n");
    (void)printf("Type 'bench' to run MATHACL benchmark.\r\n");
    (void)printf("Type 'encdiag' for one read-only hardware/encoder snapshot.\r\n");
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0U)
    (void)printf("Type 'encdiag cap [A|B|C|D|ALL] [ms]' for capture-edge diagnostics.\r\n");
#endif
    (void)printf("Type 'drvscope start' for the persistent DRV8870 oscilloscope session.\r\n");
    (void)printf("Type 'drvscope status' for scope command help/status.\r\n");
    (void)printf("Type 'mathdiag' to run MATHACL hardware diagnostic.\r\n");
    (void)printf("Type 'zutptest N' to run N-sec ZUPT drift test.\r\n");
    (void)printf("  - Outputs: yaw drift/std/range, bias_z tracking, fault detect, verdict\r\n");
    (void)printf("Type 'turndtest ANG [T]' to start turn test (e.g. turndtest 90).\r\n");
    (void)printf("  - Outputs: angle error, turn/stable phase stats, bias_z gating, verdict\r\n");
    (void)printf("Type 'turnend' to finish turn (after reaching target angle).\r\n");
    (void)printf("Type 'kftune N' to run KF parameter sweep (N sec/set, 9 sets).\r\n");
    (void)printf("Type 'rtosdiag' to show FreeRTOS stack/heap diagnostics.\r\n");
    (void)printf("  - Sweeps Q_angle x Q_bias, outputs drift/std/jump, finds best params\r\n");
    (void)printf("Type 'imu' to continuously send 10-field IMU CSV via UART0 DMA every 200 ms.\r\n");
    (void)printf("Type 'ir' to read four infrared channels once.\r\n");
    (void)printf("Send another non-empty menu command to stop IMU streaming.\r\n");

    for (;;) {
        now_ms = osal_ticks_to_ms(osal_get_tick_count());
        menu_process_imu_stream(ctx, now_ms);
        /* 说明：菜单命令相关处理。 */
        if (need_refresh) {
            menu_print_status(ctx, selected_motor);
            need_refresh = false;
        }

        /* 说明：菜单命令相关处理。 */
        if (menu_read_line(line_buf, PRJ_MENU_LINE_BUF_SIZE, &line_pos)) {
            /*
             * 说明：菜单命令相关处理。
             * 说明：菜单命令相关处理。
             * 说明：菜单命令相关处理。
             */
            if (line_buf[0] != '\0') {
                menu_drvscope_cmd_t scope_cmd;
                if (strcmp(line_buf, "imu") == 0) {
                    /* `imu` is the only stream command: start/restart at 200 ms. */
                    s_imu_stream_enabled = true;
                    s_imu_stream_next_ms = now_ms;
                    menu_process_imu_stream(ctx, now_ms);
                    need_refresh = false;
                } else {
                    /* Preserve the old console behavior: another command stops IMU output. */
                    s_imu_stream_enabled = false;
            if (menu_parse_drvscope(line_buf, &scope_cmd)) {
                switch (scope_cmd.action) {
                case MENU_DRVSCOPE_START:
                    app_debug_drv8870_scope_start(ctx);
                    break;
                case MENU_DRVSCOPE_SET_ONE:
                    app_debug_drv8870_scope_set(scope_cmd.motor_id,
                                                  scope_cmd.duty_permille);
                    break;
                case MENU_DRVSCOPE_SET_ALL:
                    app_debug_drv8870_scope_set_all(scope_cmd.duty_permille);
                    break;
                case MENU_DRVSCOPE_STATUS:
                    app_debug_drv8870_scope_status();
                    menu_print_drvscope_usage();
                    break;
                case MENU_DRVSCOPE_OFF:
                    app_debug_drv8870_scope_stop();
                    break;
                default:
                    menu_print_drvscope_usage();
                    break;
                }
                need_refresh = true;
            } else if (strncmp(line_buf, "drvscope", 8U) == 0) {
                menu_print_drvscope_usage();
                need_refresh = true;
            } else if (strcmp(line_buf, "encdiag") == 0) {
                app_debug_hwmap_snapshot();
                need_refresh = true;
            } else if (strncmp(line_buf, "encdiag cap", 11U) == 0) {
                uint32_t capture_motor = BSP_ENCODER_COUNT;
                uint32_t capture_window = 1000U;
                if (menu_parse_encdiag_capture(line_buf,
                                               &capture_motor,
                                               &capture_window)) {
                    app_debug_encoder_capture_diag(capture_motor,
                                                   capture_window);
                } else {
                    menu_print_encdiag_capture_usage();
                }
                need_refresh = true;
            } else if (strcmp(line_buf, "enc") == 0) {
                app_debug_encoder_stream(50);
                need_refresh = true;
            } else if (strcmp(line_buf, "rtosdiag") == 0) {
                app_runtime_diag_t runtime_diag;
                if (app_runtime_diag_read(&runtime_diag)) {
                    (void)printf("RTOS: ctrl_stack=%lu words, menu_stack=%lu words, imu_stack=%lu\r\n",
                        (unsigned long)runtime_diag.control_stack_high_watermark_words,
                        (unsigned long)runtime_diag.menu_stack_high_watermark_words,
                        (unsigned long)runtime_diag.imu_stack_high_watermark_words);
                    (void)printf("RTOS: heap_free=%lu bytes, heap_min_free=%lu bytes, fault=%lu\r\n",
                        (unsigned long)runtime_diag.free_heap_bytes,
                        (unsigned long)runtime_diag.minimum_ever_free_heap_bytes,
                        (unsigned long)runtime_diag.fault_code);
                } else {
                    (void)printf("RTOS diagnostics unavailable: invalid output buffer.\r\n");
                }
                need_refresh = true;
            } else if (strcmp(line_buf, "diag") == 0) {
                app_debug_encoder_diag(ctx, selected_motor);
                need_refresh = true;
            } else if (strcmp(line_buf, "adc") == 0) {
                app_debug_adc_test();
                need_refresh = true;
            } else if (strcmp(line_buf, "ir") == 0) {
                /* UART0 输入 ir：读取一次四路红外并打印，不进入循迹环。 */
                app_debug_ir_snapshot();
                need_refresh = true;
            } else if (strncmp(line_buf, "zutptest", 8) == 0) {
                /* 说明：菜单命令相关处理。 */
                uint32_t dur = 60;
                if (strlen(line_buf) > 9) {
                    dur = (uint32_t)atoi(&line_buf[9]);
                    if (dur == 0 || dur > 86400) dur = 60;
                }
                app_test_runner_start(dur);
            } else if (strncmp(line_buf, "turndtest", 9) == 0) {
                /* 说明：菜单命令相关处理。 */
                float target = 90.0f;
                uint32_t timeout = 60;
                /* 说明：菜单命令相关处理。 */
                char *p = &line_buf[9];
                while (*p == ' ') p++;
                if (*p != '\0') {
                    target = (float)atof(p);
                    /* 说明：菜单命令相关处理。 */
                    while (*p != '\0' && *p != ' ') p++;
                    while (*p == ' ') p++;
                    if (*p != '\0') {
                        int t = atoi(p);
                        if (t > 0 && t <= 86400) timeout = (uint32_t)t;
                    }
                }
                if (target == 0.0f) target = 90.0f;
                app_test_runner_start_turn(target, timeout);
            } else if (strcmp(line_buf, "turnend") == 0) {
                /* turnend: 手动确认转动结束 */
                app_test_runner_end_turn();
            } else if (strncmp(line_buf, "kftune", 6) == 0) {
                /* 说明：菜单命令相关处理。 */
                uint32_t dur = 10;
                if (strlen(line_buf) > 7) {
                    dur = (uint32_t)atoi(&line_buf[7]);
                    if (dur == 0 || dur > 600) dur = 10;
                }
                app_test_runner_start_kftune(dur);
            } else {
                vofa_cmd_t cmd;
                if (app_vofa_parse_cmd(line_buf, &cmd)) {
                    app_vofa_apply_cmd(&cmd, ctx, &selected_motor,
                                       &need_refresh);

                    if (cmd.type == VOFA_CMD_STOP ||
                        cmd.type == VOFA_CMD_STOP_ALL ||
                        cmd.type == VOFA_CMD_ABORT ||
                        cmd.type == VOFA_CMD_STREAM_OFF) {
                        app_encoder_telemetry_stop();
                    }

                    /* Run 命令: 进入持续数据输出 */
                    if (cmd.type == VOFA_CMD_RUN ||
                        cmd.type == VOFA_CMD_STREAM_ON) {
                        menu_data_output_loop(ctx, &selected_motor,
                                              &need_refresh);
                        /* 说明：菜单命令相关处理。 */
                        need_refresh = true;
                    }
                }
            }
        }
            }

            }

        /* 说明：菜单命令相关处理。 */
        led_cnt += MENU_LED_PERIOD_MS;
        if (led_cnt >= LED_TOGGLE_THRESH) {
            led_cnt = 0U;
            bsp_led_toggle();
        }

        osal_task_delay_ms(MENU_LED_PERIOD_MS);
    }
}

