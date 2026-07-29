/**
 * @file    app_vofa.c
 * @brief   VOFA+ 通信协议实现
 * @note    FireWater: 文本格式, printf 输出
 *          格式: "val0,val1,val2,...,valN\n"
 * 说明：VOFA+通信相关处理。
 */
#include "app_vofa.h"
#include "app_pid.h"
#include "app_feedforward.h"
#include "app_model_id.h"
#include "app_position_control.h"
#include "osal_api.h"
#include "bsp_uart.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "axiomtrace.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ======================== 私有类型 ======================== */

typedef enum {
    PID_PARAM_KP = 0,
    PID_PARAM_KI,
    PID_PARAM_KD
} pid_param_idx_t;/* 电机 A/B/C/D 到物理编码器 RB/RF/LF/LB 的统一映射。 */
static const bsp_encoder_id_t s_vofa_motor_encoder_map[BSP_MOTOR_COUNT] =
    PRJ_MOTOR_ENCODER_MAP;

/* ======================== 私有辅助函数 ======================== */

/**
 * @brief  忽略大小写的前缀比较
 * 说明：VOFA+通信相关处理。
 * 说明：VOFA+通信相关处理。
 */
static bool strnicmp_prefix(const char *str, const char *prefix)
{
    while (*prefix != '\0') {
        if (*str == '\0') {
            return false;
        }
        char c1 = *str;
        char c2 = *prefix;
        /* 说明：VOFA+通信相关处理。 */
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
 * 说明：VOFA+通信相关处理。
 * @param  numstr  数字符串
 * 说明：VOFA+通信相关处理。
 */
static bool stricmp_equal(const char *lhs, const char *rhs)
{
    if (lhs == NULL || rhs == NULL) { return false; }
    while (*lhs != '\0' && *rhs != '\0') {
        char c1 = *lhs++;
        char c2 = *rhs++;
        if (c1 >= 'A' && c1 <= 'Z') { c1 += 32; }
        if (c2 >= 'A' && c2 <= 'Z') { c2 += 32; }
        if (c1 != c2) { return false; }
    }
    return (*lhs == '\0' && *rhs == '\0');
}

static const char *vofa_mode_name(app_ctrl_mode_t mode)
{
    switch (mode) {
    case APP_CTRL_MODE_SPEED: return "speed";
    case APP_CTRL_MODE_POSITION: return "position";
    case APP_CTRL_MODE_ANGLE: return "angle";
    default: return "unknown";
    }
}

static bool safe_atof(const char *numstr, float *out_val)
{
    if (numstr == NULL || *numstr == '\0') {
        return false;
    }
    char *endptr = NULL;
    float val = strtof(numstr, &endptr);
    if (endptr == numstr || *endptr != '\0') {
        return false;
    }
    if (!isfinite(val)) {
        return false;
    }
    *out_val = val;
    return true;
}

/**
 * @brief  应用单个PID参数更新
 */
static void vofa_apply_pid_param(app_pid_t *pid, float new_val,
                                  pid_param_idx_t which, uint32_t motor_id)
{
    static const char *names[] = { "Kp", "Ki", "Kd" };
    if (new_val >  PRJ_VOFA_PID_PARAM_MAX) { new_val =  PRJ_VOFA_PID_PARAM_MAX; }
    if (new_val < -PRJ_VOFA_PID_PARAM_MAX) { new_val = -PRJ_VOFA_PID_PARAM_MAX; }

    float old_val;
    OSAL_CRITICAL_SECTION {
        float kp = pid->kp, ki = pid->ki, kd = pid->kd;
        old_val = (which == PID_PARAM_KP) ? kp : (which == PID_PARAM_KI) ? ki : kd;
        if (which == PID_PARAM_KP) kp = new_val;
        if (which == PID_PARAM_KI) ki = new_val;
        if (which == PID_PARAM_KD) kd = new_val;
        app_pid_set_params(pid, kp, ki, kd);
    }
    (void)printf("[%lu] %s %.2f -> %.2f\r\n",
        (unsigned long)motor_id, names[which],
        (double)old_val, (double)new_val);
}

/**
 * 说明：VOFA+通信相关处理。
 */
static void vofa_putc(uint8_t ch)
{
    (void)bsp_uart_putc(ch);
}

/**
 * 说明：VOFA+通信相关处理。
 */
static void vofa_send_float_bytes(float val)
{
    const uint8_t *p = (const uint8_t *)&val;
    for (uint32_t i = 0; i < sizeof(float); i++) {
        vofa_putc(p[i]);
    }
}

/* 说明：VOFA+通信相关处理。 */

/**
 * 说明：VOFA+通信相关处理。
 * @retval true  检测到 "Stop" / "StopAll" / "Stop=x"
 * 说明：VOFA+通信相关处理。
 */
static bool vofa_detect_stop_cmd(void)
{
    uint8_t ch;
    static char s_buf[16];
    static uint32_t s_pos = 0;

    while (bsp_uart_getc(&ch) == BSP_OK) {
        if (ch == '\r' || ch == '\n') {
            s_buf[s_pos] = '\0';
            for (uint32_t i = 0; s_buf[i]; i++) {
                if (s_buf[i] >= 'A' && s_buf[i] <= 'Z') {
                    s_buf[i] += 32;
                }
            }
            s_pos = 0;
            if (strcmp(s_buf, "stop") == 0 ||
                strcmp(s_buf, "stopall") == 0 ||
                strncmp(s_buf, "stop=", 5) == 0) {
                return true;
            }
        } else if (ch == 0x7FU || ch == 0x08U) {
            if (s_pos > 0U) s_pos--;
        } else if (ch >= 0x20U && ch < 0x7FU) {
            if (s_pos < sizeof(s_buf) - 1U) {
                s_buf[s_pos++] = (char)ch;
            }
        }
    }
    return false;
}

/**
 * 说明：VOFA+通信相关处理。
 * 说明：VOFA+通信相关处理。
 * 说明：VOFA+通信相关处理。
 * @retval true   识别成功
 * 说明：VOFA+通信相关处理。
 */
static bool vofa_run_step_id(app_shared_ctx_t *ctx, uint32_t mid,
                               int32_t pwm, app_id_step_result_t *result)
{
    app_motor_stop(ctx, mid);
    osal_task_delay_ms(300);

    app_id_start_step(mid, pwm);

    uint32_t timeout = 500;
    while (timeout > 0U) {
        if (g_id_data.done) break;
        if (vofa_detect_stop_cmd() || g_sweep_cancel) {
            app_id_abort();
            bsp_motor_stop((bsp_motor_id_t)mid, BSP_MOTOR_MODE_BRAKE);
            return false;
        }
        osal_task_delay_ms(10);
        timeout--;
    }

    if (timeout == 0U && !g_id_data.done) {
        app_id_abort();
        bsp_motor_stop((bsp_motor_id_t)mid, BSP_MOTOR_MODE_BRAKE);
        return false;
    }

    float dt_s = (float)PRJ_CONTROL_PERIOD_MS / (float)PRJ_MS_PER_S;
    return app_id_process_step(g_id_data.rpm_buf, g_id_data.write_idx,
                                pwm, dt_s, result);
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

    /* 说明：VOFA+通信相关处理。 */
    for (uint32_t i = 0; i < count; i++) {
        vofa_send_float_bytes(channels[i]);
    }

    /* 说明：VOFA+通信相关处理。 */
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

    /* Stable v1 host integration commands. */
    if (stricmp_equal(line, "info?") || stricmp_equal(line, "info")) {
        cmd->type = VOFA_CMD_INFO_QUERY;
        return true;
    }
    if (stricmp_equal(line, "config?") || stricmp_equal(line, "config")) {
        cmd->type = VOFA_CMD_CONFIG_QUERY;
        return true;
    }
    if (stricmp_equal(line, "status?") || stricmp_equal(line, "status")) {
        cmd->type = VOFA_CMD_STATUS_QUERY;
        return true;
    }
    if (strnicmp_prefix(line, "status=")) {
        char *endptr = NULL;
        long val = strtol(line + 7, &endptr, 10);
        if (endptr == line + 7 || *endptr != '\0' ||
            val < 0 || val >= (long)BSP_MOTOR_COUNT) {
            return false;
        }
        cmd->type = VOFA_CMD_STATUS_QUERY;
        cmd->motor_id = (uint32_t)val;
        cmd->has_motor = true;
        return true;
    }
    if (stricmp_equal(line, "stream=1") || stricmp_equal(line, "stream=on")) {
        cmd->type = VOFA_CMD_STREAM_ON;
        return true;
    }
    if (stricmp_equal(line, "stream=0") || stricmp_equal(line, "stream=off")) {
        cmd->type = VOFA_CMD_STREAM_OFF;
        return true;
    }

    /* 说明：VOFA+通信相关处理。 */
    if (stricmp_equal(line, "step") || strnicmp_prefix(line, "step=")) {
        cmd->type = VOFA_CMD_STEP;
        const char *eq = strchr(line, '=');
        if (eq != NULL) {
            if (!safe_atof(eq + 1, &cmd->value)) {
                return false;
            }
            cmd->has_value = true;
        }
        return true;
    }

    /* 说明：VOFA+通信相关处理。 */
    if (stricmp_equal(line, "auto") || strnicmp_prefix(line, "auto=")) {
        cmd->type = VOFA_CMD_AUTOTUNE;
        const char *eq = strchr(line, '=');
        if (eq != NULL) {
            if (!safe_atof(eq + 1, &cmd->value)) {
                return false;
            }
            cmd->has_value = true;
        }
        return true;
    }

    /* ---- 辅助: 安全解析电机ID ---- */
    #define PARSE_MOTOR_ID(str, out_id) do {            \
        char *_endptr;                                   \
        long _val = strtol((str), &_endptr, 10);         \
        if (*_endptr != '\0' || _endptr == (str)) {      \
            return false; /* 说明：VOFA+通信相关处理。 */                \
        }                                                \
        if (_val < 0 || _val >= (long)BSP_MOTOR_COUNT) { \
            return false; /* 电机 ID 超出范围 */                  \
        }                                                \
        *(out_id) = (uint32_t)_val;                      \
    } while(0)

    /* ---- 逐关键字匹配 ---- */

    /* 说明：VOFA+通信相关处理。 */
    if (stricmp_equal(line, "stopall")) {
        cmd->type = VOFA_CMD_STOP_ALL;
        return true;
    }

    /* 说明：VOFA+通信相关处理。 */
    if (stricmp_equal(line, "stop") || strnicmp_prefix(line, "stop=")) {
        cmd->type = VOFA_CMD_STOP;
        const char *eq = strchr(line, '=');
        if (eq != NULL) {
            PARSE_MOTOR_ID(eq + 1, &cmd->motor_id);
            cmd->has_motor = true;
        }
        return true;
    }

    /* 说明：VOFA+通信相关处理。 */
    if (stricmp_equal(line, "run") || strnicmp_prefix(line, "run=")) {
        cmd->type = VOFA_CMD_RUN;
        const char *eq = strchr(line, '=');
        if (eq != NULL) {
            PARSE_MOTOR_ID(eq + 1, &cmd->motor_id);
            cmd->has_motor = true;
        }
        return true;
    }

    /* "Motor=x" */
    if (strnicmp_prefix(line, "motor=")) {
        cmd->type = VOFA_CMD_SET_MOTOR;
        PARSE_MOTOR_ID(line + 6, &cmd->motor_id);
        cmd->has_motor = true;
        return true;
    }

    /* "Target=x" */
    if (strnicmp_prefix(line, "target=")) {
        cmd->type = VOFA_CMD_SET_TARGET;
        if (safe_atof(line + 7, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "Kp=x" */
    if (strnicmp_prefix(line, "kp=")) {
        cmd->type = VOFA_CMD_SET_KP;
        if (safe_atof(line + 3, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "Ki=x" */
    if (strnicmp_prefix(line, "ki=")) {
        cmd->type = VOFA_CMD_SET_KI;
        if (safe_atof(line + 3, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "Kd=x" */
    if (strnicmp_prefix(line, "kd=")) {
        cmd->type = VOFA_CMD_SET_KD;
        if (safe_atof(line + 3, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* 说明：VOFA+通信相关处理。 */
    if (strnicmp_prefix(line, "ffk=")) {
        cmd->type = VOFA_CMD_SET_FF_K;
        if (safe_atof(line + 4, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "FFb=x" (前馈截距) */
    if (strnicmp_prefix(line, "ffb=")) {
        cmd->type = VOFA_CMD_SET_FF_B;
        if (safe_atof(line + 4, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "FFe=x" (前馈使能: 1=使能, 0=禁用) */
    if (strnicmp_prefix(line, "ffe=")) {
        cmd->type = VOFA_CMD_SET_FF_ENABLE;
        if (safe_atof(line + 4, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "Sweep" (自动扫频标定) */
    if (stricmp_equal(line, "sweep")) {
        cmd->type = VOFA_CMD_SWEEP;
        return true;
    }

    /* "Menu" 命令 */
    if (stricmp_equal(line, "menu")) {
        cmd->type = VOFA_CMD_MENU;
        return true;
    }

    /* 说明：VOFA+通信相关处理。 */
    if (strnicmp_prefix(line, "ffkp=")) {
        cmd->type = VOFA_CMD_SET_FF_KP;
        if (safe_atof(line + 5, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "FFKi=x" (FF模式积分增益) */
    if (strnicmp_prefix(line, "ffki=")) {
        cmd->type = VOFA_CMD_SET_FF_KI;
        if (safe_atof(line + 5, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "FFKd=x" (FF模式微分增益) */
    if (strnicmp_prefix(line, "ffkd=")) {
        cmd->type = VOFA_CMD_SET_FF_KD;
        if (safe_atof(line + 5, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* ---- 位置-速度串级控制命令 ---- */

    /* "mode=speed/position/angle" (切换控制模式) */
    if (strnicmp_prefix(line, "mode=")) {
        cmd->type = VOFA_CMD_SET_MODE;
        const char *p = line + 5;
        if (stricmp_equal(p, "speed")) {
            cmd->value = (float)APP_CTRL_MODE_SPEED;
            cmd->has_value = true;
            return true;
        } else if (stricmp_equal(p, "position")) {
            cmd->value = (float)APP_CTRL_MODE_POSITION;
            cmd->has_value = true;
            return true;
        } else if (stricmp_equal(p, "angle")) {
            cmd->value = (float)APP_CTRL_MODE_ANGLE;
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "pos=<脉冲>,<巡航RPM>" (位置控制, 双号分隔) */
    if (strnicmp_prefix(line, "pos=")) {
        cmd->type = VOFA_CMD_POS;
        const char *p = line + 4;
        const char *comma = strchr(p, ',');
        if (comma != NULL) {
            /* 说明：VOFA+通信相关处理。 */
            char buf[32];
            size_t len = (size_t)(comma - p);
            if (len > 0 && len < sizeof(buf)) {
                memcpy(buf, p, len);
                buf[len] = '\0';
                if (safe_atof(buf, &cmd->value) &&
                    safe_atof(comma + 1, &cmd->value2)) {
                    cmd->has_value = true;
                    cmd->has_value2 = true;
                    return true;
                }
            }
        }
        return false;
    }

    /* 说明：VOFA+通信相关处理。 */
    if (strnicmp_prefix(line, "angle=")) {
        cmd->type = VOFA_CMD_ANGLE;
        const char *p = line + 6;
        const char *comma = strchr(p, ',');
        if (comma != NULL) {
            char buf[32];
            size_t len = (size_t)(comma - p);
            if (len > 0 && len < sizeof(buf)) {
                memcpy(buf, p, len);
                buf[len] = '\0';
                if (safe_atof(buf, &cmd->value) &&
                    safe_atof(comma + 1, &cmd->value2)) {
                    cmd->has_value = true;
                    cmd->has_value2 = true;
                    return true;
                }
            }
        }
        return false;
    }

    /* 说明：VOFA+通信相关处理。 */
    if (stricmp_equal(line, "abort")) {
        cmd->type = VOFA_CMD_ABORT;
        return true;
    }

    return false;
}

void app_vofa_apply_cmd(const vofa_cmd_t *cmd,
                         app_shared_ctx_t *ctx,
                         uint32_t *current_motor,
                         bool *need_refresh)
{
    if (cmd == NULL || ctx == NULL || current_motor == NULL) {
        return;
    }

    uint32_t mid = *current_motor;

    /* 使用命令中的电机 ID */
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
                vofa_apply_pid_param(&ctx->pid[mid], cmd->value, PID_PARAM_KP, mid);
            }
        }
        break;

    case VOFA_CMD_SET_KI:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] Ki rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                vofa_apply_pid_param(&ctx->pid[mid], cmd->value, PID_PARAM_KI, mid);
            }
        }
        break;

    case VOFA_CMD_SET_KD:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] Kd rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                vofa_apply_pid_param(&ctx->pid[mid], cmd->value, PID_PARAM_KD, mid);
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
                if (val > PRJ_VOFA_TARGET_RPM_MAX) { val = PRJ_VOFA_TARGET_RPM_MAX; }
                if (val < -PRJ_VOFA_TARGET_RPM_MAX) { val = -PRJ_VOFA_TARGET_RPM_MAX; }
                float old_sp;
                OSAL_CRITICAL_SECTION {
                    old_sp = ctx->pid[mid].setpoint;
                    app_pid_set_setpoint(&ctx->pid[mid], val);
                    /* FF duty 在控制任务中自动更新, PID integral 保持不变 */
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
            /* 说明：VOFA+通信相关处理。 */
            app_pid_reset(&ctx->pid[mid]);
            ctx->motor_enabled[mid] = true;
        }
        (void)printf("[%lu] Run (target=%.0f RPM)\r\n",
            (unsigned long)mid,
            (double)ctx->pid[mid].setpoint);
        break;

    case VOFA_CMD_STOP:
        g_sweep_cancel = true;  /* 中断 Sweep */
        app_motor_stop(ctx, mid);
        (void)printf("[%lu] Stop\r\n", (unsigned long)mid);
        break;

    case VOFA_CMD_STOP_ALL:
        g_sweep_cancel = true;  /* 中断 Sweep */
        app_motor_stop_all(ctx);
        (void)printf("All stopped\r\n");
        break;

    case VOFA_CMD_SET_FF_K:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] FFk rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                OSAL_CRITICAL_SECTION {
                    ctx->ff[mid].k = cmd->value;
                }
                (void)printf("[%lu] FF_k = %.4f\r\n",
                    (unsigned long)mid, (double)cmd->value);
            }
        }
        break;

    case VOFA_CMD_SET_FF_B:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] FFb rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                OSAL_CRITICAL_SECTION {
                    ctx->ff[mid].b = cmd->value;
                }
                (void)printf("[%lu] FF_b = %.4f\r\n",
                    (unsigned long)mid, (double)cmd->value);
            }
        }
        break;

    case VOFA_CMD_SET_FF_ENABLE:
        if (cmd->has_value) {
            bool en = (cmd->value != 0.0f);
            OSAL_CRITICAL_SECTION {
                app_ff_set_enabled(&ctx->ff[mid], en);
                ctx->pid[mid].use_ff = en;
            }
            (void)printf("[%lu] FF %s (restart motor to apply)\r\n",
                (unsigned long)mid,
                en ? "enabled" : "disabled");
        }
        break;

    case VOFA_CMD_SWEEP:
    {
        app_ff_sweep_result_t sweep_result;
        (void)printf("[%lu] Sweep starting (~40s)...\r\n",
            (unsigned long)mid);

        /* 说明：VOFA+通信相关处理。 */
        if (app_ff_sweep(ctx, mid, &sweep_result)) {
            /* 说明：VOFA+通信相关处理。 */
            float k, b;
            if (app_ff_fit_linear(&sweep_result, &k, &b)) {
                /* 自动应用拟合结果 */
                OSAL_CRITICAL_SECTION {
                    ctx->ff[mid].k = k;
                    ctx->ff[mid].b = b;
                    ctx->ff[mid].enabled = true;
                }
                (void)printf("[SWEEP] k=%.4f b=%.4f (auto-applied)\r\n",
                    (double)k, (double)b);
            } else {
                (void)printf("[SWEEP] Fit failed\r\n");
            }
        } else {
            (void)printf("[SWEEP] Failed\r\n");
        }
        break;
    }

    case VOFA_CMD_MENU:
        if (need_refresh != NULL) {
            *need_refresh = true;
        }
        break;

    case VOFA_CMD_SET_FF_KP:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] FFKp rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                float val = cmd->value;
                if (val >  PRJ_VOFA_PID_PARAM_MAX) { val =  PRJ_VOFA_PID_PARAM_MAX; }
                if (val < -PRJ_VOFA_PID_PARAM_MAX) { val = -PRJ_VOFA_PID_PARAM_MAX; }
                OSAL_CRITICAL_SECTION {
                    ctx->pid[mid].ff_kp = val;
                }
                (void)printf("[%lu] FF_Kp = %.4f\r\n",
                    (unsigned long)mid, (double)val);
            }
        }
        break;

    case VOFA_CMD_SET_FF_KI:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] FFKi rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                float val = cmd->value;
                if (val >  PRJ_VOFA_PID_PARAM_MAX) { val =  PRJ_VOFA_PID_PARAM_MAX; }
                if (val < -PRJ_VOFA_PID_PARAM_MAX) { val = -PRJ_VOFA_PID_PARAM_MAX; }
                OSAL_CRITICAL_SECTION {
                    ctx->pid[mid].ff_ki = val;
                }
                (void)printf("[%lu] FF_Ki = %.4f\r\n",
                    (unsigned long)mid, (double)val);
            }
        }
        break;

    case VOFA_CMD_SET_FF_KD:
        if (cmd->has_value) {
            if (!isfinite(cmd->value)) {
                (void)printf("[%lu] FFKd rejected: NaN/Inf\r\n",
                    (unsigned long)mid);
            } else {
                float val = cmd->value;
                if (val >  PRJ_VOFA_PID_PARAM_MAX) { val =  PRJ_VOFA_PID_PARAM_MAX; }
                if (val < -PRJ_VOFA_PID_PARAM_MAX) { val = -PRJ_VOFA_PID_PARAM_MAX; }
                OSAL_CRITICAL_SECTION {
                    ctx->pid[mid].ff_kd = val;
                }
                (void)printf("[%lu] FF_Kd = %.4f\r\n",
                    (unsigned long)mid, (double)val);
            }
        }
        break;

    case VOFA_CMD_STEP:
    {
        int32_t pwm = ID_DEFAULT_PWM_STEP;
        if (cmd->has_value) {
            int32_t v = (int32_t)cmd->value;
            if (v > 0 && v <= ID_PWM_MAX) pwm = v;
        }

        (void)printf("[%lu] Step test starting (PWM=%ld)...\r\n",
            (unsigned long)mid, (long)pwm);

        app_id_step_result_t step_result;
        bool ok = vofa_run_step_id(ctx, mid, pwm, &step_result);

        if (ok) {
            g_id_data.result = step_result;
            (void)printf("[STEP] K=%.4f (RPM/PWM)  "
                "T=%.3f (s)  omega_ss=%.0f (RPM)  "
                "fit=%.1f%%\r\n",
                (double)step_result.K,
                (double)step_result.T_s,
                (double)step_result.omega_ss,
                (double)(step_result.fit_quality * 100.0f));

            float ff_k = 1.0f / step_result.K;
            if (isfinite(ff_k) && ff_k > 0.0f) {
                OSAL_CRITICAL_SECTION {
                    ctx->ff[mid].k = ff_k;
                }
                (void)printf("[STEP] FF_k auto-updated: %.6f\r\n",
                    (double)ff_k);
            }
        } else {
            (void)printf("[STEP] Identification failed"
                " or aborted\r\n");
        }
        break;
    }

    case VOFA_CMD_AUTOTUNE:
    {
        int32_t pwm = ID_DEFAULT_PWM_STEP;
        (void)printf("[%lu] Auto-tune starting (PWM=%ld)...\r\n",
            (unsigned long)mid, (long)pwm);

        app_id_step_result_t step_result;
        bool ok = vofa_run_step_id(ctx, mid, pwm, &step_result);

        if (!ok) {
            (void)printf("[AUTO] Identification failed"
                " or aborted\r\n");
            return;
        }

        g_id_data.result = step_result;
        (void)printf("[AUTO] K=%.4f  T=%.3fs  fit=%.1f%%\r\n",
            (double)step_result.K,
            (double)step_result.T_s,
            (double)(step_result.fit_quality * 100.0f));

        /* 极点配置计算PID参数 */
        float bw = 5.0f;  /* 默认5Hz闭环带宽 */
        if (cmd->has_value && cmd->value > 0.5f) {
            bw = cmd->value;
        }

        float kp, ki;
        if (app_id_pole_placement(&step_result, bw, 0.707f,
                                   &kp, &ki)) {
            OSAL_CRITICAL_SECTION {
                app_pid_set_params(&ctx->pid[mid], kp, ki, 0.0f);
            }
            (void)printf("[AUTO] PI applied: "
                "Kp=%.4f  Ki=%.4f  (BW=%.1fHz)\r\n",
                (double)kp, (double)ki, (double)bw);

            /* 自动更新前馈 */
            float ff_k = 1.0f / step_result.K;
            if (isfinite(ff_k) && ff_k > 0.0f) {
                OSAL_CRITICAL_SECTION {
                    ctx->ff[mid].k = ff_k;
                }
                (void)printf("[AUTO] FF_k=%.6f (auto-applied)\r\n",
                    (double)ff_k);
            }
        } else {
            (void)printf("[AUTO] Pole placement failed\r\n");
        }
        break;
    }

    /* ---- 位置-速度串级控制命令 ---- */

    case VOFA_CMD_SET_MODE:
    {
        if (!cmd->has_value) { break; }
        app_ctrl_mode_t new_mode = (app_ctrl_mode_t)cmd->value;

        /* 切换模式时读取 4 个电机的当前 RPM, 避免速度突变 */
        float cur_rpm[APP_POS_MOTOR_COUNT];
        OSAL_CRITICAL_SECTION {
            for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
                cur_rpm[i] = (float)ctx->status.rpm[i];
            }
        }

        uint32_t now = osal_get_tick_count();
        OSAL_CRITICAL_SECTION {
            app_posctrl_set_mode(&ctx->posctrl, new_mode,
                                  cur_rpm, now);
        }

        const char *mode_str = "unknown";
        switch (new_mode) {
        case APP_CTRL_MODE_SPEED:    mode_str = "SPEED"; break;
        case APP_CTRL_MODE_POSITION: mode_str = "POSITION"; break;
        case APP_CTRL_MODE_ANGLE:    mode_str = "ANGLE"; break;
        default: break;
        }
        (void)printf("[MODE] %s\r\n", mode_str);
        break;
    }

    case VOFA_CMD_POS:
    {
        if (!cmd->has_value || !cmd->has_value2) { break; }

        float target_pulses = cmd->value;
        float cruise_rpm   = cmd->value2;

        /* 参数校验 */
        if (!isfinite(target_pulses) || !isfinite(cruise_rpm)) {
            (void)printf("[POS] rejected: NaN/Inf\r\n");
            break;
        }
        if (cruise_rpm <= 0.0f ||
            cruise_rpm > PRJ_PLANNER_MAX_RPM) {
            (void)printf("[POS] rejected: speed %.0f out of range\r\n",
                (double)cruise_rpm);
            break;
        }

        /* 如果当前不是POSITION模式, 先切模式 */
        uint32_t now = osal_get_tick_count();
        if (ctx->posctrl.mode != APP_CTRL_MODE_POSITION) {
            float cur_rpm[APP_POS_MOTOR_COUNT];
            OSAL_CRITICAL_SECTION {
                for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
                    cur_rpm[i] = (float)ctx->status.rpm[i];
                }
            }
            OSAL_CRITICAL_SECTION {
                app_posctrl_set_mode(&ctx->posctrl,
                    APP_CTRL_MODE_POSITION, cur_rpm, now);
            }
        }

        /* 说明：VOFA+通信相关处理。 */
        int32_t enc_totals[BSP_ENCODER_COUNT];
        (void)bsp_encoder_get_all_totals(enc_totals);
        float enc_avg = 0.0f;
        app_pos_feedback_t start_feedback = {0};
        uint32_t feedback_count = 0U;
        for (uint32_t motor = 0U; motor < BSP_MOTOR_COUNT; motor++) {
            if ((PRJ_MOTION_FEEDBACK_MASK & (1UL << motor)) == 0UL) {
                continue;
            }
            uint32_t encoder_id = (uint32_t)s_vofa_motor_encoder_map[motor];
            if (encoder_id < BSP_ENCODER_COUNT) {
                start_feedback.position[encoder_id] =
                    (float)enc_totals[encoder_id];
                start_feedback.valid_mask |= (1UL << encoder_id);
                enc_avg += (float)enc_totals[encoder_id];
                feedback_count++;
            }
        }
        if (feedback_count == 0U) {
            (void)printf("[POS] rejected: no feedback encoder\r\n");
            break;
        }
        enc_avg /= (float)feedback_count;
        start_feedback.average_position = enc_avg;

        /* 启动位置控制 */
        OSAL_CRITICAL_SECTION {
            app_posctrl_start_position_feedback(&ctx->posctrl,
                target_pulses, cruise_rpm, enc_avg, &start_feedback);
        }

        (void)printf("[POS] target=%.0f pulses, cruise=%.0f RPM\r\n",
            (double)target_pulses, (double)cruise_rpm);
        break;
    }

    case VOFA_CMD_ANGLE:
    {
        if (!cmd->has_value || !cmd->has_value2) { break; }

        float target_angle = cmd->value;
        float cruise_rpm   = cmd->value2;

        /* 参数校验 */
        if (!isfinite(target_angle) || !isfinite(cruise_rpm)) {
            (void)printf("[ANGLE] rejected: NaN/Inf\r\n");
            break;
        }
        if (cruise_rpm <= 0.0f ||
            cruise_rpm > PRJ_PLANNER_MAX_RPM) {
            (void)printf("[ANGLE] rejected: speed %.0f out of range\r\n",
                (double)cruise_rpm);
            break;
        }

        /* 如果当前不是ANGLE模式, 先切模式 */
        uint32_t now = osal_get_tick_count();
        if (ctx->posctrl.mode != APP_CTRL_MODE_ANGLE) {
            float cur_rpm[APP_POS_MOTOR_COUNT];
            OSAL_CRITICAL_SECTION {
                for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
                    cur_rpm[i] = (float)ctx->status.rpm[i];
                }
            }
            OSAL_CRITICAL_SECTION {
                app_posctrl_set_mode(&ctx->posctrl,
                    APP_CTRL_MODE_ANGLE, cur_rpm, now);
            }
        }

        /* 读取当前yaw */
        float cur_yaw;
        OSAL_CRITICAL_SECTION {
            cur_yaw = ctx->imu.yaw;
        }

        /* 启动角度控制 */
        OSAL_CRITICAL_SECTION {
            app_posctrl_start_angle(&ctx->posctrl,
                target_angle, cruise_rpm, cur_yaw);
        }

        (void)printf("[ANGLE] target=%.1f deg, cruise=%.0f RPM\r\n",
            (double)target_angle, (double)cruise_rpm);
        break;
    }

    case VOFA_CMD_ABORT:
    {
        OSAL_CRITICAL_SECTION {
            app_posctrl_emergency_stop(&ctx->posctrl);
        }
        app_motor_stop_all(ctx);
        (void)printf("[ABORT] Position control stopped\r\n");
        break;
    }

    case VOFA_CMD_INFO_QUERY:
        (void)printf("@INFO,fw=%s,proto=%lu,board=%s,driver=%s,motors=%lu,baud=%lu,telemetry=firewater\r\n",
            PRJ_VERSION_STRING, (unsigned long)PRJ_PROTOCOL_VERSION,
            PRJ_BOARD_NAME, PRJ_MOTOR_DRIVER_NAME,
            (unsigned long)BSP_MOTOR_COUNT,
            (unsigned long)UART_0_DEBUG_BAUD_RATE);
        break;

    case VOFA_CMD_CONFIG_QUERY:
        (void)printf("@CONFIG,ppr=%lu,gear_num=%lu,gear_den=%lu,decode=%lu,cpr=%lu,wheel_mm=%.2f,wheelbase_m=%.4f,dead_low=%lu,neutral=%lu,dead_high=%lu,period_ms=%lu,channels=%lu\r\n",
            (unsigned long)PRJ_MOTOR_ENCODER_PPR,
            (unsigned long)PRJ_MOTOR_GEAR_RATIO_NUMERATOR,
            (unsigned long)PRJ_MOTOR_GEAR_RATIO_DENOMINATOR,
            (unsigned long)PRJ_ENCODER_DECODE_MULTIPLIER,
            (unsigned long)PRJ_ENCODER_PULSES_PER_REV,
            (double)PRJ_MOTOR_WHEEL_DIAMETER_MM,
            (double)PRJ_CF_WHEEL_BASE_M,
            (unsigned long)PRJ_DRV8870_DEADBAND_LOW_PERCENT,
            (unsigned long)PRJ_DRV8870_NEUTRAL_PERCENT,
            (unsigned long)PRJ_DRV8870_DEADBAND_HIGH_PERCENT,
            (unsigned long)PRJ_RPM_OUTPUT_PERIOD_MS,
            (unsigned long)VOFA_TELEMETRY_CHANNEL_COUNT);
        break;

    case VOFA_CMD_STATUS_QUERY:
    {
        app_state_snapshot_t snapshot;
        if (!app_state_snapshot_read(ctx, &snapshot)) {
            (void)printf("@STATUS,error=snapshot\r\n");
            break;
        }

        const app_motor_state_snapshot_t *motor = &snapshot.motor[mid];
        bool enabled = motor->enabled;
        bool ff_enabled = motor->ff_enabled;
        int32_t rpm = motor->rpm;
        int32_t output = motor->output;
        float target = motor->target;
        float kp = motor->kp;
        float ki = motor->ki;
        float kd = motor->kd;
        float ff_k = motor->ff_k;
        float ff_b = motor->ff_b;
        app_ctrl_mode_t mode = snapshot.mode;
        (void)printf("@STATUS,motor=%lu,enabled=%u,power=%u,rpm=%ld,target=%.3f,output=%ld,kp=%.6f,ki=%.6f,kd=%.6f,ff_en=%u,ff_k=%.6f,ff_b=%.6f,mode=%s\r\n",
            (unsigned long)mid, enabled ? 1U : 0U,
            bsp_motor_power_is_enabled() ? 1U : 0U,
            (long)rpm, (double)target, (long)output,
            (double)kp, (double)ki, (double)kd,
            ff_enabled ? 1U : 0U, (double)ff_k, (double)ff_b,
            vofa_mode_name(mode));
        break;
    }

    case VOFA_CMD_STREAM_ON:
        (void)printf("@ACK,cmd=stream,state=1\r\n");
        break;

    case VOFA_CMD_STREAM_OFF:
        (void)printf("@ACK,cmd=stream,state=0\r\n");
        break;

    default:
        break;
    }
}
