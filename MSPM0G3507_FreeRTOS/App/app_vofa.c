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
#include "app_feedforward.h"
#include "app_model_id.h"
#include "osal_api.h"
#include "bsp_uart.h"
#include "bsp_motor.h"
#include "axiomtrace.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ======================== 私有类型 ======================== */

typedef enum {
    PID_PARAM_KP = 0,
    PID_PARAM_KI,
    PID_PARAM_KD
} pid_param_idx_t;

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
 * @brief  安全解析浮点数值(使用strtof替代atof)
 * @param  numstr  数值字符串
 * @param  out_val 输出值
 * @retval true 解析成功
 */
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
    if (new_val >  VOFA_PID_PARAM_MAX) { new_val =  VOFA_PID_PARAM_MAX; }
    if (new_val < -VOFA_PID_PARAM_MAX) { new_val = -VOFA_PID_PARAM_MAX; }

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

/* ======================== 私有辅助：ID 测试中止检测 ======================== */

/**
 * @brief  非阻塞检测 UART 缓冲区中的 "Stop" 命令
 * @retval true  检测到 "Stop" / "StopAll" / "Stop=x"
 * @note   消耗 UART 字符，用于在长时间阻塞操作(Step/Auto/Sweep)中支持提前中止
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
 * @brief  执行阶跃响应辨识并等待完成(阻塞，带中止检测)
 * @param  ctx    共享上下文
 * @param  mid    目标电机索引
 * @param  pwm    阶跃PWM值
 * @param  result 输出: 辨识结果
 * @retval true   辨识成功
 * @retval false  失败或中止
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

    float dt_s = (float)APP_CONTROL_PERIOD_MS / 1000.0f;
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

    /* "Step" 或 "Step=x" (阶跃响应辨识, x=PWM值 0~1000) */
    if (strnicmp_prefix(line, "step")) {
        cmd->type = VOFA_CMD_STEP;
        const char *eq = strchr(line, '=');
        if (eq != NULL) {
            cmd->has_value = safe_atof(eq + 1, &cmd->value);
        }
        return true;
    }

    /* "Auto" 或 "Auto=x" (自动整定, x=期望闭环带宽Hz) */
    if (strnicmp_prefix(line, "auto")) {
        cmd->type = VOFA_CMD_AUTOTUNE;
        const char *eq = strchr(line, '=');
        if (eq != NULL) {
            cmd->has_value = safe_atof(eq + 1, &cmd->value);
        }
        return true;
    }

    /* ---- 辅助: 安全解析电机ID ---- */
    #define PARSE_MOTOR_ID(str, out_id) do {            \
        char *_endptr;                                   \
        long _val = strtol((str), &_endptr, 10);         \
        if (*_endptr != '\0' || _endptr == (str)) {      \
            return false; /* 非数字输入 */                \
        }                                                \
        if (_val < 0 || _val >= (long)BSP_MOTOR_COUNT) { \
            return false; /* 超出范围 */                  \
        }                                                \
        *(out_id) = (uint32_t)_val;                      \
    } while(0)

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
            PARSE_MOTOR_ID(eq + 1, &cmd->motor_id);
            cmd->has_motor = true;
        }
        return true;
    }

    /* "Run" 或 "Run=x" */
    if (strnicmp_prefix(line, "run")) {
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

    /* "FFk=x" (前馈斜率, 必须在 "FFb" 之前) */
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
    if (strnicmp_prefix(line, "sweep")) {
        cmd->type = VOFA_CMD_SWEEP;
        return true;
    }

    /* "Menu" (刷新菜单显示) */
    if (strnicmp_prefix(line, "menu")) {
        cmd->type = VOFA_CMD_MENU;
        return true;
    }

    /* "FFKp=x" (FF模式比例增益, 必须在 "FFKi" 之前) */
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
                if (val > VOFA_TARGET_RPM_MAX) { val = VOFA_TARGET_RPM_MAX; }
                if (val < -VOFA_TARGET_RPM_MAX) { val = -VOFA_TARGET_RPM_MAX; }
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
            /* PID 状态复位, FF duty 由控制任务独立计算 */
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

        /* 执行扫频(阻塞约33秒) */
        if (app_ff_sweep(ctx, mid, &sweep_result)) {
            /* 最小二乘拟合 */
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
                if (val >  VOFA_PID_PARAM_MAX) { val =  VOFA_PID_PARAM_MAX; }
                if (val < -VOFA_PID_PARAM_MAX) { val = -VOFA_PID_PARAM_MAX; }
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
                if (val >  VOFA_PID_PARAM_MAX) { val =  VOFA_PID_PARAM_MAX; }
                if (val < -VOFA_PID_PARAM_MAX) { val = -VOFA_PID_PARAM_MAX; }
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
                if (val >  VOFA_PID_PARAM_MAX) { val =  VOFA_PID_PARAM_MAX; }
                if (val < -VOFA_PID_PARAM_MAX) { val = -VOFA_PID_PARAM_MAX; }
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
            if (v > 0 && v < 1000) pwm = v;
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

    default:
        break;
    }
}