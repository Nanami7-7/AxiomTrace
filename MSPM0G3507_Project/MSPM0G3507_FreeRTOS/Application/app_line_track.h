/**
 * @file    app_line_track.h
 * @brief   四/五路红外循迹控制接口。
 *
 * 通道数量由 BSP_IR_CHANNEL_COUNT（见 bsp_ir.h）切换：4=四路板，5=五路板。
 * 五路板物理尺寸：中间三路跨度 48.9mm，总宽 96.5mm。算法按实际位置计算偏差，
 * 不再把五路简单当成等间距的 {4,2,0,-2,-4}，这样更适合椭圆/圆弧赛道。
 *
 * 使用方式很简单：
 * 1. 初始化阶段调用 app_line_track_init()；
 * 2. 需要运行时调用 app_line_track_start()；
 * 3. 在固定周期任务中调用 app_line_track_step()；
 * 4. 停止时调用 app_line_track_stop()。
 *
 * app_line_track_update() 仍保留为纯算法接口，方便单独测试或移植到其他任务。
 * app_line_track_step() 只读取红外并计算左右轮目标RPM，不直接驱动电机。
 * 控制任务把目标RPM交给现有增量式速度PI，再统一完成限幅、过流保护和电机输出。
 */
#ifndef APP_LINE_TRACK_H
#define APP_LINE_TRACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "bsp_ir.h"

/*
 * 物理安装顺序：左到右对应 BSP 红外通道索引。
 * 五路板默认 CH1~CH5 从左到右；若实车安装方向不同，只需调整本映射顺序，
 * 不必改动算法。注意：映射错误会导致转向方向相反，实车前务必逐路核对。
 */
#ifndef LINE_TRACK_CH_MAP
#if (BSP_IR_CHANNEL_COUNT == 4U)
#define LINE_TRACK_CH_MAP              { 0U, 1U, 2U, 3U }
#else
#define LINE_TRACK_CH_MAP              { 0U, 1U, 2U, 3U, 4U }
#endif
#endif

/* 循迹外环参数，单位均为RPM；正转向量表示左转。 */
#ifndef LINE_TRACK_TURN90_ANGLE
#define LINE_TRACK_TURN90_ANGLE         (85.0f)
#endif
#ifndef LINE_TRACK_TURN_MAX_ANGLE
#define LINE_TRACK_TURN_MAX_ANGLE       (75.0f)
#endif
#ifndef LINE_TRACK_TURN_MID_ANGLE
//#define LINE_TRACK_TURN_MID_ANGLE       (45.0f)
#define LINE_TRACK_TURN_MID_ANGLE       (25.0f)
#endif
#ifndef LINE_TRACK_TURN_MIN_ANGLE
//#define LINE_TRACK_TURN_MIN_ANGLE       (24.0f)
#define LINE_TRACK_TURN_MIN_ANGLE       (8.0f)
#endif

/*
 * 简单位置式PD的D项：使用相邻红外采样的误差差值，不除以2ms。
 * 首次闭环调试保持0，只调P；车辆出现左右摆动后再从1~2逐步增加。
 */
#ifndef LINE_TRACK_KD_RPM_PER_STEP
#define LINE_TRACK_KD_RPM_PER_STEP      (0.0f)
#endif

/*
 * 速度参数，仍沿用原循迹算法的 mm/s 等效单位。
 * 赛题中心线一圈约6.14m：20s项目平均速度至少0.307m/s，30s项目至少0.205m/s。
 * 默认30用于五路红外首圈低速验证；确认方向和转向稳定后，再逐步提高并标定。
 */
#ifndef LINE_TRACK_BASE_SPEED
#define LINE_TRACK_BASE_SPEED           (30.0f)
#endif
#ifndef LINE_TRACK_FORWARD_LIMIT
#define LINE_TRACK_FORWARD_LIMIT        (70.0f)
#endif

/*
 * 首次低速调试禁止内侧轮反转，避免圆弧上因差速过大产生机械冲击。
 * 若后续赛道确实需要原地转向，再改为1；普通椭圆/圆弧赛道保持0即可。
 */
#ifndef LINE_TRACK_ALLOW_REVERSE
#define LINE_TRACK_ALLOW_REVERSE        (0U)
#endif

/*
 * 赛题赛道是半径 0.5m 的连续圆弧，不是直角弯，默认关闭直角弯保持。
 * 只有以后改到真正的直角赛道时，才把 LINE_TRACK_RIGHT_ANGLE_ENABLE 改为 1。
 */
#ifndef LINE_TRACK_RIGHT_ANGLE_ENABLE
#define LINE_TRACK_RIGHT_ANGLE_ENABLE   (0U)
#endif
#ifndef LINE_TRACK_TURN90_HOLD_CYCLES
#define LINE_TRACK_TURN90_HOLD_CYCLES   (9U)
#endif
#ifndef LINE_TRACK_TURN90_MAX_CYCLES
#define LINE_TRACK_TURN90_MAX_CYCLES    (200U)
#endif

/*
 * 数字红外的简单抗毛刺参数（四路、五路通用）。
 * 3次多数表决在当前2ms控制周期下通常增加约2~4ms确认延迟，不使用阻塞延时。
 */
#ifndef LINE_TRACK_FILTER_ENABLE
#define LINE_TRACK_FILTER_ENABLE        (1U)
#endif

/*
 * 圆弧速度下限比例。转向越大速度越低，但不会像旧算法那样降到0。
 * 赛题R=0.5m，保持连续前进通常比“弯中急停再加速”更平稳。
 */
#ifndef LINE_TRACK_CURVE_MIN_SPEED_RATIO
#define LINE_TRACK_CURVE_MIN_SPEED_RATIO (0.55f)
#endif

/* 丢线时降速并按最近方向搜索，持续丢线后由单圈管理器执行安全停车。 */
#ifndef LINE_TRACK_LOST_SPEED_RATIO
#define LINE_TRACK_LOST_SPEED_RATIO     (0.50f)
#endif

/*
 * 丢线后沿最后有效状态的方向搜索，并在该时间内从中等转向逐步增大到大转向。
 * 这样保留 tracefind 示例的“记住上次状态”思路，又避免刚丢线就猛打方向。
 */
#ifndef LINE_TRACK_LOST_SEARCH_RAMP_MS
#define LINE_TRACK_LOST_SEARCH_RAMP_MS  (80U)
#endif

/*
 * 直行双轮同步校正：只在红外基本居中时，根据左右编码器RPM差做一个很小的P修正。
 * 不加积分，也不替换现有速度环，避免130RPM以下的低速量化抖动被再次放大。
 * correction > 0 表示左轮实测更快，因此减小左命令、增大右命令。
 */
#ifndef LINE_TRACK_STRAIGHT_SYNC_ENABLE
#define LINE_TRACK_STRAIGHT_SYNC_ENABLE          (0U)
#endif
#ifndef LINE_TRACK_STRAIGHT_SYNC_KP
#define LINE_TRACK_STRAIGHT_SYNC_KP              (0.30f)
#endif
#ifndef LINE_TRACK_STRAIGHT_SYNC_MAX_RPM
#define LINE_TRACK_STRAIGHT_SYNC_MAX_RPM     (20)
#endif
#ifndef LINE_TRACK_STRAIGHT_SYNC_MIN_RPM
#define LINE_TRACK_STRAIGHT_SYNC_MIN_RPM         (20)
#endif
#ifndef LINE_TRACK_STRAIGHT_SYNC_LINE_ERROR_MAX
#define LINE_TRACK_STRAIGHT_SYNC_LINE_ERROR_MAX  (0.25f)
#endif

/*
 * 起步防跑偏：启动后可选地缓慢增加基础RPM，并临时加强左右轮RPM同步。
 * 一侧先动、另一侧仍接近0时，会主动压低快轮并抬高慢轮。
 */
#ifndef LINE_TRACK_STARTUP_ASSIST_ENABLE
#define LINE_TRACK_STARTUP_ASSIST_ENABLE         (0U)
#endif
#ifndef LINE_TRACK_STARTUP_RAMP_MS
#define LINE_TRACK_STARTUP_RAMP_MS               (450U)
#endif
#ifndef LINE_TRACK_STARTUP_BEGIN_RATIO
#define LINE_TRACK_STARTUP_BEGIN_RATIO           (0.55f)
#endif
#ifndef LINE_TRACK_STARTUP_SYNC_WINDOW_MS
#define LINE_TRACK_STARTUP_SYNC_WINDOW_MS        (650U)
#endif
#ifndef LINE_TRACK_STARTUP_SYNC_KP
#define LINE_TRACK_STARTUP_SYNC_KP               (0.85f)
#endif
#ifndef LINE_TRACK_STARTUP_SYNC_MAX_RPM
#define LINE_TRACK_STARTUP_SYNC_MAX_RPM      (45)
#endif
#ifndef LINE_TRACK_STARTUP_MOVING_RPM
#define LINE_TRACK_STARTUP_MOVING_RPM            (25)
#endif
#ifndef LINE_TRACK_STARTUP_STALLED_RPM
#define LINE_TRACK_STARTUP_STALLED_RPM           (8)
#endif
#ifndef LINE_TRACK_STARTUP_MISMATCH_RPM
#define LINE_TRACK_STARTUP_MISMATCH_RPM      (32)
#endif

/*
 * 循迹直线段IMU修正总开关。只在红外连续居中时锁定当前yaw；进入弯道、
 * 丢线或IMU超时会立即解除，不会用旧航向阻止车辆正常转弯。
 */
#ifndef LINE_TRACK_IMU_ASSIST_ENABLE
#define LINE_TRACK_IMU_ASSIST_ENABLE             (0U)
#endif
#ifndef LINE_TRACK_IMU_MAX_AGE_MS
#define LINE_TRACK_IMU_MAX_AGE_MS                (60U)
#endif
#ifndef LINE_TRACK_IMU_STRAIGHT_CONFIRM_CYCLES
#define LINE_TRACK_IMU_STRAIGHT_CONFIRM_CYCLES   (3U)
#endif
/* 若修正方向相反，只把1.0f改为-1.0f。 */
#ifndef LINE_TRACK_IMU_YAW_SIGN
#define LINE_TRACK_IMU_YAW_SIGN                  (1.0f)
#endif
#ifndef LINE_TRACK_IMU_YAW_KP_RPM_PER_DEG
#define LINE_TRACK_IMU_YAW_KP_RPM_PER_DEG    (4.0f)
#endif
#ifndef LINE_TRACK_IMU_GYRO_KD_RPM_PER_DPS
#define LINE_TRACK_IMU_GYRO_KD_RPM_PER_DPS   (1.30f)
#endif
#ifndef LINE_TRACK_IMU_GYRO_FILTER_ALPHA
#define LINE_TRACK_IMU_GYRO_FILTER_ALPHA         (0.30f)
#endif
#ifndef LINE_TRACK_IMU_TRIM_MAX_RPM
#define LINE_TRACK_IMU_TRIM_MAX_RPM          (45.0f)
#endif
#ifndef LINE_TRACK_IMU_TRIM_SLEW_RPM_PER_SAMPLE
#define LINE_TRACK_IMU_TRIM_SLEW_RPM_PER_SAMPLE      (50.0f)
#endif
#ifndef LINE_TRACK_IMU_HEADING_ERROR_MAX_DEG
#define LINE_TRACK_IMU_HEADING_ERROR_MAX_DEG     (12.0f)
#endif
#ifndef LINE_TRACK_IMU_ACCEL_NORM_MIN_G
#define LINE_TRACK_IMU_ACCEL_NORM_MIN_G          (0.75f)
#endif
#ifndef LINE_TRACK_IMU_ACCEL_NORM_MAX_G
#define LINE_TRACK_IMU_ACCEL_NORM_MAX_G          (1.25f)
#endif

/*
 * 曲率模型总开关。
 * 0：完全保留当前非线性P/PD循迹，便于随时回退；
 * 1：启用“红外曲率 + 中心速度S型规划 + 陀螺仪角速度反馈”。
 * 首次实车前建议保持0，确认传感器方向、Ls和陀螺仪符号后再改为1。
 */
#ifndef LINE_TRACK_MODEL_CONTROL_ENABLE
#define LINE_TRACK_MODEL_CONTROL_ENABLE          (0U)
#endif
#ifndef LINE_TRACK_MODEL_SPEED_SCURVE_ENABLE
#define LINE_TRACK_MODEL_SPEED_SCURVE_ENABLE     (1U)
#endif
#ifndef LINE_TRACK_MODEL_GYRO_RATE_ENABLE
#define LINE_TRACK_MODEL_GYRO_RATE_ENABLE        (1U)
#endif
#ifndef LINE_TRACK_MODEL_DEBUG_ENABLE
#define LINE_TRACK_MODEL_DEBUG_ENABLE            (1U)
#endif

/* 红外传感器排到车辆有效旋转中心的前向距离，必须按实车测量后覆盖。 */
#ifndef LINE_TRACK_MODEL_SENSOR_FORWARD_M
#define LINE_TRACK_MODEL_SENSOR_FORWARD_M        (0.100f)
#endif

/* line_error每1.0对应的实际横向距离。五路板由中间相邻通道间距自动换算。 */
#ifndef LINE_TRACK_MODEL_ERROR_UNIT_M
#if (BSP_IR_CHANNEL_COUNT == 4U)
#define LINE_TRACK_MODEL_ERROR_UNIT_M             (0.015f)
#else
#define LINE_TRACK_MODEL_ERROR_UNIT_M             (LINE_TRACK_5CH_MIDDLE_SPAN_MM * 0.0005f)
#endif
#endif

/* 方向不对时只改符号宏，不要改公式：正曲率约定为左转。 */
#ifndef LINE_TRACK_MODEL_ERROR_SIGN
#define LINE_TRACK_MODEL_ERROR_SIGN              (1.0f)
#endif
#ifndef LINE_TRACK_MODEL_GYRO_SIGN
#define LINE_TRACK_MODEL_GYRO_SIGN               (1.0f)
#endif

/* 底盘几何参数。换车轮或底盘后只需要覆盖这两个宏。 */
#ifndef LINE_TRACK_MODEL_WHEEL_DIAMETER_M
#define LINE_TRACK_MODEL_WHEEL_DIAMETER_M         (0.060f)
#endif
#ifndef LINE_TRACK_MODEL_EFFECTIVE_WHEEL_BASE_M
#define LINE_TRACK_MODEL_EFFECTIVE_WHEEL_BASE_M  (0.190f)
#endif

/* 曲率和速度规划参数。最大侧向加速度是软限速，最小速度比例用于保持循迹能力。 */
#ifndef LINE_TRACK_MODEL_MAX_CURVATURE_M_INV
#define LINE_TRACK_MODEL_MAX_CURVATURE_M_INV     (8.0f)
#endif
#ifndef LINE_TRACK_MODEL_MAX_LATERAL_ACCEL_M_S2
#define LINE_TRACK_MODEL_MAX_LATERAL_ACCEL_M_S2  (0.80f)
#endif
#ifndef LINE_TRACK_MODEL_MIN_SPEED_RATIO
#define LINE_TRACK_MODEL_MIN_SPEED_RATIO          (0.45f)
#endif
#ifndef LINE_TRACK_MODEL_MAX_ACCEL_RPM_S
#define LINE_TRACK_MODEL_MAX_ACCEL_RPM_S          (800.0f)
#endif
#ifndef LINE_TRACK_MODEL_MAX_JERK_RPM_S2
#define LINE_TRACK_MODEL_MAX_JERK_RPM_S2          (12000.0f)
#endif

/* 陀螺仪角速度P反馈；IMU无效时自动退化为纯运动学前馈。 */
#ifndef LINE_TRACK_MODEL_GYRO_FILTER_ALPHA
#define LINE_TRACK_MODEL_GYRO_FILTER_ALPHA        (0.35f)
#endif
#ifndef LINE_TRACK_MODEL_YAW_RATE_KP_RPM_PER_RAD_S
#define LINE_TRACK_MODEL_YAW_RATE_KP_RPM_PER_RAD_S (12.0f)
#endif
#ifndef LINE_TRACK_MODEL_YAW_RATE_FB_MAX_RPM
#define LINE_TRACK_MODEL_YAW_RATE_FB_MAX_RPM      (30.0f)
#endif

/* 转向差速保持快速通道，不使用普通四轮S型规划，仅限制极端单周期跳变。 */
#ifndef LINE_TRACK_MODEL_TURN_SLEW_RPM_PER_S
#define LINE_TRACK_MODEL_TURN_SLEW_RPM_PER_S      (6000.0f)
#endif

#if ((LINE_TRACK_MODEL_CONTROL_ENABLE != 0U) && \
     (LINE_TRACK_MODEL_CONTROL_ENABLE != 1U))
#error "LINE_TRACK_MODEL_CONTROL_ENABLE must be 0 or 1"
#endif
#if ((LINE_TRACK_MODEL_SPEED_SCURVE_ENABLE != 0U) && \
     (LINE_TRACK_MODEL_SPEED_SCURVE_ENABLE != 1U))
#error "LINE_TRACK_MODEL_SPEED_SCURVE_ENABLE must be 0 or 1"
#endif
#if ((LINE_TRACK_MODEL_GYRO_RATE_ENABLE != 0U) && \
     (LINE_TRACK_MODEL_GYRO_RATE_ENABLE != 1U))
#error "LINE_TRACK_MODEL_GYRO_RATE_ENABLE must be 0 or 1"
#endif
#if ((LINE_TRACK_MODEL_DEBUG_ENABLE != 0U) && \
     (LINE_TRACK_MODEL_DEBUG_ENABLE != 1U))
#error "LINE_TRACK_MODEL_DEBUG_ENABLE must be 0 or 1"
#endif

/* 0表示不限制旧循迹算法的纠偏速度；模型路径使用独立的每秒速率限制。 */
#ifndef LINE_TRACK_TURN_SLEW_STEP
#define LINE_TRACK_TURN_SLEW_STEP       (0.0f)
#endif

/*
 * 单圈自动停车参数。
 * 当前调试阶段关闭自动停车，让循迹持续运行；仍保留持续丢线停车保护。
 * 后续需要跑一圈自动停车时，将 LINE_TRACK_AUTO_STOP_ENABLE 改为1。
 */
#ifndef LINE_TRACK_AUTO_STOP_ENABLE
#define LINE_TRACK_AUTO_STOP_ENABLE         (0U)
#endif
#ifndef LINE_TRACK_LAP_ARM_DISTANCE_M
#define LINE_TRACK_LAP_ARM_DISTANCE_M        (4.50f)
#endif
#ifndef LINE_TRACK_START_LEAVE_CONFIRM_MS
#define LINE_TRACK_START_LEAVE_CONFIRM_MS    (50U)
#endif
#ifndef LINE_TRACK_FINISH_CONFIRM_MS
#define LINE_TRACK_FINISH_CONFIRM_MS         (10U)
#endif
#ifndef LINE_TRACK_LOST_STOP_MS
#define LINE_TRACK_LOST_STOP_MS              (300U)
#endif
#ifndef LINE_TRACK_MAX_RUN_MS
#define LINE_TRACK_MAX_RUN_MS                (35000U)
#endif
/*
 * 检测到终点横线后继续前进的补偿距离，单位 m。
 * 默认 0 表示立即刹车；实车根据“传感器到停车基准点距离－制动滑行距离”标定。
 */
#ifndef LINE_TRACK_FINISH_ADVANCE_M
#define LINE_TRACK_FINISH_ADVANCE_M          (0.0f)
#endif

/* A到B为1.5m直线段，仅用于串口打印通过B点的估算时间，不参与转向控制。 */
#ifndef LINE_TRACK_POINT_B_DISTANCE_M
#define LINE_TRACK_POINT_B_DISTANCE_M        (1.50f)
#endif

/*
 * 黑线掩码位宽与横线（启停线）识别掩码。
 * 四路：bit3=最左，全黑(0x0F)视为横线。
 * 五路：bit4=最左，0x0E 表示中间三路同时检测到黑线。
 * 中间三路跨度为48.9mm、总宽为96.5mm时，不要求外侧两路同时变黑，
 * 这样车辆轻微偏置或横线宽度变化时仍能稳定识别起点/终点。
 * 如果实际赛道标记不同，可以在工程配置中覆盖此宏，不改算法代码。
 */
#if (BSP_IR_CHANNEL_COUNT == 4U)
#define LINE_TRACK_MASK_ALL                (0x0FU)
#ifndef LINE_TRACK_CROSS_BLACK_MASK
#define LINE_TRACK_CROSS_BLACK_MASK        (0x0FU)
#endif
#else
#define LINE_TRACK_MASK_ALL                (0x1FU)
#ifndef LINE_TRACK_CROSS_BLACK_MASK
#define LINE_TRACK_CROSS_BLACK_MASK        (0x0EU)
#endif
#endif

/*
 * 五路传感器的物理尺寸（单位：mm）。
 *
 * 约定 CH1~CH5 从左到右：
 *   CH1 = +96.5/2 = +48.25mm
 *   CH2 = +48.9/2 = +24.45mm
 *   CH3 = 0mm
 *   CH4 = -24.45mm
 *   CH5 = -48.25mm
 *
 * 中间三路的跨度 48.9mm 用来做归一化，因此五路 line_error 大约落在
 * -1.97~+1.97，而不是旧算法的 -4~+4。这样小弯、圆弧和大弯之间的变化
 * 更连续，且改动设备尺寸时只需要改下面两个宏。
 */
#ifndef LINE_TRACK_5CH_MIDDLE_SPAN_MM
#define LINE_TRACK_5CH_MIDDLE_SPAN_MM     (48.9f)
#endif
#ifndef LINE_TRACK_5CH_TOTAL_WIDTH_MM
#define LINE_TRACK_5CH_TOTAL_WIDTH_MM      (96.5f)
#endif

/* 五路加权偏差权重（左到右），由赛题给出的实际尺寸自动换算。
 * 如果换了传感器安装尺寸，只修改上面的两个尺寸宏；如果只想快速试车，
 * 也可以直接覆盖 LINE_TRACK_WEIGHTS_5CH。 */
#ifndef LINE_TRACK_WEIGHTS_5CH
#define LINE_TRACK_WEIGHTS_5CH \
    { (LINE_TRACK_5CH_TOTAL_WIDTH_MM / LINE_TRACK_5CH_MIDDLE_SPAN_MM), \
      1.0f, 0.0f, -1.0f, \
      -(LINE_TRACK_5CH_TOTAL_WIDTH_MM / LINE_TRACK_5CH_MIDDLE_SPAN_MM) }
#endif

#if (BSP_IR_CHANNEL_COUNT == 4U)
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
#else
/*
 * 五路红外组合状态，bit4=最左，bit0=最右；0=黑线，1=白色。
 * CROSS 由“中间三路全黑”判据置位(值仍为 0)，LOST 为全白(0x1F)。
 * 直角弯相关状态仅在 LINE_TRACK_RIGHT_ANGLE_ENABLE=1 时使用，五路板默认关闭，
 * 其取值为占位定义，不影响默认圆弧循迹行为。
 */
typedef enum {
    LINE_TRACK_STATE_CROSS        = 0,      /* 中间三路全黑=启停横线 */
    LINE_TRACK_STATE_STRAIGHT     = 0x1B,   /* 仅中间路见线 (bit2黑) */
    LINE_TRACK_STATE_LEFT_SMALL   = 0x17,   /* 左偏一路 (bit3黑) */
    LINE_TRACK_STATE_RIGHT_SMALL  = 0x1D,   /* 右偏一路 (bit1黑) */
    LINE_TRACK_STATE_LEFT_BIG     = 0x0F,   /* 左偏两路 (bit4黑) */
    LINE_TRACK_STATE_RIGHT_BIG    = 0x1E,   /* 右偏两路 (bit0黑) */
    LINE_TRACK_STATE_LOST         = 0x1F,   /* 全白=丢线 */
    /* 直角弯占位状态（默认不启用）。 */
    LINE_TRACK_STATE_LEFT_90_A    = 0x07,
    LINE_TRACK_STATE_LEFT_90_B    = 0x03,
    LINE_TRACK_STATE_RIGHT_90_A   = 0x1C,
    LINE_TRACK_STATE_RIGHT_90_B   = 0x18,
} line_track_state_t;
#endif

/** 一次算法更新的结果。 */
typedef struct {
    float left_target_rpm;      /* 左轮最终目标RPM，直接交给现有速度PI。 */
    float right_target_rpm;     /* 右轮最终目标RPM，直接交给现有速度PI。 */
    float base_target_rpm;      /* 曲线降速和可选起步处理后的基础RPM。 */
    float turn_diff_rpm;        /* 最终差速量：(右目标-左目标)/2。 */
    float sync_correction_rpm;  /* 可选双轮同步修正，默认关闭。 */
    float imu_correction_rpm;   /* 可选IMU直线修正，默认关闭。 */
    uint8_t sensor_bits;        /* 兼容状态字，最高bit=最左；0=黑线，1=白色。 */
    uint8_t black_mask;         /* 滤波后的黑线掩码，最高bit=最左；1=黑线。 */
    uint8_t ir_raw[BSP_IR_CHANNEL_COUNT]; /* 原始BSP读数：1=黑线，0=白色。 */
    float line_error;           /* 加权偏差：正值表示线在车体左侧。四路约-3~+3，五路约-1.97~+1.97。 */
    int current_state;          /* 当前兼容状态值；CROSS=0，LOST=15(四路)/31(五路)。 */
    int last_valid_state;       /* 最近一次非丢线状态，供丢线恢复和调试使用。 */
    uint16_t lost_cycles;       /* 连续丢线周期数，当前控制周期为2ms。 */
    bool turn90_active;         /* 直角弯兼容状态是否正在保持。 */

    /* IMU和旧直线辅助调试量。 */
    float imu_yaw_deg;                    /* 原始偏航角，单位度。 */
    float imu_heading_ref_deg;            /* 直线航向锁定参考角，单位度。 */
    float imu_heading_error_deg;          /* 参考角减当前角，单位度。 */
    float imu_gyro_z_dps;                 /* 原始Z轴角速度，单位度/秒。 */
    float imu_gyro_filtered_dps;          /* 旧直线辅助使用的滤波角速度。 */
    float imu_accel_norm_g;               /* 三轴加速度模长，单位g。 */
    uint32_t imu_age_ms;                  /* 最新IMU样本年龄。 */
    uint16_t imu_straight_cycles;         /* 连续满足直线条件的周期数。 */
    bool imu_valid;                       /* IMU样本有限且未超时。 */
    bool imu_heading_locked;              /* 旧直线航向参考是否已锁定。 */

    /* 曲率模型调试量；总开关关闭时保持为0，便于统一日志格式。 */
    float line_error_m;                   /* 红外横向偏差，单位m，正值表示线在左侧。 */
    float curvature_raw_m_inv;            /* Pure Pursuit原始曲率，单位1/m。 */
    float curvature_m_inv;                /* 限幅后的目标曲率，单位1/m。 */
    float base_request_rpm;               /* 曲率限速后的中心速度请求。 */
    float base_planned_rpm;               /* S型规划后的中心速度。 */
    float base_accel_rpm_s;               /* 中心速度规划器当前加速度。 */
    float yaw_rate_ref_dps;               /* 根据v*kappa得到的目标角速度。 */
    float yaw_rate_measured_dps;          /* 符号修正和滤波后的实测角速度。 */
    float yaw_rate_error_dps;             /* 目标角速度减实测角速度。 */
    float turn_feedforward_rpm;           /* 底盘运动学差速前馈。 */
    float turn_feedback_rpm;              /* 陀螺仪角速度P反馈。 */
    bool model_enabled;                   /* 编译期总开关是否打开。 */
    bool model_valid;                     /* 本周期模型参数和计算是否有效。 */
    bool model_imu_rate_valid;            /* 本周期角速度反馈是否使用有效IMU。 */
} line_track_output_t;

/* 调试快照沿用输出结构，避免维护两套重复字段。 */
typedef line_track_output_t line_track_debug_t;

/** 运行时可调的算法参数。 */
typedef struct {
    float turn90_angle;
    float turn_max_angle;
    float turn_mid_angle;
    float turn_min_angle;
    float base_speed;
    float forward_limit;
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
 * 函数内部完成：读取红外 → 计算左右轮目标RPM。
 * 本函数不直接操作电机；返回true表示本周期已生成新目标，false表示未启动。
 */
bool app_line_track_step(void);

/** 清除上一次方向、丢线计数、编码器反馈和直角弯记忆。 */
void app_line_track_reset(void);

/**
 * 写入左右驱动轮的实测RPM，供下一循迹周期做直行同步校正。
 * 当前底盘左轮使用M4/D编码器，右轮使用M1/A编码器；调用者负责完成映射。
 */
void app_line_track_set_speed_feedback(int32_t left_rpm, int32_t right_rpm);

/** 写入最新IMU姿态，供循迹直线段抑制起步偏航。 */
void app_line_track_set_imu_feedback(float yaw_deg,
                                     float gyro_z_dps,
                                     float accel_x_g,
                                     float accel_y_g,
                                     float accel_z_g,
                                     uint32_t sample_timestamp_ms,
                                     uint32_t age_ms);

/** 执行一次读取和计算；out 不能为 NULL。该接口不直接驱动电机。 */
void app_line_track_update(line_track_output_t *out);

/** 获取最近一次输出，返回静态对象，不需要释放。 */
const line_track_output_t *app_line_track_get_output(void);

/**
 * 复制最近一次循迹调试快照。
 * 本函数只复制结构体，不格式化字符串、不发送串口；建议由控制任务复制后交给低优先级日志任务。
 * @return debug非空时返回true，否则返回false。
 */
bool app_line_track_get_debug(line_track_debug_t *debug);

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
