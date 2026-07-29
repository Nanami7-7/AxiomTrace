/**
 * @file    app_line_track.c
 * @brief   四路红外循迹控制实现。
 *
 * 设计原则：
 * 1. app_line_track_update() 只负责读取传感器和计算结果，便于单元测试；
 * 2. app_line_track_step() 是实际控制任务使用的一站式接口，会复用
 *    bsp_motor_set_speed() 输出四路电机命令；
 * 3. 不在本模块中增加看门狗、任务或阻塞等待；
 * 4. 上电默认停止，必须显式调用 app_line_track_start() 才会输出电机命令。
 */
#include "app_line_track.h"
#include "bsp_motor.h"
#include "project_config.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

/* 工厂测试固件不启用循迹算法，避免引入额外浮点库代码。 */
#if defined(PRJ_DRV8870_FACTORY_TEST_ENABLE) && \
    (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0)
#define APP_LINE_TRACK_BUILD_ENABLE 0
#else
#define APP_LINE_TRACK_BUILD_ENABLE 1
#endif

/* 如果工程没有定义电机通道掩码，默认允许四路输出。 */
#ifndef PRJ_MOTION_MOTOR_ACTIVE_MASK
#define PRJ_MOTION_MOTOR_ACTIVE_MASK   (0x0FU)
#endif

#if defined(PRJ_MOTOR_COMMAND_MAX)
#define APP_LINE_TRACK_COMMAND_MAX \
    ((int32_t)(PRJ_MOTOR_COMMAND_MAX))
#else
#define APP_LINE_TRACK_COMMAND_MAX \
    ((int32_t)(LINE_TRACK_COMMAND_MAX))
#endif

static volatile bool s_running;
static line_track_output_t s_last_output;

#if APP_LINE_TRACK_BUILD_ENABLE

static int s_last_state;
static int s_turn_cnt;
static int s_saved_state;
static const uint8_t s_ch_map[BSP_IR_CHANNEL_COUNT] = LINE_TRACK_CH_MAP;
static line_track_params_t s_params;

/** 将浮点速度换算为统一有符号电机命令，并做限幅。 */
static int32_t line_track_speed_to_command(float speed_m_s)
{
    float scale = s_params.command_scale;
    float command_f;

    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    /* 1 m/s = 1000 mm/s；再乘整体可调缩放系数。 */
    command_f = speed_m_s * 1000.0f * scale;
    if (command_f >= (float)APP_LINE_TRACK_COMMAND_MAX) {
        return APP_LINE_TRACK_COMMAND_MAX;
    }
    if (command_f <= -(float)APP_LINE_TRACK_COMMAND_MAX) {
        return -APP_LINE_TRACK_COMMAND_MAX;
    }

    /* 直接截断即可，避免引入额外的四舍五入规则。 */
    return (int32_t)command_f;
}

/** 通过现有电机 BSP 输出一侧的命令。 */
static void line_track_set_motor(bsp_motor_id_t motor, int32_t command)
{
    if ((PRJ_MOTION_MOTOR_ACTIVE_MASK & (1UL << (uint32_t)motor)) != 0UL) {
        (void)bsp_motor_set_speed(motor, command);
    } else {
        /* 未开放的通道始终保持刹车，避免备用接口误动作。 */
        (void)bsp_motor_stop(motor, BSP_MOTOR_MODE_BRAKE);
    }
}

/** 将左右轮命令映射到 M1~M4：A/B 为右轮，C/D 为左轮。 */
static void line_track_apply_output(const line_track_output_t *out)
{
    if (out == NULL) {
        return;
    }

    line_track_set_motor(BSP_MOTOR_A, out->right_command);
    line_track_set_motor(BSP_MOTOR_B, out->right_command);
    line_track_set_motor(BSP_MOTOR_C, out->left_command);
    line_track_set_motor(BSP_MOTOR_D, out->left_command);
}

void app_line_track_init(void)
{
    memset(&s_last_output, 0, sizeof(s_last_output));
    s_running = false;
    app_line_track_reset();
    app_line_track_restore_default_params();
}

void app_line_track_start(void)
{
    /* 启动不自动使能电机功率，只允许控制任务开始输出命令。 */
    app_line_track_reset();
    s_running = true;
}

void app_line_track_stop(void)
{
    s_running = false;
    app_line_track_reset();
}

bool app_line_track_is_running(void)
{
    return s_running;
}

bool app_line_track_step(void)
{
    if (!s_running) {
        return false;
    }

    app_line_track_update(&s_last_output);
    line_track_apply_output(&s_last_output);
    return true;
}

void app_line_track_stop_motors(void)
{
    bsp_motor_stop_all();
}

void app_line_track_reset(void)
{
    s_last_state = LINE_TRACK_STATE_CROSS;
    s_turn_cnt = 0;
    s_saved_state = LINE_TRACK_STATE_CROSS;
    memset(&s_last_output, 0, sizeof(s_last_output));
}

void app_line_track_restore_default_params(void)
{
    s_params.turn90_angle = LINE_TRACK_TURN90_ANGLE;
    s_params.turn_max_angle = LINE_TRACK_TURN_MAX_ANGLE;
    s_params.turn_mid_angle = LINE_TRACK_TURN_MID_ANGLE;
    s_params.turn_min_angle = LINE_TRACK_TURN_MIN_ANGLE;
    s_params.base_speed = LINE_TRACK_BASE_SPEED;
    s_params.forward_limit = LINE_TRACK_FORWARD_LIMIT;
    s_params.command_scale = LINE_TRACK_COMMAND_SCALE;
}

line_track_params_t *app_line_track_get_params(void)
{
    return &s_params;
}

void app_line_track_update(line_track_output_t *out)
{
    uint8_t ir[BSP_IR_CHANNEL_COUNT];
    uint8_t bit;
    int sensor_state;
    float turn_diff = 0.0f;
    float base_speed_mm = 0.0f;

    if (out == NULL) {
        return;
    }

    BSP_IR_Read(ir);
    (void)memcpy(out->ir_raw, ir, sizeof(out->ir_raw));

    /* 算法状态字沿用 Board B：0=黑线、1=白色。 */
    bit = (ir[s_ch_map[0]] == 0U) ? 1U : 0U;
    sensor_state = (int)(bit << 3);
    bit = (ir[s_ch_map[1]] == 0U) ? 1U : 0U;
    sensor_state |= (int)(bit << 2);
    bit = (ir[s_ch_map[2]] == 0U) ? 1U : 0U;
    sensor_state |= (int)(bit << 1);
    bit = (ir[s_ch_map[3]] == 0U) ? 1U : 0U;
    sensor_state |= (int)bit;
    out->sensor_bits = (uint8_t)sensor_state;

    /* 直角弯记忆：先短暂直行，再保持原转向。 */
    if ((sensor_state == LINE_TRACK_STATE_LEFT_90_A ||
         sensor_state == LINE_TRACK_STATE_RIGHT_90_A ||
         sensor_state == LINE_TRACK_STATE_LEFT_90_B ||
         sensor_state == LINE_TRACK_STATE_RIGHT_90_B) &&
        (s_turn_cnt == 0)) {
        s_saved_state = sensor_state;
        s_turn_cnt = 1;
    }

    if (s_turn_cnt > 0) {
        if (s_turn_cnt < (int)LINE_TRACK_TURN90_HOLD_CYCLES) {
            sensor_state = LINE_TRACK_STATE_STRAIGHT;
        } else if ((s_turn_cnt < (int)LINE_TRACK_TURN90_MAX_CYCLES) &&
                   (sensor_state != LINE_TRACK_STATE_LEFT_BIG) &&
                   (sensor_state != LINE_TRACK_STATE_RIGHT_BIG)) {
            sensor_state = s_saved_state;
        } else {
            s_turn_cnt = 0;
            s_saved_state = LINE_TRACK_STATE_CROSS;
        }

        if (s_turn_cnt > 0) {
            s_turn_cnt++;
        }
    }
    out->turn90_active = (s_turn_cnt > 0);

    /* 状态转为转向量：正值左转，负值右转。 */
    switch (sensor_state) {
    case LINE_TRACK_STATE_CROSS:
    case LINE_TRACK_STATE_STRAIGHT:
        turn_diff = 0.0f;
        break;
    case LINE_TRACK_STATE_LEFT_90_A:
    case LINE_TRACK_STATE_LEFT_90_B:
        turn_diff = s_params.turn90_angle;
        break;
    case LINE_TRACK_STATE_RIGHT_90_A:
    case LINE_TRACK_STATE_RIGHT_90_B:
        turn_diff = -s_params.turn90_angle;
        break;
    case LINE_TRACK_STATE_LEFT_BIG:
        turn_diff = s_params.turn_max_angle;
        break;
    case LINE_TRACK_STATE_RIGHT_BIG:
        turn_diff = -s_params.turn_max_angle;
        break;
    case LINE_TRACK_STATE_LEFT_SMALL:
        turn_diff = s_params.turn_min_angle;
        break;
    case LINE_TRACK_STATE_RIGHT_SMALL:
        turn_diff = -s_params.turn_min_angle;
        break;
    case LINE_TRACK_STATE_LOST:
        /* 丢线时沿用最近一次转向趋势。 */
        if ((s_last_state == LINE_TRACK_STATE_LEFT_SMALL) ||
            (s_last_state == LINE_TRACK_STATE_LEFT_BIG)) {
            turn_diff = (s_last_state == LINE_TRACK_STATE_LEFT_BIG) ?
                        s_params.turn_max_angle : s_params.turn_mid_angle;
        } else if ((s_last_state == LINE_TRACK_STATE_RIGHT_SMALL) ||
                   (s_last_state == LINE_TRACK_STATE_RIGHT_BIG)) {
            turn_diff = (s_last_state == LINE_TRACK_STATE_RIGHT_BIG) ?
                        -s_params.turn_max_angle : -s_params.turn_mid_angle;
        }
        break;
    default:
        turn_diff = 0.0f;
        break;
    }

    if (sensor_state != LINE_TRACK_STATE_LOST) {
        s_last_state = sensor_state;
    }

    if ((s_params.forward_limit > 0.0f) &&
        (fabsf(turn_diff) < s_params.forward_limit)) {
        base_speed_mm = s_params.base_speed -
                        (s_params.base_speed *
                         (fabsf(turn_diff) / s_params.forward_limit));
    } else {
        base_speed_mm = 0.0f;
    }

    out->left_target_speed = 0.001f * (base_speed_mm - turn_diff);
    out->right_target_speed = 0.001f * (base_speed_mm + turn_diff);
    out->left_command = line_track_speed_to_command(out->left_target_speed);
    out->right_command = line_track_speed_to_command(out->right_target_speed);
    out->turn_diff = turn_diff;
    out->base_speed_mm = base_speed_mm;
    out->current_state = sensor_state;
}

const line_track_output_t *app_line_track_get_output(void)
{
    return &s_last_output;
}

const char *app_line_track_state_name(int state)
{
    switch (state) {
    case LINE_TRACK_STATE_CROSS:       return "CROSS";
    case LINE_TRACK_STATE_LEFT_90_A:   return "L90A";
    case LINE_TRACK_STATE_LEFT_90_B:   return "L90B";
    case LINE_TRACK_STATE_RIGHT_90_A:  return "R90A";
    case LINE_TRACK_STATE_RIGHT_90_B:  return "R90B";
    case LINE_TRACK_STATE_LEFT_BIG:    return "L_BIG";
    case LINE_TRACK_STATE_RIGHT_BIG:   return "R_BIG";
    case LINE_TRACK_STATE_LEFT_SMALL:  return "L_MIN";
    case LINE_TRACK_STATE_RIGHT_SMALL: return "R_MIN";
    case LINE_TRACK_STATE_STRAIGHT:    return "STR";
    case LINE_TRACK_STATE_LOST:        return "LOST";
    default:                           return "UNK";
    }
}

#else

/* 未启用循迹时保留完整空实现，保证不同 Keil 目标可以共用头文件。 */
void app_line_track_init(void)
{
    memset(&s_last_output, 0, sizeof(s_last_output));
    s_running = false;
}

void app_line_track_start(void)
{
    s_running = false;
}

void app_line_track_stop(void)
{
    s_running = false;
}

bool app_line_track_is_running(void)
{
    return false;
}

bool app_line_track_step(void)
{
    return false;
}

void app_line_track_reset(void)
{
    memset(&s_last_output, 0, sizeof(s_last_output));
}

void app_line_track_update(line_track_output_t *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
}

const line_track_output_t *app_line_track_get_output(void)
{
    return &s_last_output;
}

void app_line_track_stop_motors(void)
{
}

line_track_params_t *app_line_track_get_params(void)
{
    return NULL;
}

void app_line_track_restore_default_params(void)
{
}

const char *app_line_track_state_name(int state)
{
    (void)state;
    return "OFF";
}

#endif /* APP_LINE_TRACK_BUILD_ENABLE */
