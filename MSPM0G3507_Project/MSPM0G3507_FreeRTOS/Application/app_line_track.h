/**
 * @file    app_line_track.h
 * @brief   四路红外循迹控制接口。
 *
 * 使用方式很简单：
 * 1. 初始化阶段调用 app_line_track_init()；
 * 2. 需要运行时调用 app_line_track_start()；
 * 3. 在固定周期任务中调用 app_line_track_step()；
 * 4. 停止时调用 app_line_track_stop()。
 *
 * app_line_track_update() 仍保留为纯算法接口，方便单独测试或移植到其他任务。
 * app_line_track_step() 会读取红外、计算左右轮命令，并通过 bsp_motor_set_speed()
 * 输出到四路电机。电机输出的真正映射集中在 app_line_track.c 中，业务层不需要
 * 再重复处理 M1~M4 的左右轮关系。
 */
#ifndef APP_LINE_TRACK_H
#define APP_LINE_TRACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "bsp_ir.h"

/* 物理安装顺序：左到右对应 BSP 红外通道索引。 */
#ifndef LINE_TRACK_CH_MAP
#define LINE_TRACK_CH_MAP              { 0U, 1U, 2U, 3U }
#endif

/* 转向参数，单位为 mm/s 等效转向量。 */
#ifndef LINE_TRACK_TURN90_ANGLE
#define LINE_TRACK_TURN90_ANGLE         (70.0f)
#endif
#ifndef LINE_TRACK_TURN_MAX_ANGLE
#define LINE_TRACK_TURN_MAX_ANGLE       (45.0f)
#endif
#ifndef LINE_TRACK_TURN_MID_ANGLE
#define LINE_TRACK_TURN_MID_ANGLE       (20.0f)
#endif
#ifndef LINE_TRACK_TURN_MIN_ANGLE
#define LINE_TRACK_TURN_MIN_ANGLE       (15.0f)
#endif

/* 速度参数，仍沿用原循迹算法的 mm/s 单位。 */
#ifndef LINE_TRACK_BASE_SPEED
#define LINE_TRACK_BASE_SPEED           (150.0f)
#endif
#ifndef LINE_TRACK_FORWARD_LIMIT
#define LINE_TRACK_FORWARD_LIMIT        (70.0f)
#endif

/*
 * 速度到电机业务命令的换算系数。
 * 默认 1.0：1 mm/s 等效为 1 个电机业务命令单位。
 * 如果更换驱动器或希望整体调速，只需修改这个宏，或运行时修改
 * line_track_params_t.command_scale，不需要改循迹状态机。
 */
#ifndef LINE_TRACK_COMMAND_SCALE
#define LINE_TRACK_COMMAND_SCALE        (1.0f)
#endif

/* 电机业务命令的默认最大绝对值；实际工程会优先使用 PRJ_MOTOR_COMMAND_MAX。 */
#ifndef LINE_TRACK_COMMAND_MAX
#define LINE_TRACK_COMMAND_MAX          (500)
#endif

/* 直角弯记忆周期数，取决于 app_line_track_update() 的调用频率。 */
#ifndef LINE_TRACK_TURN90_HOLD_CYCLES
#define LINE_TRACK_TURN90_HOLD_CYCLES   (9U)
#endif
#ifndef LINE_TRACK_TURN90_MAX_CYCLES
#define LINE_TRACK_TURN90_MAX_CYCLES    (200U)
#endif

/** 红外组合状态，bit3=最左，bit0=最右；0=黑线，1=白色。 */
typedef enum {
    LINE_TRACK_STATE_CROSS       = 0,
    LINE_TRACK_STATE_LEFT_90_A   = 1,
    LINE_TRACK_STATE_LEFT_90_B   = 3,
    LINE_TRACK_STATE_RIGHT_90_A  = 8,
    LINE_TRACK_STATE_RIGHT_90_B  = 12,
    LINE_TRACK_STATE_LEFT_BIG    = 7,
    LINE_TRACK_STATE_RIGHT_BIG   = 14,
    LINE_TRACK_STATE_LEFT_SMALL  = 11,
    LINE_TRACK_STATE_RIGHT_SMALL = 13,
    LINE_TRACK_STATE_STRAIGHT    = 9,
    LINE_TRACK_STATE_LOST        = 15,
} line_track_state_t;

/** 一次算法更新的结果。 */
typedef struct {
    float left_target_speed;    /* 左轮建议速度，单位 m/s。 */
    float right_target_speed;   /* 右轮建议速度，单位 m/s。 */
    float turn_diff;            /* 转向量：正值左转，负值右转。 */
    float base_speed_mm;        /* 基础速度，单位 mm/s。 */
    int32_t left_command;       /* 左轮统一电机命令，正值表示车体前进。 */
    int32_t right_command;      /* 右轮统一电机命令，正值表示车体前进。 */
    uint8_t sensor_bits;        /* 算法状态字，bit3=最左，bit0=最右。 */
    uint8_t ir_raw[BSP_IR_CHANNEL_COUNT]; /* 1=黑线，0=白色。 */
    int current_state;          /* 当前 line_track_state_t 状态。 */
    bool turn90_active;         /* 直角弯记忆是否有效。 */
} line_track_output_t;

/** 运行时可调的算法参数。 */
typedef struct {
    float turn90_angle;
    float turn_max_angle;
    float turn_mid_angle;
    float turn_min_angle;
    float base_speed;
    float forward_limit;
    float command_scale;        /* m/s 到电机命令的整体缩放系数。 */
} line_track_params_t;

/** 初始化参数和内部状态；上电默认不运行、不输出电机命令。 */
void app_line_track_init(void);

/** 启动循迹；启动时会清除上一次方向和直角弯记忆。 */
void app_line_track_start(void);

/** 停止循迹算法；控制任务应同时确保电机安全停止。 */
void app_line_track_stop(void);

/** 查询循迹算法是否处于运行状态。 */
bool app_line_track_is_running(void);

/**
 * 执行一次循迹控制周期。
 *
 * 函数内部完成：读取红外 → 算法计算 → 调用现有电机接口输出。
 * 返回 true 表示本周期确实输出了循迹命令，false 表示当前未启动。
 */
bool app_line_track_step(void);

/** 清除上一次方向和直角弯记忆。 */
void app_line_track_reset(void);

/** 执行一次读取和计算；out 不能为 NULL。该接口不直接驱动电机。 */
void app_line_track_update(line_track_output_t *out);

/** 获取最近一次输出，返回静态对象，不需要释放。 */
const line_track_output_t *app_line_track_get_output(void);

/** 通过现有 BSP 接口停止四路电机。 */
void app_line_track_stop_motors(void);

/** 获取运行时参数指针，业务代码可直接修改。 */
line_track_params_t *app_line_track_get_params(void);

/** 恢复头文件中的默认参数。 */
void app_line_track_restore_default_params(void);

/** 获取状态名称，返回静态字符串，不需要释放。 */
const char *app_line_track_state_name(int state);

#ifdef __cplusplus
}
#endif

#endif /* APP_LINE_TRACK_H */
