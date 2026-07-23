/**
 * @file    app_vofa.c
 * @brief   VOFA+ 閫氫俊鍗忚瀹炵幇
 * @note    FireWater: 鏂囨湰鏍煎紡, printf 杈撳嚭
 *          鏍煎紡: "val0,val1,val2,...,valN\n"
 *          JustFloat: 浜岃繘鍒舵牸寮? bsp_uart_putc 閫愬瓧鑺傝緭鍑? *          涓嬭鍛戒护: 鍩轰簬 sscanf/key-value 瑙ｆ瀽
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
#include "project_config.h"  /* PROJECT_VERSION_* 已合并 */
#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ======================== 绉佹湁绫诲瀷 ======================== */

typedef enum {
    PID_PARAM_KP = 0,
    PID_PARAM_KI,
    PID_PARAM_KD
} pid_param_idx_t;

/* ======================== 绉佹湁杈呭姪鍑芥暟 ======================== */

/**
 * @brief  蹇界暐澶у皬鍐欑殑鍓嶇紑姣旇緝
 * @param  str    瀹屾暣瀛楃涓? * @param  prefix 鍓嶇紑(灏忓啓)
 * @retval true  str 浠?prefix 寮€澶?蹇界暐澶у皬鍐?
 */
static bool strnicmp_prefix(const char *str, const char *prefix)
{
    while (*prefix != '\0') {
        if (*str == '\0') {
            return false;
        }
        char c1 = *str;
        char c2 = *prefix;
        /* 绠€鍗曞皬鍐欒浆鎹?(浠匒SCII瀛楁瘝) */
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
 * @brief  瀹夊叏瑙ｆ瀽娴偣鏁板€?浣跨敤strtof鏇夸唬atof)
 * @param  numstr  鏁板€煎瓧绗︿覆
 * @param  out_val 杈撳嚭鍊? * @retval true 瑙ｆ瀽鎴愬姛
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
 * @brief  搴旂敤鍗曚釜PID鍙傛暟鏇存柊
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
 * @brief  鍙戦€佸崟涓瓧鑺?灏佽 bsp_uart_putc)
 */
static void vofa_putc(uint8_t ch)
{
    (void)bsp_uart_putc(ch);
}

/**
 * @brief  鍙戦€?float 鐨勫師濮嬪瓧鑺?灏忕搴? JustFloat 鐢?
 */
static void vofa_send_float_bytes(float val)
{
    const uint8_t *p = (const uint8_t *)&val;
    for (uint32_t i = 0; i < sizeof(float); i++) {
        vofa_putc(p[i]);
    }
}

/* ======================== 绉佹湁杈呭姪锛欼D 娴嬭瘯涓妫€娴?======================== */

/**
 * @brief  闈為樆濉炴娴?UART 缂撳啿鍖轰腑鐨?"Stop" 鍛戒护
 * @retval true  妫€娴嬪埌 "Stop" / "StopAll" / "Stop=x"
 * @note   娑堣€?UART 瀛楃锛岀敤浜庡湪闀挎椂闂撮樆濉炴搷浣?Step/Auto/Sweep)涓敮鎸佹彁鍓嶄腑姝? */
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
 * @brief  鎵ц闃惰穬鍝嶅簲杈ㄨ瘑骞剁瓑寰呭畬鎴?闃诲锛屽甫涓妫€娴?
 * @param  ctx    鍏变韩涓婁笅鏂? * @param  mid    鐩爣鐢垫満绱㈠紩
 * @param  pwm    闃惰穬PWM鍊? * @param  result 杈撳嚭: 杈ㄨ瘑缁撴灉
 * @retval true   杈ㄨ瘑鎴愬姛
 * @retval false  澶辫触鎴栦腑姝? */
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

    float dt_s = (float)APP_CONTROL_PERIOD_MS / (float)PRJ_MS_PER_S;
    return app_id_process_step(g_id_data.rpm_buf, g_id_data.write_idx,
                                pwm, dt_s, result);
}

/* ======================== 鍏叡鍑芥暟瀹炵幇 ======================== */

void app_vofa_send_firewater(const float channels[], uint32_t count)
{
    if (channels == NULL || count == 0U) {
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (i > 0U) {
            vofa_putc(',');
        }
        /* 鏍煎紡: "val0,val1,...,valN\n" */
        (void)printf("%.6f", (double)channels[i]);
    }
    vofa_putc('\n');
}

void app_vofa_send_justfloat(const float channels[], uint32_t count)
{
    if (channels == NULL || count == 0U) {
        return;
    }

    /* 鍙戦€佹墍鏈夐€氶亾鐨勫師濮嬫诞鐐瑰瓧鑺?*/
    for (uint32_t i = 0; i < count; i++) {
        vofa_send_float_bytes(channels[i]);
    }

    /* 鍙戦€佸抚灏?0x00 0x00 0x80 0x7F */
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

    /* 璺宠繃鍓嶅绌虹櫧 */
    while (*line == ' ' || *line == '\t') {
        line++;
    }

    /* 璺宠繃绌鸿 */
    if (*line == '\0') {
        return false;
    }

    /* 娓呴浂鍛戒护缁撴瀯 */
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

    /* "Step" 鎴?"Step=x" (闃惰穬鍝嶅簲杈ㄨ瘑, x=鏈夌鍙峰懡浠ゅ箙鍊?~500) */
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

    /* "Auto" 鎴?"Auto=x" (鑷姩鏁村畾, x=鏈熸湜闂幆甯﹀Hz) */
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

    /* ---- 杈呭姪: 瀹夊叏瑙ｆ瀽鐢垫満ID ---- */
    #define PARSE_MOTOR_ID(str, out_id) do {            \
        char *_endptr;                                   \
        long _val = strtol((str), &_endptr, 10);         \
        if (*_endptr != '\0' || _endptr == (str)) {      \
            return false; /* 闈炴暟瀛楄緭鍏?*/                \
        }                                                \
        if (_val < 0 || _val >= (long)BSP_MOTOR_COUNT) { \
            return false; /* 瓒呭嚭鑼冨洿 */                  \
        }                                                \
        *(out_id) = (uint32_t)_val;                      \
    } while(0)

    /* ---- 閫愬叧閿瓧鍖归厤 ---- */

    /* "StopAll" (蹇呴』鍦?"Stop" 涔嬪墠鍖归厤) */
    if (stricmp_equal(line, "stopall")) {
        cmd->type = VOFA_CMD_STOP_ALL;
        return true;
    }

    /* "Stop" 鎴?"Stop=x" */
    if (stricmp_equal(line, "stop") || strnicmp_prefix(line, "stop=")) {
        cmd->type = VOFA_CMD_STOP;
        const char *eq = strchr(line, '=');
        if (eq != NULL) {
            PARSE_MOTOR_ID(eq + 1, &cmd->motor_id);
            cmd->has_motor = true;
        }
        return true;
    }

    /* "Run" 鎴?"Run=x" */
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

    /* "FFk=x" (鍓嶉鏂滅巼, 蹇呴』鍦?"FFb" 涔嬪墠) */
    if (strnicmp_prefix(line, "ffk=")) {
        cmd->type = VOFA_CMD_SET_FF_K;
        if (safe_atof(line + 4, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "FFb=x" (鍓嶉鎴窛) */
    if (strnicmp_prefix(line, "ffb=")) {
        cmd->type = VOFA_CMD_SET_FF_B;
        if (safe_atof(line + 4, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "FFe=x" (鍓嶉浣胯兘: 1=浣胯兘, 0=绂佺敤) */
    if (strnicmp_prefix(line, "ffe=")) {
        cmd->type = VOFA_CMD_SET_FF_ENABLE;
        if (safe_atof(line + 4, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "Sweep" (鑷姩鎵鏍囧畾) */
    if (stricmp_equal(line, "sweep")) {
        cmd->type = VOFA_CMD_SWEEP;
        return true;
    }

    /* "Menu" (鍒锋柊鑿滃崟鏄剧ず) */
    if (stricmp_equal(line, "menu")) {
        cmd->type = VOFA_CMD_MENU;
        return true;
    }

    /* "FFKp=x" (FF妯″紡姣斾緥澧炵泭, 蹇呴』鍦?"FFKi" 涔嬪墠) */
    if (strnicmp_prefix(line, "ffkp=")) {
        cmd->type = VOFA_CMD_SET_FF_KP;
        if (safe_atof(line + 5, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "FFKi=x" (FF妯″紡绉垎澧炵泭) */
    if (strnicmp_prefix(line, "ffki=")) {
        cmd->type = VOFA_CMD_SET_FF_KI;
        if (safe_atof(line + 5, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* "FFKd=x" (FF妯″紡寰垎澧炵泭) */
    if (strnicmp_prefix(line, "ffkd=")) {
        cmd->type = VOFA_CMD_SET_FF_KD;
        if (safe_atof(line + 5, &cmd->value)) {
            cmd->has_value = true;
            return true;
        }
        return false;
    }

    /* ---- 浣嶇疆-閫熷害涓茬骇鎺у埗鍛戒护 ---- */

    /* "mode=speed/position/angle" (鍒囨崲鎺у埗妯″紡) */
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

    /* "pos=<鑴夊啿>,<宸¤埅RPM>" (浣嶇疆鎺у埗, 鍙屽€奸€楀彿鍒嗛殧) */
    if (strnicmp_prefix(line, "pos=")) {
        cmd->type = VOFA_CMD_POS;
        const char *p = line + 4;
        const char *comma = strchr(p, ',');
        if (comma != NULL) {
            /* 瑙ｆ瀽绗竴鍊?鑴夊啿) */
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

    /* "angle=<搴?,<宸¤埅RPM>" (瑙掑害鎺у埗, 鍙屽€奸€楀彿鍒嗛殧) */
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

    /* "abort" (绱ф€ュ仠姝綅缃帶鍒? */
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

    /* 濡傛灉鍛戒护鎸囧畾浜嗙數鏈篒D, 鏇存柊褰撳墠鐢垫満 */
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
                    /* FF duty 鍦ㄦ帶鍒朵换鍔′腑鑷姩鏇存柊, PID integral 淇濇寔涓嶅彉 */
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
            /* PID 鐘舵€佸浣? FF duty 鐢辨帶鍒朵换鍔＄嫭绔嬭绠?*/
            app_pid_reset(&ctx->pid[mid]);
            ctx->motor_enabled[mid] = true;
        }
        (void)printf("[%lu] Run (target=%.0f RPM)\r\n",
            (unsigned long)mid,
            (double)ctx->pid[mid].setpoint);
        break;

    case VOFA_CMD_STOP:
        g_sweep_cancel = true;  /* 涓柇 Sweep */
        app_motor_stop(ctx, mid);
        (void)printf("[%lu] Stop\r\n", (unsigned long)mid);
        break;

    case VOFA_CMD_STOP_ALL:
        g_sweep_cancel = true;  /* 涓柇 Sweep */
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

        /* 鎵ц鎵(闃诲绾?3绉? */
        if (app_ff_sweep(ctx, mid, &sweep_result)) {
            /* 鏈€灏忎簩涔樻嫙鍚?*/
            float k, b;
            if (app_ff_fit_linear(&sweep_result, &k, &b)) {
                /* 鑷姩搴旂敤鎷熷悎缁撴灉 */
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

        /* 鏋佺偣閰嶇疆璁＄畻PID鍙傛暟 */
        float bw = 5.0f;  /* 榛樿5Hz闂幆甯﹀ */
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

            /* 鑷姩鏇存柊鍓嶉 */
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

    /* ---- 浣嶇疆-閫熷害涓茬骇鎺у埗鍛戒护 ---- */

    case VOFA_CMD_SET_MODE:
    {
        if (!cmd->has_value) { break; }
        app_ctrl_mode_t new_mode = (app_ctrl_mode_t)cmd->value;

        /* 鑾峰彇褰撳墠4璺疪PM鐢ㄤ簬P0杩囨浮 */
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

        /* 鍙傛暟鏍￠獙 */
        if (!isfinite(target_pulses) || !isfinite(cruise_rpm)) {
            (void)printf("[POS] rejected: NaN/Inf\r\n");
            break;
        }
        if (cruise_rpm <= 0.0f ||
            cruise_rpm > APP_PLANNER_MAX_RPM) {
            (void)printf("[POS] rejected: speed %.0f out of range\r\n",
                (double)cruise_rpm);
            break;
        }

        /* 濡傛灉褰撳墠涓嶆槸POSITION妯″紡, 鍏堝垏妯″紡 */
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

        /* 璇诲彇褰撳墠缂栫爜鍣ㄤ綅缃?4璺钩鍧? */
        int32_t enc[BSP_ENCODER_COUNT];
        (void)bsp_encoder_get_all_counts(enc);
        float enc_avg = 0.0f;
        for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
            enc_avg += (float)enc[i];
        }
        enc_avg /= (float)BSP_ENCODER_COUNT;

        /* 鍚姩浣嶇疆鎺у埗 */
        OSAL_CRITICAL_SECTION {
            app_posctrl_start_position(&ctx->posctrl,
                target_pulses, cruise_rpm, enc_avg);
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

        /* 鍙傛暟鏍￠獙 */
        if (!isfinite(target_angle) || !isfinite(cruise_rpm)) {
            (void)printf("[ANGLE] rejected: NaN/Inf\r\n");
            break;
        }
        if (cruise_rpm <= 0.0f ||
            cruise_rpm > APP_PLANNER_MAX_RPM) {
            (void)printf("[ANGLE] rejected: speed %.0f out of range\r\n",
                (double)cruise_rpm);
            break;
        }

        /* 濡傛灉褰撳墠涓嶆槸ANGLE妯″紡, 鍏堝垏妯″紡 */
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

        /* 璇诲彇褰撳墠yaw */
        float cur_yaw;
        OSAL_CRITICAL_SECTION {
            cur_yaw = ctx->imu.yaw;
        }

        /* 鍚姩瑙掑害鎺у埗 */
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
            PROJECT_VERSION_STRING, (unsigned long)PROJECT_PROTOCOL_VERSION,
            PROJECT_BOARD_NAME, PROJECT_MOTOR_DRIVER_NAME,
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
            (unsigned long)APP_RPM_OUTPUT_PERIOD_MS,
            (unsigned long)VOFA_TELEMETRY_CHANNEL_COUNT);
        break;

    case VOFA_CMD_STATUS_QUERY:
    {
        bool enabled, ff_enabled;
        int32_t rpm, output;
        float target, kp, ki, kd, ff_k, ff_b;
        app_ctrl_mode_t mode;
        OSAL_CRITICAL_SECTION {
            enabled = ctx->motor_enabled[mid];
            rpm = ctx->status.rpm[mid];
            output = ctx->status.output[mid];
            target = ctx->pid[mid].setpoint;
            kp = ctx->pid[mid].kp;
            ki = ctx->pid[mid].ki;
            kd = ctx->pid[mid].kd;
            ff_enabled = ctx->ff[mid].enabled;
            ff_k = ctx->ff[mid].k;
            ff_b = ctx->ff[mid].b;
            mode = ctx->posctrl.mode;
        }
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
