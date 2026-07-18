#include "app_debug.h"
#include "app_main.h"
#include "project_config.h"
#include "bsp_encoder.h"
#include "bsp_adc.h"
#include "bsp_uart.h"
#include "bsp_timer.h"
#include "bsp_motor.h"
#include "bsp_drv8870.h"
#include "app_model_id.h"
#include "osal_api.h"
#include "ti_msp_dl_config.h"
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
                                        : (uint32_t)(diff + PRJ_UINT16_MOD);
            if (t_ref == 0) t_ref = 1;
            int64_t num = (int64_t)M * PRJ_ENCODER_RPM_CALC_CONST;
            int64_t den = (int64_t)t_ref * (int64_t)ppr;
            calc_rpm = (den != 0) ? (int32_t)(num / den) : 0;
            if (calc_rpm == 0) {
                verdict = "INT_TRUNC(M/T)";
            }

        } else if (M == 1 || M == -1) {
            if (d.period == 0) {
                /* CC0首沿period未更新, 使用time_since估算(同P0修复) */
                uint32_t ts = (d.time_since != 0) ? d.time_since : 1;
                int64_t num = (int64_t)M * PRJ_ENCODER_RPM_CALC_CONST;
                int64_t den = (int64_t)ts * (int64_t)ppr;
                calc_rpm = (den != 0) ? (int32_t)(num / den) : 0;
                verdict = (calc_rpm != 0) ? "T:period=0(ts-fallback)"
                                          : "T:period=0(ts-trunc)";
            } else {
                int64_t num = (int64_t)M * PRJ_ENCODER_RPM_CALC_CONST;
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

/* ======================== DRV8870 persistent scope session ======================== */

#if (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0U)
#define APP_DRVSCOPE_DUTY_SCALE  (1000U) /* 0.1% resolution */

static const char *const s_drvscope_motor_names[BSP_MOTOR_COUNT] = {
    "A/M1", "B/M2", "C/M3", "D/M4"
};
static const char *const s_drvscope_pwm_names[BSP_MOTOR_COUNT] = {
    "PA8/TIMA0 CC0", "PA9/TIMA0 CC1", "PB17/TIMA0 CC2", "PB2/TIMA0 CC3"
};

static uint32_t app_drvscope_permille_to_compare(uint32_t duty_permille)
{
    uint64_t scaled = (uint64_t)duty_permille *
                      (uint64_t)PRJ_DRV8870_PWM_PERIOD;
    return (uint32_t)((scaled + (APP_DRVSCOPE_DUTY_SCALE / 2U)) /
                      APP_DRVSCOPE_DUTY_SCALE);
}

static uint32_t app_drvscope_compare_to_permille(uint32_t compare)
{
    if (PRJ_DRV8870_PWM_PERIOD == 0U) {
        return 0U;
    }
    return (uint32_t)(((uint64_t)compare * APP_DRVSCOPE_DUTY_SCALE +
                       (PRJ_DRV8870_PWM_PERIOD / 2U)) /
                      PRJ_DRV8870_PWM_PERIOD);
}

#endif

void app_debug_drv8870_scope_status(void)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    (void)printf("DRV8870 scope commands are disabled in this production build.\r\n");
#else
    bool active = bsp_drv8870_hw_scope_is_active();
    bool power_on = bsp_motor_power_is_enabled();
    uint32_t pwm_frequency_hz = 0U;

    if (PRJ_DRV8870_PWM_PERIOD != 0U) {
        pwm_frequency_hz = (uint32_t)(PRJ_DRV8870_PWM_CLK_HZ /
                                      PRJ_DRV8870_PWM_PERIOD);
    }

    (void)printf("\r\n===== DRV8870 SCOPE STATUS =====\r\n");
    (void)printf("Session=%s; PB19 command=%s; PWM clock=%lu Hz; period=%lu; frequency=%lu Hz.\r\n",
                 active ? "ACTIVE" : "INACTIVE",
                 power_on ? "ON" : "OFF",
                 (unsigned long)PRJ_DRV8870_PWM_CLK_HZ,
                 (unsigned long)PRJ_DRV8870_PWM_PERIOD,
                 (unsigned long)pwm_frequency_hz);
    (void)printf("All four channels share TIMA0, so their PWM frequency is identical.\r\n");
    for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
        uint32_t compare = 0U;
        bsp_status_t status = bsp_drv8870_hw_scope_get_compare(
            (bsp_drv8870_id_t)i, &compare);
        uint32_t duty_permille = app_drvscope_compare_to_permille(compare);

        if (status == BSP_OK) {
            (void)printf("%s %-14s freq=%5lu Hz compare=%4lu duty=%3lu.%lu%%\r\n",
                         s_drvscope_motor_names[i], s_drvscope_pwm_names[i],
                         (unsigned long)pwm_frequency_hz,
                         (unsigned long)compare,
                         (unsigned long)(duty_permille / 10U),
                         (unsigned long)(duty_permille % 10U));
        } else {
            (void)printf("%s %-14s compare read failed: status=%d\r\n",
                         s_drvscope_motor_names[i], s_drvscope_pwm_names[i],
                         (int)status);
        }
    }
    if (active) {
        (void)printf("PB19 stays ON until 'drvscope off' or MCU reset.\r\n");
    }
#endif
}

void app_debug_drv8870_scope_start(struct app_shared_ctx_s *ctx)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    (void)ctx;
    (void)printf("DRV8870 scope start is disabled in this production build. PB19 was not changed.\r\n");
#else
    bsp_status_t status;

    if (ctx == NULL) {
        (void)printf("DRV8870 scope start rejected: invalid application context.\r\n");
        return;
    }
    if (bsp_drv8870_hw_scope_is_active()) {
        (void)printf("DRV8870 scope session is already active; existing duties were preserved.\r\n");
        app_debug_drv8870_scope_status();
        return;
    }

    /* Stop every normal producer before the BSP scope lock is acquired. */
    app_id_abort();
    app_motor_stop_all(ctx);
    osal_task_delay_ms(20U);

    status = bsp_drv8870_hw_scope_start();
    if (status != BSP_OK) {
        (void)printf("DRV8870 scope start failed: status=%d. PB19 was forced OFF.\r\n",
                     (int)status);
        return;
    }

    (void)printf("\r\n===== DRV8870 SCOPE SESSION STARTED =====\r\n");
    (void)printf("All four PWM channels were initialized to 50.0%%, then PB19 was enabled.\r\n");
    (void)printf("There is NO automatic timeout. Use 'drvscope off' before reconnecting hardware.\r\n");
    (void)printf("Warning: 0.0%% and 100.0%% apply approximately full VM to a connected motor.\r\n");
    (void)printf("The board hardware cutoff removes 12V, but the PB19 software latch remains ON.\r\n");
    app_debug_drv8870_scope_status();
#endif
}

void app_debug_drv8870_scope_set(uint32_t motor_id,
                                  uint32_t duty_permille)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    (void)motor_id;
    (void)duty_permille;
    (void)printf("DRV8870 scope commands are disabled in this production build.\r\n");
#else
    bsp_status_t status;
    uint32_t compare;

    if ((motor_id >= BSP_MOTOR_COUNT) ||
        (duty_permille > APP_DRVSCOPE_DUTY_SCALE)) {
        (void)printf("DRV8870 scope set rejected: invalid motor or duty.\r\n");
        return;
    }
    compare = app_drvscope_permille_to_compare(duty_permille);
    status = bsp_drv8870_hw_scope_set_compare(
        (bsp_drv8870_id_t)motor_id, compare);
    if (status == BSP_ERR_NOT_READY) {
        (void)printf("DRV8870 scope is not active. Run 'drvscope start' first.\r\n");
        return;
    }
    if (status != BSP_OK) {
        (void)printf("DRV8870 scope set failed: status=%d. Hardware-fault exits force PB19 OFF.\r\n",
                     (int)status);
        return;
    }

    (void)printf("%s set: compare=%lu, duty=%lu.%lu%%; PB19 remains ON.\r\n",
                 s_drvscope_motor_names[motor_id],
                 (unsigned long)compare,
                 (unsigned long)(duty_permille / 10U),
                 (unsigned long)(duty_permille % 10U));
#endif
}

void app_debug_drv8870_scope_set_all(uint32_t duty_permille)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    (void)duty_permille;
    (void)printf("DRV8870 scope commands are disabled in this production build.\r\n");
#else
    bsp_status_t status;
    uint32_t compare;

    if (duty_permille > APP_DRVSCOPE_DUTY_SCALE) {
        (void)printf("DRV8870 scope all rejected: duty must be 0.0..100.0%%.\r\n");
        return;
    }
    compare = app_drvscope_permille_to_compare(duty_permille);
    status = bsp_drv8870_hw_scope_set_all_compare(compare);
    if (status == BSP_ERR_NOT_READY) {
        (void)printf("DRV8870 scope is not active. Run 'drvscope start' first.\r\n");
        return;
    }
    if (status != BSP_OK) {
        (void)printf("DRV8870 scope all failed: status=%d. Hardware-fault exits force PB19 OFF.\r\n",
                     (int)status);
        return;
    }

    (void)printf("All motors set: compare=%lu, duty=%lu.%lu%%; PB19 remains ON.\r\n",
                 (unsigned long)compare,
                 (unsigned long)(duty_permille / 10U),
                 (unsigned long)(duty_permille % 10U));
#endif
}

void app_debug_drv8870_scope_stop(void)
{
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE == 0U)
    (void)printf("DRV8870 scope off is disabled in this production build.\r\n");
#else
    bsp_status_t status = bsp_drv8870_hw_scope_stop();

    if (status == BSP_OK) {
        (void)printf("DRV8870 scope stopped: all PWM=neutral, PB19=OFF, ownership released.\r\n");
    } else {
        (void)printf("DRV8870 scope stop returned status=%d.\r\n", (int)status);
    }
#endif
}

/* ======================== Temporary HW mapping diagnostic ======================== */

/**
 * @brief Convert one-hot GPIO pin masks to a human-readable pin number.
 * @note  The generated DL_GPIO_PIN_x macros are one-hot masks. This helper is
 *        read-only and is used only by the temporary serial diagnostic.
 */
static uint32_t app_debug_gpio_pin_index(uint32_t pin_mask)
{
    for (uint32_t index = 0U; index < 32U; index++) {
        if (pin_mask == (1UL << index)) {
            return index;
        }
    }

    return 0xFFFFFFFFUL;
}

/**
 * @brief Print exactly one board-mapping and encoder snapshot on demand.
 * @note  This function deliberately does not call any motor, PID, encoder-clear,
 *        or task-creation API. It is intended for manual wheel rotation only.
 */
void app_debug_hwmap_snapshot(void)
{
    static bool s_has_previous_snapshot;
    static int32_t s_previous_totals[BSP_ENCODER_COUNT];

    static const char *const s_wheel_names[BSP_ENCODER_COUNT] = {
        "LF", "LB", "RF", "RB"
    };
    static const char *const s_timer_names[BSP_ENCODER_COUNT] = {
        "M3/TIMG7", "M4/TIMA1", "M2/TIMG6", "M1/TIMG0"
    };
    static const uint32_t s_capture_pins[BSP_ENCODER_COUNT] = {
        GPIO_M3_C0_PIN, GPIO_M4_C0_PIN, GPIO_M2_C0_PIN, GPIO_M1_C0_PIN
    };
    static const uint32_t s_b_phase_pins[BSP_ENCODER_COUNT] = {
        PRJ_ENCODER_LF_B_PIN, PRJ_ENCODER_LB_B_PIN,
        PRJ_ENCODER_RF_B_PIN, PRJ_ENCODER_RB_B_PIN
    };
    static const int8_t s_direction_signs[BSP_ENCODER_COUNT] = {
        PRJ_ENCODER_LF_DIR_SIGN, PRJ_ENCODER_LB_DIR_SIGN,
        PRJ_ENCODER_RF_DIR_SIGN, PRJ_ENCODER_RB_DIR_SIGN
    };

    int32_t totals[BSP_ENCODER_COUNT];
    bsp_uart_tx_diag_t uart_diag;
    bsp_status_t totals_status = bsp_encoder_get_all_totals(totals);
    bsp_status_t uart_status = bsp_uart_get_tx_diag(&uart_diag);
    uint32_t led_pin = app_debug_gpio_pin_index(PRJ_LED_PIN);

    (void)printf("\r\n===== TEMP READ-ONLY HW MAP DIAG =====\r\n");
    (void)printf("No PWM, no motor command, no encoder clear, no periodic task.\r\n");
    if (led_pin < 32U) {
        (void)printf("LED: PA%lu\r\n", (unsigned long)led_pin);
    } else {
        (void)printf("LED: invalid GPIO mask 0x%08lX\r\n",
                     (unsigned long)PRJ_LED_PIN);
    }

    if (totals_status != BSP_OK) {
        (void)printf("ENC snapshot unavailable: status=%d\r\n",
                     (int)totals_status);
    } else {
        (void)printf("wheel timer      A-pin B-pin cfg-sign total delta edge last-dir overflow\r\n");
        for (uint32_t i = 0U; i < BSP_ENCODER_COUNT; i++) {
            bsp_encoder_diag_t diag;
            bsp_status_t diag_status =
                bsp_encoder_get_diag((bsp_encoder_id_t)i, &diag);
            uint32_t capture_pin = app_debug_gpio_pin_index(s_capture_pins[i]);
            uint32_t b_phase_pin = app_debug_gpio_pin_index(s_b_phase_pins[i]);

            if (diag_status != BSP_OK) {
                (void)printf("%s %s diag unavailable: status=%d\r\n",
                             s_wheel_names[i], s_timer_names[i],
                             (int)diag_status);
                continue;
            }

            if (s_has_previous_snapshot) {
                (void)printf("%-5s %-10s PA%-2lu PA%-2lu %8d %5ld %5ld %4u %8ld %8lu\r\n",
                             s_wheel_names[i], s_timer_names[i],
                             (unsigned long)capture_pin,
                             (unsigned long)b_phase_pin,
                             (int)s_direction_signs[i],
                             (long)totals[i],
                             (long)(totals[i] - s_previous_totals[i]),
                             diag.has_edge ? 1U : 0U,
                             (long)diag.last_dir,
                             (unsigned long)diag.overflow);
            } else {
                (void)printf("%-5s %-10s PA%-2lu PA%-2lu %8d %5ld %5s %4u %8ld %8lu\r\n",
                             s_wheel_names[i], s_timer_names[i],
                             (unsigned long)capture_pin,
                             (unsigned long)b_phase_pin,
                             (int)s_direction_signs[i],
                             (long)totals[i], "N/A",
                             diag.has_edge ? 1U : 0U,
                             (long)diag.last_dir,
                             (unsigned long)diag.overflow);
            }
        }

        for (uint32_t i = 0U; i < BSP_ENCODER_COUNT; i++) {
            s_previous_totals[i] = totals[i];
        }
        s_has_previous_snapshot = true;
    }

    if (uart_status == BSP_OK) {
        (void)printf("UART-DMA(pre-print): start=%lu done=%lu eot=%lu busy=%lu max=%u\r\n",
                     (unsigned long)uart_diag.dma_start_count,
                     (unsigned long)uart_diag.dma_done_count,
                     (unsigned long)uart_diag.eot_done_count,
                     (unsigned long)uart_diag.busy_reject_count,
                     (unsigned)uart_diag.max_submit_len);
    } else {
        (void)printf("UART-DMA snapshot unavailable: status=%d\r\n",
                     (int)uart_status);
    }

    (void)printf("Run encdiag once for a baseline; rotate one wheel by hand; run it again.\r\n");
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

        printf("  %3lu  %4u  %4umV | %4u %4u\r\n",
            (unsigned long)i, (unsigned)raw, (unsigned)mv,
            (unsigned)raw_min, (unsigned)raw_max);

        bsp_delay_ms(200);
    }

done:
    if (sample_cnt > 0) {
        uint32_t avg = raw_sum / sample_cnt;
        printf("\r\n%d samples. Avg=%u (%umV)  Min=%u  Max=%u\r\n",
            (int)sample_cnt, (unsigned)avg,
            (unsigned)(avg * PRJ_ADC_VREF_MV / PRJ_ADC_RESOLUTION),
            (unsigned)raw_min, (unsigned)raw_max);
    }
    printf("ADC test done.\r\n");
}
