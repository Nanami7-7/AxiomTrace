/**
 * @file    app_encoder_telemetry.c
 * @brief   编码器累计计数 UART0 DMA 遥测实现。
 */
#include "app_encoder_telemetry.h"
#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "bsp_uart.h"
#include "project_config.h"
#include <stdio.h>

#ifndef PRJ_ENCODER_TELEMETRY_PERIOD_MS
#define PRJ_ENCODER_TELEMETRY_PERIOD_MS (200U)
#endif

/**
 * 电机接口 A/B/C/D 到 BSP 编码器数组(LF/LB/RF/RB)的映射。
 *
 * PRJ_MOTOR_ENCODER_MAP 是当前工程唯一的硬件映射来源：
 *   A/M1 -> RB，B/M2 -> RF，C/M3 -> LF，D/M4 -> LB。
 * 遥测必须按电机接口顺序输出，不能直接按 BSP 的车轮枚举顺序输出。
 */
static const bsp_encoder_id_t s_motor_encoder_map[BSP_MOTOR_COUNT] =
    PRJ_MOTOR_ENCODER_MAP;

/** 遥测开关；只由任务/按键回调访问，发送本身在控制任务中完成。 */
static volatile bool s_telemetry_active = false;
/** 下一次允许提交 DMA 的时间。 */
static uint32_t s_next_send_ms = 0U;

void app_encoder_telemetry_start(void)
{
    s_telemetry_active = true;
    s_next_send_ms = 0U;
}

void app_encoder_telemetry_stop(void)
{
    s_telemetry_active = false;
}

bool app_encoder_telemetry_is_active(void)
{
    return s_telemetry_active;
}

void app_encoder_telemetry_process(uint32_t now_ms)
{
    int32_t totals[BSP_ENCODER_COUNT];
    char tx_buf[96];
    int len;

    if (!s_telemetry_active) {
        return;
    }

    /* 允许首次启动立即发送；后续按配置周期发送。 */
    if (s_next_send_ms != 0U &&
        (int32_t)(now_ms - s_next_send_ms) < 0) {
        return;
    }

    if (bsp_encoder_get_all_totals(totals) != BSP_OK) {
        return;
    }

    len = snprintf(tx_buf, sizeof(tx_buf),
                   "ENC_TOTAL,A=%ld,B=%ld,C=%ld,D=%ld\r\n",
                   (long)totals[s_motor_encoder_map[0U]],
                   (long)totals[s_motor_encoder_map[1U]],
                   (long)totals[s_motor_encoder_map[2U]],
                   (long)totals[s_motor_encoder_map[3U]]);
    if (len <= 0 || (uint32_t)len >= sizeof(tx_buf)) {
        return;
    }

    /* DMA 忙时不推进时间点，下一个控制周期继续重试，避免停止遥测。 */
    if (bsp_uart_send_dma((const uint8_t *)tx_buf, (uint16_t)len) == BSP_OK) {
        s_next_send_ms = now_ms + PRJ_ENCODER_TELEMETRY_PERIOD_MS;
    }
}