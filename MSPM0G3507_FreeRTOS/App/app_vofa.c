/**
 * @file    app_vofa.c
 * @brief   VOFA+ 通信协议实现
 * @note    FireWater: 文本格式, printf 输出
 *          格式: "val0,val1,val2,...,valN\n"
 *          JustFloat: 二进制格式, bsp_uart_putc 逐字节输出
 *          下行命令: 基于 sscanf/key-value 解析
 */
#include "app_vofa.h"
#include "app_pid.h"
#include "osal_api.h"
#include "bsp_uart.h"
#include "bsp_motor.h"
#include "axiomtrace.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ======================== 私有辅助函数 ======================== */

/**
 * @brief  忽略大小写的前缀比较
 * @param  str    完整字符串
 * @param  prefix 前缀(小写)
 * @retval true  str 以 prefix 开头(忽略大小写)
 */
static bool strnicmp_prefix(const char *str, const char *prefix)
{
    while (*prefix != '\0') {
        if (*str == '\0') {
            return false;
        }
        char c1 = *str;
        char c2 = *prefix;
        /* 简单小写转换 (仅ASCII字母) */
        if (c1 >= 'A' && c1 <= 'Z') { c1 += 32; }
        if (c2 >= 'A' && c2 <= 'Z') { c2 += 32; }
        if (c1 != c2) {
            return false;
        }
        str++;
        prefix++;
    }
    return true;
}

/**
 * @brief  发送单个字节(封装 bsp_uart_putc)
 */
static void vofa_putc(uint8_t ch)
{
    (void)bsp_uart_putc(ch);
}

/**
 * @brief  发送 float 的原始字节(小端序, JustFloat 用)
 */
static void vofa_send_float_bytes(float val)
{
    const uint8_t *p = (const uint8_t *)&val;
    for (uint32_t i = 0; i < sizeof(float); i++) {
        vofa_putc(p[i]);
    }
}

/* ======================== 公共函数实现 ======================== */

void app_vofa_send_firewater(const float channels[], uint32_t count)
{
    if (channels == NULL || count == 0U) {
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (i > 0U) {
            vofa_putc(',');
        }
        /* 格式: "val0,val1,...,valN\n" */
        (void)printf("%.6f", (double)channels[i]);
    }
    vofa_putc('\n');
}

void app_vofa_send_justfloat(const float channels[], uint32_t count)
{
    if (channels == NULL || count == 0U) {
        return;
    }

    /* 发送所有通道的原始浮点字节 */
    for (uint32_t i = 0; i < count; i++) {
        vofa_send_float_bytes(channels[i]);
    }

    /* 发送帧尾 0x00 0x00 0x80 0x7F */
    vofa_putc(VOFA_JUSTFLOAT_TAIL_0);
    vofa_putc(VOFA_JUSTFLOAT_TAIL_1);
    vofa_putc(VOFA_JUSTFLOAT_TAIL_2);
    vofa_putc(VOFA_JUSTFLOAT_TAIL_3);
}

bool app_vofa_parse_cmd(const char *line, vofa_cmd_t *cmd)
{
    if (line == NULL || cmd == NULL) {
        return false;
    }

    /* 跳过前导空白 */
    while (*line == ' ' || *line == '\t') {
        line++;
    }

    /* 跳过空行 */
    if (*line == '\0') {
        return false;
    }

    /* 清零命令结构 */
    memset(cmd, 0, sizeof(vofa_cmd_t));

    /* ---- 逐关键字匹配 ---- */

    /* "StopAll" (必须在 "Stop" 之前匹配) */
    if (strnicmp_prefix(line, "stopall")) {
        cmd->type = VOFA_CMD_STOP_ALL;
        return true;
    }

    /* "Stop" 或 "Stop=x" */
    if (strnicmp_prefix(line, "stop")) {
        cmd->type = VOFA_CMD_STOP;
        const char *eq = strchr(line, '=');
        if (eq != NULL) {
            cmd->motor_id = (uint32_t)atoi(eq + 1);
            cmd->has_motor = true;
        }
        return true;
    }

    /* "Run" 或 "Run=x" */
    if (strnicmp_prefix(line, "run")) {
        cmd->type = VOFA_CMD_RUN;
        const char *eq = strchr(line, '=');
        if (eq != NULL) {
            cmd->motor_id = (uint32_t)atoi(eq + 1);
            cmd->has_motor = true;
        }
        return true;
    }

    /* "Motor=x" */
    if (strnicmp_prefix(line, "motor=")) {
        cmd->type = VOFA_CMD_SET_MOTOR;
        cmd->motor_id = (uint32_t)atoi(line + 6);
        cmd->has_motor = true;
        return true;
    }

    /* "Target=x" */
    if (strnicmp_prefix(line, "target=")) {
        cmd->type = VOFA_CMD_SET_TARGET;
        cmd->value = (float)atof(line + 7);
        cmd->has_value = true;
        return true;
    }

    /* "Kp=x" */
    if (strnicmp_prefix(line, "kp=")) {
        cmd->type = VOFA_CMD_SET_KP;
        cmd->value = (float)atof(line + 3);
        cmd->has_value = true;
        return true;
    }

    /* "Ki=x" */
    if (strnicmp_prefix(line, "ki=")) {
        cmd->type = VOFA_CMD_SET_KI;
        cmd->value = (float)atof(line + 3);
        cmd->has_value = true;
        return true;
    }

    /* "Kd=x" */
    if (strnicmp_prefix(line, "kd=")) {
        cmd->type = VOFA_CMD_SET_KD;
        cmd->value = (float)atof(line + 3);
        cmd->has_value = true;
        return true;
    }

    return false;
}

void app_vofa_apply_cmd(const vofa_cmd_t *cmd,
                         app_shared_ctx_t *ctx,
                         uint32_t *current_motor)
{
    if (cmd == NULL || ctx == NULL || current_motor == NULL) {
        return;
    }

    uint32_t mid = *current_motor;

    /* 如果命令指定了电机ID, 更新当前电机 */
    if (cmd->has_motor && cmd->motor_id < BSP_MOTOR_COUNT) {
        mid = cmd->motor_id;
        *current_motor = mid;
    }

    switch (cmd->type) {
    case VOFA_CMD_SET_KP:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] Kp rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                float val = cmd->value;
                if (val > VOFA_PID_PARAM_MAX) { val = VOFA_PID_PARAM_MAX; }
                if (val < -VOFA_PID_PARAM_MAX) { val = -VOFA_PID_PARAM_MAX; }
                float old_val;
                OSAL_CRITICAL_SECTION {
                    old_val = ctx->pid[mid].kp;
                    ctx->pid[mid].kp = val;
                }
                (void)printf("[%lu] Kp %.2f -> %.2f\r\n",
                    (unsigned long)mid,
                    (double)old_val, (double)val);
            }
        }
        break;

    case VOFA_CMD_SET_KI:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] Ki rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                float val = cmd->value;
                if (val > VOFA_PID_PARAM_MAX) { val = VOFA_PID_PARAM_MAX; }
                if (val < -VOFA_PID_PARAM_MAX) { val = -VOFA_PID_PARAM_MAX; }
                float old_val;
                OSAL_CRITICAL_SECTION {
                    old_val = ctx->pid[mid].ki;
                    ctx->pid[mid].ki = val;
                }
                (void)printf("[%lu] Ki %.2f -> %.2f\r\n",
                    (unsigned long)mid,
                    (double)old_val, (double)val);
            }
        }
        break;

    case VOFA_CMD_SET_KD:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] Kd rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                float val = cmd->value;
                if (val > VOFA_PID_PARAM_MAX) { val = VOFA_PID_PARAM_MAX; }
                if (val < -VOFA_PID_PARAM_MAX) { val = -VOFA_PID_PARAM_MAX; }
                float old_val;
                OSAL_CRITICAL_SECTION {
                    old_val = ctx->pid[mid].kd;
                    ctx->pid[mid].kd = val;
                }
                (void)printf("[%lu] Kd %.2f -> %.2f\r\n",
                    (unsigned long)mid,
                    (double)old_val, (double)val);
            }
        }
        break;

    case VOFA_CMD_SET_TARGET:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] Target rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                float val = cmd->value;
                if (val > VOFA_TARGET_RPM_MAX) { val = VOFA_TARGET_RPM_MAX; }
                if (val < -VOFA_TARGET_RPM_MAX) { val = -VOFA_TARGET_RPM_MAX; }
                float old_sp;
                OSAL_CRITICAL_SECTION {
                    old_sp = ctx->pid[mid].setpoint;
                    app_pid_set_setpoint(&ctx->pid[mid], val);
                }
                (void)printf("[%lu] Target %.0f -> %.0f RPM\r\n",
                    (unsigned long)mid,
                    (double)old_sp, (double)val);
            }
        }
        break;

    case VOFA_CMD_SET_MOTOR:
        (void)printf("Motor %lu selected\r\n",
            (unsigned long)mid);
        break;

    case VOFA_CMD_RUN:
        OSAL_CRITICAL_SECTION {
            app_pid_set_setpoint(&ctx->pid[mid],
                ctx->pid[mid].setpoint);
            ctx->motor_enabled[mid] = true;
        }
        (void)printf("[%lu] Run (target=%.0f RPM)\r\n",
            (unsigned long)mid,
            (double)ctx->pid[mid].setpoint);
        break;

    case VOFA_CMD_STOP:
        OSAL_CRITICAL_SECTION {
            ctx->motor_enabled[mid] = false;
            app_pid_set_setpoint(&ctx->pid[mid], 0.0f);
            app_pid_reset(&ctx->pid[mid]);
        }
        (void)bsp_motor_stop((bsp_motor_id_t)mid,
            BSP_MOTOR_MODE_BRAKE);
        (void)printf("[%lu] Stop\r\n", (unsigned long)mid);
        break;

    case VOFA_CMD_STOP_ALL:
        OSAL_CRITICAL_SECTION {
            for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
                ctx->motor_enabled[i] = false;
                app_pid_set_setpoint(&ctx->pid[i], 0.0f);
                app_pid_reset(&ctx->pid[i]);
            }
        }
        for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
            (void)bsp_motor_stop((bsp_motor_id_t)i,
                BSP_MOTOR_MODE_BRAKE);
        }
        (void)printf("All stopped\r\n");
        break;

    default:
        break;
    }
}