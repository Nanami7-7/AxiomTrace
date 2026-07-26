/**
 * @file    app_imu_console.c
 * @brief   IMU 串口状态查询与受控遥测输出实现
 */

#include "app_imu_console.h"

#include "bsp_uart.h"
#include "osal_api.h"
#include "project_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_IMU_CONSOLE_TX_BUF_SIZE    (512U)
#define APP_IMU_CONSOLE_REPLY_GUARD_MS (100U)

typedef struct {
    bool streaming;
    bool sample_valid;
    app_imu_console_format_t format;
    uint32_t period_ms;
    uint32_t last_tx_ms;
    uint32_t reply_guard_start_ms;
    uint32_t tx_frame_count;
    uint32_t tx_busy_drop_count;
    app_imu_console_sample_t last_sample;
} app_imu_console_state_t;

static app_imu_console_state_t s_console;
static char s_tx_buf[APP_IMU_CONSOLE_TX_BUF_SIZE];

static uint32_t app_imu_console_now_ms(void)
{
    return osal_ticks_to_ms(osal_get_tick_count());
}

static const char *app_imu_console_skip_spaces(const char *text)
{
    while (text != NULL && *text == ' ') {
        text++;
    }
    return text;
}

static bool app_imu_console_is_command(const char *line)
{
    if (line == NULL || strncmp(line, "imu", 3U) != 0) {
        return false;
    }
    return line[3] == '\0' || line[3] == ' ';
}

static const char *app_imu_console_format_name(app_imu_console_format_t format)
{
    return (format == APP_IMU_CONSOLE_FORMAT_FULL) ? "full" : "compact";
}

static void app_imu_console_print_schema(app_imu_console_format_t format)
{
    if (format == APP_IMU_CONSOLE_FORMAT_FULL) {
        (void)printf("  schema: ax,ay,az,gx,gy,gz,pitch,roll,yaw,"
                     "kf_p00_x,kf_p00_y,kf_p00_z,gyro_mag,acc_norm_err,"
                     "kf_p11_x,temp,kf_bias_z\r\n");
    } else {
        (void)printf("  schema: roll,pitch,yaw (deg)\r\n");
    }
}

static void app_imu_console_print_help(void)
{
    (void)printf("\r\nIMU console commands:\r\n");
    (void)printf("  imu | imu status          Print one readable IMU snapshot.\r\n");
    (void)printf("  imu stream start          Start controlled CSV telemetry.\r\n");
    (void)printf("  imu stream stop           Stop controlled CSV telemetry.\r\n");
    (void)printf("  imu rate <ms>             Set period (%lu..%lu ms).\r\n",
        (unsigned long)PRJ_IMU_CONSOLE_MIN_PERIOD_MS,
        (unsigned long)PRJ_IMU_CONSOLE_MAX_PERIOD_MS);
    (void)printf("  imu format compact        CSV: roll,pitch,yaw.\r\n");
    (void)printf("  imu format full           CSV: 17-channel diagnostic frame.\r\n");
    (void)printf("  imu help                  Show this help.\r\n");
    (void)printf("Non-IMU commands stop the IMU stream before execution.\r\n");
}

static void app_imu_console_print_status(void)
{
    app_imu_console_state_t snapshot;
    uint32_t now_ms = app_imu_console_now_ms();

    OSAL_CRITICAL_SECTION {
        snapshot = s_console;
    }

    (void)printf("\r\nIMU console status:\r\n");
    (void)printf("  stream : %s\r\n", snapshot.streaming ? "ON" : "OFF");
    (void)printf("  format : %s\r\n",
        app_imu_console_format_name(snapshot.format));
    (void)printf("  period : %lu ms\r\n", (unsigned long)snapshot.period_ms);
    (void)printf("  tx     : frames=%lu busy-drops=%lu\r\n",
        (unsigned long)snapshot.tx_frame_count,
        (unsigned long)snapshot.tx_busy_drop_count);

    if (!snapshot.sample_valid) {
        (void)printf("  sample : unavailable (IMU not initialized or no valid update)\r\n");
        return;
    }

    (void)printf("  sample : age=%lu ms timestamp=%lu ms\r\n",
        (unsigned long)(now_ms - snapshot.last_sample.timestamp_ms),
        (unsigned long)snapshot.last_sample.timestamp_ms);
    (void)printf("  attitude(deg): R=%.3f P=%.3f Y=%.3f\r\n",
        (double)snapshot.last_sample.roll,
        (double)snapshot.last_sample.pitch,
        (double)snapshot.last_sample.yaw);
    (void)printf("  accel(g)     : X=%.4f Y=%.4f Z=%.4f\r\n",
        (double)snapshot.last_sample.accel_x_g,
        (double)snapshot.last_sample.accel_y_g,
        (double)snapshot.last_sample.accel_z_g);
    (void)printf("  gyro(dps)    : X=%.3f Y=%.3f Z=%.3f\r\n",
        (double)snapshot.last_sample.gyro_x_dps,
        (double)snapshot.last_sample.gyro_y_dps,
        (double)snapshot.last_sample.gyro_z_dps);
    (void)printf("  temp         : %.1f C\r\n",
        (double)snapshot.last_sample.temperature);
}

static bool app_imu_console_parse_period(const char *text, uint32_t *period_ms)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL || period_ms == NULL) {
        return false;
    }

    text = app_imu_console_skip_spaces(text);
    if (*text == '\0') {
        return false;
    }

    value = strtoul(text, &end, 10);
    if (end == text) {
        return false;
    }
    end = (char *)app_imu_console_skip_spaces(end);
    if (*end != '\0' ||
        value < (unsigned long)PRJ_IMU_CONSOLE_MIN_PERIOD_MS ||
        value > (unsigned long)PRJ_IMU_CONSOLE_MAX_PERIOD_MS) {
        return false;
    }

    *period_ms = (uint32_t)value;
    return true;
}

void app_imu_console_init(void)
{
    OSAL_CRITICAL_SECTION {
        memset(&s_console, 0, sizeof(s_console));
        s_console.format = APP_IMU_CONSOLE_FORMAT_COMPACT;
        s_console.period_ms = PRJ_IMU_CONSOLE_DEFAULT_PERIOD_MS;
    }
}

void app_imu_console_update(const app_imu_console_sample_t *sample)
{
#if (PRJ_IMU_CONSOLE_ENABLE != 0U)
    bool streaming;
    app_imu_console_format_t format;
    uint32_t period_ms;
    uint32_t last_tx_ms;
    uint32_t reply_guard_start_ms;
    int len;
    bsp_status_t send_status;

    if (sample == NULL) {
        return;
    }

    OSAL_CRITICAL_SECTION {
        s_console.last_sample = *sample;
        s_console.sample_valid = true;
        streaming = s_console.streaming;
        format = s_console.format;
        period_ms = s_console.period_ms;
        last_tx_ms = s_console.last_tx_ms;
        reply_guard_start_ms = s_console.reply_guard_start_ms;
    }

    if (!streaming ||
        (uint32_t)(sample->timestamp_ms - reply_guard_start_ms) <
            APP_IMU_CONSOLE_REPLY_GUARD_MS ||
        (uint32_t)(sample->timestamp_ms - last_tx_ms) < period_ms) {
        return;
    }

    if (format == APP_IMU_CONSOLE_FORMAT_FULL) {
        len = snprintf(s_tx_buf, sizeof(s_tx_buf),
            "%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
            "%.6f,%.6f,%.6f,%.3f,%.4f,%.6f,%.1f,%.4f\r\n",
            (double)sample->accel_x_g,
            (double)sample->accel_y_g,
            (double)sample->accel_z_g,
            (double)sample->gyro_x_dps,
            (double)sample->gyro_y_dps,
            (double)sample->gyro_z_dps,
            (double)sample->pitch,
            (double)sample->roll,
            (double)sample->yaw,
            (double)sample->kf_p00_x,
            (double)sample->kf_p00_y,
            (double)sample->kf_p00_z,
            (double)sample->gyro_mag_dps,
            (double)sample->acc_norm_err,
            (double)sample->kf_p11_x,
            (double)sample->temperature,
            (double)sample->kf_bias_z);
    } else {
        len = snprintf(s_tx_buf, sizeof(s_tx_buf), "%.3f,%.3f,%.3f\r\n",
            (double)sample->roll,
            (double)sample->pitch,
            (double)sample->yaw);
    }

    if (len <= 0 || (uint32_t)len >= sizeof(s_tx_buf)) {
        return;
    }

    send_status = bsp_uart_send_dma((const uint8_t *)s_tx_buf, (uint16_t)len);
    OSAL_CRITICAL_SECTION {
        if (send_status == BSP_OK && s_console.streaming) {
            s_console.last_tx_ms = sample->timestamp_ms;
            s_console.tx_frame_count++;
        } else if (send_status == BSP_ERR_BUSY) {
            s_console.tx_busy_drop_count++;
        }
    }
#else
    (void)sample;
#endif
}

app_imu_console_cmd_result_t app_imu_console_handle_command(const char *line)
{
    const char *args;
    uint32_t period_ms;

    if (!app_imu_console_is_command(line)) {
        return APP_IMU_CONSOLE_CMD_NOT_HANDLED;
    }

#if (PRJ_IMU_CONSOLE_ENABLE == 0U)
    (void)printf("IMU console is disabled by PRJ_IMU_CONSOLE_ENABLE.\r\n");
    return APP_IMU_CONSOLE_CMD_HANDLED;
#else
    args = app_imu_console_skip_spaces(line + 3);

    /* 暂停遥测片刻，保证本条文本回复不会被 CSV 帧插入。 */
    {
        uint32_t now_ms = app_imu_console_now_ms();
        OSAL_CRITICAL_SECTION {
            if (s_console.streaming) {
                s_console.last_tx_ms = now_ms;
                s_console.reply_guard_start_ms = now_ms;
            }
        }
    }

    if (*args == '\0' || strcmp(args, "status") == 0) {
        app_imu_console_print_status();
        return APP_IMU_CONSOLE_CMD_HANDLED;
    }

    if (strcmp(args, "help") == 0) {
        app_imu_console_print_help();
        return APP_IMU_CONSOLE_CMD_HANDLED;
    }

    if (strcmp(args, "stream start") == 0) {
        app_imu_console_format_t format;
        uint32_t configured_period;
        OSAL_CRITICAL_SECTION {
            s_console.streaming = true;
            s_console.last_tx_ms = app_imu_console_now_ms();
            s_console.reply_guard_start_ms = s_console.last_tx_ms;
            format = s_console.format;
            configured_period = s_console.period_ms;
        }
        (void)printf("IMU stream started: format=%s period=%lu ms.\r\n",
            app_imu_console_format_name(format),
            (unsigned long)configured_period);
        app_imu_console_print_schema(format);
        (void)printf("Send 'imu stream stop' to exit.\r\n");
        return APP_IMU_CONSOLE_CMD_STREAM_STARTED;
    }

    if (strcmp(args, "stream stop") == 0) {
        bool was_streaming = app_imu_console_stop();
        (void)printf("IMU stream %s.\r\n",
            was_streaming ? "stopped" : "was already stopped");
        return APP_IMU_CONSOLE_CMD_STREAM_STOPPED;
    }

    if (strncmp(args, "rate", 4U) == 0 &&
        (args[4] == '\0' || args[4] == ' ')) {
        if (!app_imu_console_parse_period(args + 4, &period_ms)) {
            (void)printf("Invalid IMU period. Use: imu rate <%lu..%lu ms>\r\n",
                (unsigned long)PRJ_IMU_CONSOLE_MIN_PERIOD_MS,
                (unsigned long)PRJ_IMU_CONSOLE_MAX_PERIOD_MS);
            return APP_IMU_CONSOLE_CMD_HANDLED;
        }
        OSAL_CRITICAL_SECTION {
            s_console.period_ms = period_ms;
            s_console.last_tx_ms = app_imu_console_now_ms();
        }
        (void)printf("IMU stream period set to %lu ms.\r\n",
            (unsigned long)period_ms);
        return APP_IMU_CONSOLE_CMD_HANDLED;
    }

    if (strcmp(args, "format compact") == 0) {
        OSAL_CRITICAL_SECTION {
            s_console.format = APP_IMU_CONSOLE_FORMAT_COMPACT;
        }
        (void)printf("IMU stream format set to compact.\r\n");
        app_imu_console_print_schema(APP_IMU_CONSOLE_FORMAT_COMPACT);
        return APP_IMU_CONSOLE_CMD_HANDLED;
    }

    if (strcmp(args, "format full") == 0) {
        OSAL_CRITICAL_SECTION {
            s_console.format = APP_IMU_CONSOLE_FORMAT_FULL;
        }
        (void)printf("IMU stream format set to full.\r\n");
        app_imu_console_print_schema(APP_IMU_CONSOLE_FORMAT_FULL);
        return APP_IMU_CONSOLE_CMD_HANDLED;
    }

    (void)printf("Unknown IMU command.\r\n");
    app_imu_console_print_help();
    return APP_IMU_CONSOLE_CMD_HANDLED;
#endif
}

bool app_imu_console_is_streaming(void)
{
    bool streaming;
    OSAL_CRITICAL_SECTION {
        streaming = s_console.streaming;
    }
    return streaming;
}

bool app_imu_console_stop(void)
{
    bool was_streaming;
    OSAL_CRITICAL_SECTION {
        was_streaming = s_console.streaming;
        s_console.streaming = false;
    }
    return was_streaming;
}
