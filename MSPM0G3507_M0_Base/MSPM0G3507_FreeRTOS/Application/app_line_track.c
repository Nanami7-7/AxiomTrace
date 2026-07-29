/**
 * @file    app_line_track.c
 * @brief   四路红外循迹状态机实现（移植自 WHEELTEC C07A IRF 工程的 IR_Module.c）。
 *
 * @details
 * 移植要点：
 *   1. 传感器读取由直接 DL_GPIO_readPins 改为调用 BSP_IR_Read()。
 *   2. 电平翻转：BSP_IR 输出 1=黑线 0=白色，原算法用 0=黑线 1=白色，
 *      读取后按位取反使状态字编码与原工程一致。
 *   3. 通道顺序通过 LINE_TRACK_CH_MAP 宏配置，适应不同物理安装。
 *   4. 输出改为写入 line_track_output_t 结构体，不直接驱动电机。
 *   5. 所有可调参数使用运行时变量，支持在线修改。
 *   6. 直角弯记忆周期数改为可配置宏，适配不同调用频率。
 */
#include "app_line_track.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

/* ======================== 内部状态 ======================== */

static int s_last_state = 0;      /**< 上一次非丢线状态（用于丢线方向推断） */
static int s_turn_cnt = 0;        /**< 直角弯记忆计数器 */
static int s_saved_state = 0;     /**< 直角弯记忆保存的转向状态 */

/** 通道映射：物理左→右对应 BSP_IR 通道索引 */
static const uint8_t s_ch_map[4] = LINE_TRACK_CH_MAP;

/** 运行时可调参数 */
static line_track_params_t s_params;

/* ======================== 公共函数实现 ======================== */

void app_line_track_init(void)
{
    s_last_state  = 0;
    s_turn_cnt    = 0;
    s_saved_state = 0;
    app_line_track_restore_default_params();
}

void app_line_track_reset(void)
{
    s_last_state  = 0;
    s_turn_cnt    = 0;
    s_saved_state = 0;
}

void app_line_track_restore_default_params(void)
{
    s_params.turn90_angle   = LINE_TRACK_TURN90_ANGLE;
    s_params.turn_max_angle = LINE_TRACK_TURN_MAX_ANGLE;
    s_params.turn_mid_angle = LINE_TRACK_TURN_MID_ANGLE;
    s_params.turn_min_angle = LINE_TRACK_TURN_MIN_ANGLE;
    s_params.base_speed     = LINE_TRACK_BASE_SPEED;
    s_params.forward_limit  = LINE_TRACK_FORWARD_LIMIT;
}

line_track_params_t *app_line_track_get_params(void)
{
    return &s_params;
}

void app_line_track_update(line_track_output_t *out)
{
    uint8_t ir[4];
    uint8_t bit;
    int sensor_state;
    float turn_diff = 0.0f;
    float base_speed_mm = 0.0f;

    if (out == NULL) {
        return;
    }

    /* ---- 1. 读取 4 路红外传感器 ----
     * BSP_IR_Read 返回 1=黑线 0=白色。
     * 原算法状态字编码 0=黑线 1=白色，因此对每个通道取反。
     * s_ch_map 将物理左→右顺序映射到 BSP_IR 通道索引。
     */
    BSP_IR_Read(ir);
    (void)memcpy(out->ir_raw, ir, 4U);

    /* bit3 = 最左, bit0 = 最右，取反后 0=黑线 1=白色 */
    bit = (ir[s_ch_map[0]] == 0U) ? 1U : 0U;
    sensor_state  = (int)(bit << 3);
    bit = (ir[s_ch_map[1]] == 0U) ? 1U : 0U;
    sensor_state |= (int)(bit << 2);
    bit = (ir[s_ch_map[2]] == 0U) ? 1U : 0U;
    sensor_state |= (int)(bit << 1);
    bit = (ir[s_ch_map[3]] == 0U) ? 1U : 0U;
    sensor_state |= (int)bit;

    out->sensor_bits = (uint8_t)sensor_state;

    /* ---- 2. 直角弯记忆机制 ----
     * 检测到直角弯后，前 HOLD_CYCLES 个周期强制直行（冲过路口），
     * 之后恢复记忆的转向状态，直到检测到大弯或超过 MAX_CYCLES。
     */
    if ((sensor_state == LINE_TRACK_STATE_LEFT_90_A ||
         sensor_state == LINE_TRACK_STATE_RIGHT_90_A ||
         sensor_state == LINE_TRACK_STATE_LEFT_90_B ||
         sensor_state == LINE_TRACK_STATE_RIGHT_90_B) && s_turn_cnt == 0) {
        s_saved_state = sensor_state;
        s_turn_cnt = 1;
    }
    if (s_turn_cnt > 0) {
        if (s_turn_cnt < (int)LINE_TRACK_TURN90_HOLD_CYCLES) {
            sensor_state = LINE_TRACK_STATE_STRAIGHT;
        } else if (s_turn_cnt < (int)LINE_TRACK_TURN90_MAX_CYCLES &&
                   sensor_state != LINE_TRACK_STATE_LEFT_BIG &&
                   sensor_state != LINE_TRACK_STATE_RIGHT_BIG) {
            sensor_state = s_saved_state;
        } else {
            s_turn_cnt = 0;
            s_saved_state = 0;
        }
        if (s_turn_cnt > 0) {
            s_turn_cnt++;
        }
    }

    out->turn90_active = (s_turn_cnt > 0);

    /* ---- 3. 状态机：状态码 → 转向量 ----
     * 正值=左转（左轮减速，右轮加速）
     * 负值=右转（右轮减速，左轮加速）
     */
    switch (sensor_state) {
    case LINE_TRACK_STATE_CROSS:
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
    case LINE_TRACK_STATE_STRAIGHT:
        turn_diff = 0.0f;
        break;
    case LINE_TRACK_STATE_LOST:
        /* 丢线处理：按上一次状态推断转向方向 */
        if (s_last_state == LINE_TRACK_STATE_LEFT_SMALL) {
            turn_diff = s_params.turn_mid_angle;
        } else if (s_last_state == LINE_TRACK_STATE_RIGHT_SMALL) {
            turn_diff = -s_params.turn_mid_angle;
        } else if (s_last_state == LINE_TRACK_STATE_LEFT_BIG) {
            turn_diff = s_params.turn_max_angle;
        } else if (s_last_state == LINE_TRACK_STATE_RIGHT_BIG) {
            turn_diff = -s_params.turn_max_angle;
        }
        break;
    default:
        turn_diff = 0.0f;
        break;
    }

    /* 保存非丢线状态供下次丢线推断 */
    if (sensor_state != LINE_TRACK_STATE_LOST) {
        s_last_state = sensor_state;
    }

    /* ---- 4. 自适应基础速度 ----
     * 转向量越大，前进速度越慢；超过 ForwardLimit 时停止前进。
     */
    if (fabsf(turn_diff) < s_params.forward_limit) {
        base_speed_mm = s_params.base_speed -
                        (s_params.base_speed *
                         (fabsf(turn_diff) / s_params.forward_limit));
    } else {
        base_speed_mm = 0.0f;
    }

    /* ---- 5. 差速输出（单位 m/s） ----
     * 左轮 = 基础速度 - 转向量
     * 右轮 = 基础速度 + 转向量
     */
    out->left_target_speed  = 0.001f * (base_speed_mm - turn_diff);
    out->right_target_speed = 0.001f * (base_speed_mm + turn_diff);
    out->turn_diff          = turn_diff;
    out->base_speed_mm      = base_speed_mm;
    out->current_state      = sensor_state;
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
    default:                            return "UNK";
    }
}
