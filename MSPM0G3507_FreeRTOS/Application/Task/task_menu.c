/**
 * @file    task_menu.c
 * @brief   菜单任务实现(CLI模式)
 * @note    CLI模式: 显示状态 → 等待命令 → 执行 → 刷新
 *          Run命令进入数据输出模式(30ms VOFA+数据)
 *          Stop命令退出数据输出模式
 */
#include "task_menu.h"
#include "app_main.h"
#include "app_pid.h"
#include "app_feedforward.h"
#include "app_vofa.h"
#include "app_complementary_filter.h"
#include "osal_api.h"
#include "bsp_led.h"
#include "bsp_drv8870.h"
#include "bsp_uart.h"
#include "app_debug.h"
#include "app_test_runner.h"
#include "axiomtrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================== 私有常量 ======================== */

/** 电机名称查找表 */
static const char *s_motor_names[BSP_MOTOR_COUNT] = {
    "A", "B", "C", "D"
};

/** LED心跳周期(ms) */
#define MENU_LED_PERIOD_MS  (100U)

/** LED翻转阈值(ms) */
#define LED_TOGGLE_THRESH   (500U)

/* ======================== 私有函数: 行输入 ======================== */

/**
 * @brief  非阻塞行输入
 * @param  line_buf  行缓冲区
 * @param  buf_size  缓冲区大小
 * @param  line_pos  当前写入位置指针(读写)
 * @retval true  一行输入完成
 * @retval false 尚未完成
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
    char extra[MENU_LINE_BUF_SIZE];

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

/* ======================== 私有函数: 状态显示 ======================== */

/**
 * @brief  打印当前状态(电机参数+FF状态+IMU数据)
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

    /* 各电机PID参数 */
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

    /* FF状态 */
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

    /* 电机运行状态 */
    {
        bool en;
        int32_t rpm;
        OSAL_CRITICAL_SECTION {
            en = ctx->motor_enabled[motor];
            rpm = ctx->status.rpm[motor];
        }
        if (en) {
            (void)printf("Motor: running (%ld RPM)\r\n", (long)rpm);
        } else {
            (void)printf("Motor: stopped\r\n");
        }
    }

    /* IMU数据 */
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
}

/* ======================== 私有函数: 数据输出循环 ======================== */

/**
 * @brief  VOFA+数据输出模式(Run后进入, Stop退出)
 * @param  ctx          共享上下文
 * @param  motor        当前电机索引指针
 * @param  need_refresh 刷新标志指针
 */
static void menu_data_output_loop(app_shared_ctx_t *ctx,
                                   uint32_t *motor,
                                   bool *need_refresh)
{
    char line_buf[MENU_LINE_BUF_SIZE];
    uint32_t line_pos = 0U;
    uint32_t led_cnt = 0U;

    (void)printf("[DATA] VOFA+ output started. Send Stop to exit.\r\n");

    for (;;) {
        /* 非阻塞检查命令 */
        if (menu_read_line(line_buf, MENU_LINE_BUF_SIZE, &line_pos)) {
            vofa_cmd_t cmd;
            if (app_vofa_parse_cmd(line_buf, &cmd)) {
                app_vofa_apply_cmd(&cmd, ctx, motor, need_refresh);

                /* Stop退出数据输出模式 */
                if (cmd.type == VOFA_CMD_STOP ||
                    cmd.type == VOFA_CMD_STOP_ALL ||
                    cmd.type == VOFA_CMD_STREAM_OFF) {
                    (void)printf("[DATA] VOFA+ output stopped.\r\n");
                    return;
                }
                /* 非Stop命令: 只打印反馈, 不刷新菜单 */
            }
        }

        /* 输出VOFA+数据(11通道, DMA非阻塞) */
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
                /* CH9: PID修正量(选中电机) */
                channels[9] = ctx->status.pid_correction[*motor];
                /* CH10: 实际DUTY输出(选中电机) */
                channels[10] = (float)ctx->status.output[*motor];
            }

            /* 格式化到临时缓冲区 */
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
                    len = 0;  /* 缓冲区不足,放弃本帧 */
                    break;
                }
                len += ret;
            }
            if (len > 0 && len < (int)sizeof(tx_buf)) {
                tx_buf[len] = '\n';
                len++;
                /* 非阻塞DMA发送：检查标志位，忙则跳过本帧 */
                if (bsp_uart_tx_idle()) {
                    (void)bsp_uart_send_dma((uint8_t *)tx_buf, (uint16_t)len);
                }
            }
        }

        /* LED心跳 */
        led_cnt += APP_RPM_OUTPUT_PERIOD_MS;
        if (led_cnt >= LED_TOGGLE_THRESH) {
            led_cnt = 0U;
            bsp_led_toggle();
        }

        osal_task_delay_ms(APP_RPM_OUTPUT_PERIOD_MS);
    }
}

/* ======================== 公共函数实现 ======================== */

void app_menu_task(void *param)
{
    app_shared_ctx_t *ctx = (app_shared_ctx_t *)param;
    uint32_t selected_motor = 0U;
    char line_buf[MENU_LINE_BUF_SIZE];
    uint32_t line_pos = 0U;
    uint32_t led_cnt = 0U;
    bool need_refresh = true;  /* 初始显示 */

    (void)printf("\r\n=== MSPM0G3507 Motor Control ===\r\n");
    (void)printf("Send VOFA+ commands to control.\r\n");
    (void)printf("Type 'bench' to run MATHACL benchmark.\r\n");
    (void)printf("Type 'encdiag' for one read-only hardware/encoder snapshot.\r\n");
    (void)printf("Type 'drvscope start' for the persistent DRV8870 oscilloscope session.\r\n");
    (void)printf("Type 'drvscope status' for scope command help/status.\r\n");
    (void)printf("Type 'mathdiag' to run MATHACL hardware diagnostic.\r\n");
    (void)printf("Type 'zutptest N' to run N-sec ZUPT drift test.\r\n");
    (void)printf("  - Outputs: yaw drift/std/range, bias_z tracking, fault detect, verdict\r\n");
    (void)printf("Type 'turndtest ANG [T]' to start turn test (e.g. turndtest 90).\r\n");
    (void)printf("  - Outputs: angle error, turn/stable phase stats, bias_z gating, verdict\r\n");
    (void)printf("Type 'turnend' to finish turn (after reaching target angle).\r\n");
    (void)printf("Type 'kftune N' to run KF parameter sweep (N sec/set, 9 sets).\r\n");
    (void)printf("  - Sweeps Q_angle x Q_bias, outputs drift/std/jump, finds best params\r\n");

    for (;;) {
        /* 刷新菜单 */
        if (need_refresh) {
            menu_print_status(ctx, selected_motor);
            need_refresh = false;
        }

        /* 检查命令 */
        if (menu_read_line(line_buf, MENU_LINE_BUF_SIZE, &line_pos)) {
            menu_drvscope_cmd_t scope_cmd;

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
            } else if (strcmp(line_buf, "enc") == 0) {
                app_debug_encoder_stream(50);
                need_refresh = true;
            } else if (strcmp(line_buf, "diag") == 0) {
                app_debug_encoder_diag(ctx, selected_motor);
                need_refresh = true;
            } else if (strcmp(line_buf, "adc") == 0) {
                app_debug_adc_test();
                need_refresh = true;
            } else if (strncmp(line_buf, "zutptest", 8) == 0) {
                /* zutptest N: 启动 N 秒 ZUPT 测试 */
                uint32_t dur = 60;
                if (strlen(line_buf) > 9) {
                    dur = (uint32_t)atoi(&line_buf[9]);
                    if (dur == 0 || dur > 86400) dur = 60;
                }
                app_test_runner_start(dur);
            } else if (strncmp(line_buf, "turndtest", 9) == 0) {
                /* turndtest ANGLE [TIMEOUT]: 动态转动精度测试 */
                float target = 90.0f;
                uint32_t timeout = 60;
                /* 解析: "turndtest 90" 或 "turndtest 90 30" */
                char *p = &line_buf[9];
                while (*p == ' ') p++;
                if (*p != '\0') {
                    target = (float)atof(p);
                    /* 查找第二个参数 */
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
                /* kftune N: KF 参数扫描, 每组 N 秒 (默认 10) */
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

                    /* Run命令进入数据输出模式 */
                    if (cmd.type == VOFA_CMD_RUN ||
                        cmd.type == VOFA_CMD_STREAM_ON) {
                        menu_data_output_loop(ctx, &selected_motor,
                                              &need_refresh);
                        /* 退出后刷新菜单 */
                        need_refresh = true;
                    }
                }
            }
        }

        /* LED心跳 */
        led_cnt += MENU_LED_PERIOD_MS;
        if (led_cnt >= LED_TOGGLE_THRESH) {
            led_cnt = 0U;
            bsp_led_toggle();
        }

        osal_task_delay_ms(MENU_LED_PERIOD_MS);
    }
}
