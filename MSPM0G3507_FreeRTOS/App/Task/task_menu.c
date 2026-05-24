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
#include "bsp_motor.h"
#include "bsp_uart.h"
#include "app_debug.h"
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
                    cmd.type == VOFA_CMD_STOP_ALL) {
                    (void)printf("[DATA] VOFA+ output stopped.\r\n");
                    return;
                }
                /* 非Stop命令: 只打印反馈, 不刷新菜单 */
            }
        }

        /* 输出VOFA+数据(11通道, DMA非阻塞) */
        {
            float channels[11];
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
            for (uint32_t i = 0U; i < 11U; i++) {
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

    for (;;) {
        /* 刷新菜单 */
        if (need_refresh) {
            menu_print_status(ctx, selected_motor);
            need_refresh = false;
        }

        /* 检查命令 */
        if (menu_read_line(line_buf, MENU_LINE_BUF_SIZE, &line_pos)) {
            if (strcmp(line_buf, "enc") == 0) {
                app_debug_encoder_stream(50);
                need_refresh = true;
            } else if (strcmp(line_buf, "diag") == 0) {
                app_debug_encoder_diag(ctx, selected_motor);
                need_refresh = true;
            } else if (strcmp(line_buf, "adc") == 0) {
                app_debug_adc_test();
                need_refresh = true;
            } else {
                vofa_cmd_t cmd;
                if (app_vofa_parse_cmd(line_buf, &cmd)) {
                    app_vofa_apply_cmd(&cmd, ctx, &selected_motor,
                                       &need_refresh);

                    /* Run命令进入数据输出模式 */
                    if (cmd.type == VOFA_CMD_RUN) {
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
