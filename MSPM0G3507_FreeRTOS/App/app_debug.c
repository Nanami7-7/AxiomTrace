#include "app_debug.h"
#include "app_main.h"
#include "bsp_encoder.h"
#include "bsp_adc.h"
#include "bsp_uart.h"
#include "bsp_timer.h"
#include "osal_api.h"
#include <stdio.h>
#include <string.h>

void app_debug_encoder_stream(uint32_t period_ms)
{
    char line_buf[16];
    uint32_t line_pos = 0;

    printf("\r\n===== Encoder Stream (DMA) =====\r\n");
    printf("STOP to exit.\r\n");

    for (;;) {
        int32_t totals[BSP_ENCODER_COUNT];
        bsp_encoder_get_all_totals(totals);

        char buf[48];
        int len = snprintf(buf, sizeof(buf), "%ld,%ld,%ld,%ld\n",
            (long)totals[BSP_ENCODER_LF],
            (long)totals[BSP_ENCODER_LB],
            (long)totals[BSP_ENCODER_RF],
            (long)totals[BSP_ENCODER_RB]);

        if (bsp_uart_tx_idle()) {
            bsp_uart_send_dma((uint8_t *)buf, (uint16_t)len);
        }

        uint32_t elapsed = 0;
        while (elapsed < period_ms) {
            uint8_t ch;
            if (bsp_uart_getc(&ch) == BSP_OK) {
                if (ch == '\r' || ch == '\n') {
                    line_buf[line_pos] = '\0';
                    if (strcmp(line_buf, "STOP") == 0) {
                        printf("\r\n===== Stream Stopped =====\r\n");
                        return;
                    }
                    line_pos = 0;
                } else if (line_pos < sizeof(line_buf) - 1) {
                    line_buf[line_pos++] = (char)ch;
                }
            }
            bsp_delay_ms(10);
            elapsed += 10;
        }
    }
}

static bool diag_check_stop(char *buf, uint32_t *pos)
{
    uint8_t ch;
    while (bsp_uart_getc(&ch) == BSP_OK) {
        if (ch == '\r' || ch == '\n') {
            buf[*pos] = '\0';
            bool match = (strcmp(buf, "STOP") == 0);
            *pos = 0;
            if (match) return true;
        } else if (*pos < 15) {
            buf[*pos++] = (char)ch;
        }
    }
    return false;
}

void app_debug_encoder_diag(struct app_shared_ctx_s *ctx,
                             uint32_t motor_id)
{
    if (ctx == NULL || motor_id >= BSP_MOTOR_COUNT) {
        return;
    }

    bsp_encoder_id_t enc_id = (bsp_encoder_id_t)motor_id;
    uint32_t ppr = bsp_encoder_get_pulses_per_rev();
    const char *mname[] = {"A","B","C","D"};

    printf("\r\n===== ENCODER DIAGNOSTIC: Motor %s =====\r\n",
        mname[motor_id]);
    printf("PPR=%lu  Control period=%u ms\r\n",
        (unsigned long)ppr, (unsigned)APP_CONTROL_PERIOD_MS);

    float target;
    OSAL_CRITICAL_SECTION {
        target = ctx->pid[motor_id].setpoint;
    }
    printf("Target=%.0f RPM\r\n", (double)target);

    OSAL_CRITICAL_SECTION {
        app_pid_reset(&ctx->pid[motor_id]);
        ctx->motor_enabled[motor_id] = true;
    }
    printf("Motor enabled. Sampling for ~5s (STOP to exit early)...\r\n");
    printf("cnt|tot|edge|sign|first|last|period|dir|ovf|ctrl|calc|out|pid_corr|verdict\r\n");

    char stop_buf[16];
    uint32_t stop_pos = 0;
    bool aborted = false;

    for (uint32_t iter = 0; iter < 50; iter++) {
        if (diag_check_stop(stop_buf, &stop_pos)) {
            aborted = true;
            break;
        }

        bsp_encoder_diag_t d;
        bsp_status_t ret = bsp_encoder_get_diag(enc_id, &d);
        if (ret != BSP_OK) {
            printf("bsp_encoder_get_diag failed (%d)\r\n", (int)ret);
            break;
        }

        int32_t ctrl_rpm, ctrl_out;
        float pid_corr;
        OSAL_CRITICAL_SECTION {
            ctrl_rpm  = ctx->status.rpm[motor_id];
            ctrl_out  = ctx->status.output[motor_id];
            pid_corr  = ctx->status.pid_correction[motor_id];
        }

        int32_t M = d.count;
        int32_t calc_rpm = 0;
        const char *verdict = "";

        if (M >= 2 || M <= -2) {
            int16_t diff = (int16_t)(d.last_cnt - d.first_cnt);
            uint32_t t_ref = (diff > 0) ? (uint32_t)diff
                                        : (uint32_t)(diff + 65536);
            if (t_ref == 0) t_ref = 1;
            int64_t num = (int64_t)M * 6000000LL;
            int64_t den = (int64_t)t_ref * (int64_t)ppr;
            calc_rpm = (den != 0) ? (int32_t)(num / den) : 0;
            if (calc_rpm == 0) {
                verdict = "INT_TRUNC(M/T)";
            }

        } else if (M == 1 || M == -1) {
            if (d.period == 0) {
                /* CC0首沿period未更新, 使用time_since估算(同P0修复) */
                uint32_t ts = (d.time_since != 0) ? d.time_since : 1;
                int64_t num = (int64_t)M * 6000000LL;
                int64_t den = (int64_t)ts * (int64_t)ppr;
                calc_rpm = (den != 0) ? (int32_t)(num / den) : 0;
                verdict = (calc_rpm != 0) ? "T:period=0(ts-fallback)"
                                          : "T:period=0(ts-trunc)";
            } else {
                int64_t num = (int64_t)M * 6000000LL;
                int64_t den = (int64_t)d.period * (int64_t)ppr;
                calc_rpm = (den != 0) ? (int32_t)(num / den) : 0;
                if (calc_rpm == 0) {
                    verdict = "INT_TRUNC(T)";
                }
            }

        } else {
            if (d.last_dir == 0) {
                calc_rpm = 0;
                verdict = "NEVER_CAPTURED(last_dir=0)";
            } else {
                calc_rpm = 0;
                verdict = "M=0(decay)";
            }
        }

        if (ctrl_rpm == 0 && calc_rpm > 0) {
            verdict = "STALL_DETECT";
        } else if (M == 0 && d.total == 0 && d.last_dir == 0) {
            verdict = "ISR_NEVER(count=0,total=0)";
        } else if (M == 0 && d.total != 0 && d.last_dir != 0) {
            verdict = "WINDOW_GAP(total>0 but no edges this win)";
        } else if (M != 0 && calc_rpm == 0) {
            verdict = "INT_TRUNC";
        } else if (calc_rpm > 0 && ctrl_rpm != 0) {
            verdict = "OK";
        }

        printf("%+4ld|%+5ld|%d|%+2d|%5u|%5u|%5u|%+3ld|%4lu|%4ld|%5ld|%4ld|%6.1f|%s\r\n",
            (long)d.count, (long)d.total, (int)d.has_edge, (int)d.sign,
            (unsigned)d.first_cnt, (unsigned)d.last_cnt,
            (unsigned)d.period, (long)d.last_dir,
            (unsigned long)d.overflow,
            (long)ctrl_rpm, (long)calc_rpm, (long)ctrl_out,
            (double)pid_corr, verdict);

        osal_task_delay_ms(100);
    }

    app_motor_stop(ctx, motor_id);
    if (aborted) {
        printf("Motor stopped (aborted by user).\r\n");
    } else {
        printf("Motor stopped. Diagnostic complete.\r\n");
    }
}

/* ======================== ADC测试 ======================== */

void app_debug_adc_test(void)
{
    bsp_adc_init();
    /* 重复模式下只需触发一次,ADC自动连续转换 */
    bsp_adc_start_conversion(BSP_ADC_CH_VOLTAGE);

    printf("\r\n===== ADC Interrupt Test =====\r\n");
    printf("VREF=3300mV  RES=12bit(4096)  Pin=PA27  CH=0\r\n");
    printf("Type STOP to exit.\r\n");
    printf("  #   raw    mV  | min   max\r\n");

    uint16_t raw_min = 0xFFFF;
    uint16_t raw_max = 0;
    uint32_t raw_sum = 0;
    uint32_t sample_cnt = 0;

    char stop_buf[16];
    uint32_t stop_pos = 0;

    for (uint32_t i = 0; i < 100; i++) {
        uint8_t ch;
        while (bsp_uart_getc(&ch) == BSP_OK) {
            if (ch == '\r' || ch == '\n') {
                stop_buf[stop_pos] = '\0';
                if (strcmp(stop_buf, "STOP") == 0) {
                    printf("STOP detected.\r\n");
                    goto done;
                }
                stop_pos = 0;
            } else if (stop_pos < sizeof(stop_buf) - 1) {
                stop_buf[stop_pos++] = (char)ch;
            }
        }

        bsp_adc_clear_done_flag();

        uint32_t timeout = 800000;
        while (!bsp_adc_is_conversion_done()) {
            if (--timeout == 0) break;
        }

        if (timeout == 0) {
            printf("  %3lu  TIMEOUT\r\n", (unsigned long)i);
            continue;
        }

        uint16_t raw = bsp_adc_get_last_raw(BSP_ADC_CH_VOLTAGE);
        uint32_t mv = bsp_adc_get_last_voltage(BSP_ADC_CH_VOLTAGE);

        if (raw < raw_min) raw_min = raw;
        if (raw > raw_max) raw_max = raw;
        raw_sum += raw;
        sample_cnt++;

        printf("  %3lu  %4u  %4lumV | %4u %4u\r\n",
            (unsigned long)i, (unsigned)raw, (unsigned)mv,
            (unsigned)raw_min, (unsigned)raw_max);

        bsp_delay_ms(200);
    }

done:
    if (sample_cnt > 0) {
        uint32_t avg = raw_sum / sample_cnt;
        printf("\r\n%d samples. Avg=%u (%lumV)  Min=%u  Max=%u\r\n",
            (int)sample_cnt, (unsigned)avg,
            (unsigned)(avg * 3300U / 4096U),
            (unsigned)raw_min, (unsigned)raw_max);
    }
    printf("ADC test done.\r\n");
}
