/**
 * @file    app_line_track.h
 * @brief   四路红外循迹状态机（移植自 WHEELTEC C07A IRF 工程的 IR_Module）。
 *
 * @details
 * 本模块将原 WHEELTEC 工程的 IRDM_line_inspection() 状态机算法移植为
 * 应用层组件，数据源改为调用 BSP_IR_Read()，输出左右轮目标速度到结构体，
 * 不直接驱动电机硬件，实现算法与硬件解耦。
 *
 * 电平适配：
 *   - bsp_ir.c 的 BSP_IR_LINE_ACTIVE_LOW=1 时输出 1=黑线 0=白色。
 *   - 原算法状态字编码为 0=黑线 1=白色（黑线对应低电平，直读 GPIO）。
 *   - 本模块在内部自动翻转：BSP_IR_Read 的 1(黑) → 状态字的 0，
 *     使状态机逻辑与原始工程完全一致，无需修改枚举定义。
 *
 * 调用频率说明：
 *   - 原工程在 200Hz 定时器中断中调用，直角弯前冲计数 175 周期约 875ms。
 *   - 本工程默认在 100ms 周期的菜单任务中调用（10Hz），需相应调小
 *     LINE_TRACK_TURN90_HOLD_CYCLES 等周期相关宏。
 *   - 若需要更高频率，可在独立 FreeRTOS 任务中周期调用 app_line_track_update()。
 */
#ifndef APP_LINE_TRACK_H
#define APP_LINE_TRACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "bsp_ir.h"

/* ======================== 用户可配置宏 ======================== */

/**
 * @brief 通道顺序：左到右的物理排列对应的 BSP_IR 通道索引。
 * @details
 *   原工程 dh1(最左,bit3) → dh2(bit2) → dh3(bit1) → dh4(最右,bit0)。
 *   修改此映射数组以匹配实际安装方向。
 *   默认假设 CH1=最左, CH2, CH3, CH4=最右。
 */
#ifndef LINE_TRACK_CH_MAP
#define LINE_TRACK_CH_MAP  { 0U, 1U, 2U, 3U }  /**< {最左, 左中, 右中, 最右} 对应 BSP_IR 通道号 */
#endif

/* ---- 转向量参数（单位与速度同量级，原工程为 mm/s 级别） ---- */

#ifndef LINE_TRACK_TURN90_ANGLE
#define LINE_TRACK_TURN90_ANGLE      (70.0f)   /**< 直角弯转向量 */
#endif

#ifndef LINE_TRACK_TURN_MAX_ANGLE
#define LINE_TRACK_TURN_MAX_ANGLE    (45.0f)   /**< 大弯转向量 */
#endif

#ifndef LINE_TRACK_TURN_MID_ANGLE
#define LINE_TRACK_TURN_MID_ANGLE    (20.0f)   /**< 中弯转向量（丢线恢复用） */
#endif

#ifndef LINE_TRACK_TURN_MIN_ANGLE
#define LINE_TRACK_TURN_MIN_ANGLE    (15.0f)   /**< 微调转向量 */
#endif

/* ---- 速度参数 ---- */

#ifndef LINE_TRACK_BASE_SPEED
#define LINE_TRACK_BASE_SPEED        (150.0f)  /**< 基础巡线速度 (mm/s) */
#endif

#ifndef LINE_TRACK_FORWARD_LIMIT
#define LINE_TRACK_FORWARD_LIMIT     (70.0f)   /**< 前进限速阈值，转向量超过此值时停止前进 */
#endif

/* ---- 直角弯记忆机制参数（周期数，与调用频率相关） ----
 * 原工程 200Hz：175 周期 ≈ 875ms，4000 周期 ≈ 20s。
 * 若在 10Hz（100ms）任务中调用，建议分别设为 9 和 200。
 */
#ifndef LINE_TRACK_TURN90_HOLD_CYCLES
#define LINE_TRACK_TURN90_HOLD_CYCLES   (9U)   /**< 检测到直角弯后强制直行的周期数 */
#endif

#ifndef LINE_TRACK_TURN90_MAX_CYCLES
#define LINE_TRACK_TURN90_MAX_CYCLES    (200U)  /**< 直角弯记忆最大持续周期数 */
#endif

/* ======================== 路况状态枚举 ======================== */
/**
 * @brief 4 路红外合成状态（0=黑线, 1=白色，与原工程一致）。
 * @note  bit3=最左传感器, bit0=最右传感器。
 */
typedef enum {
    LINE_TRACK_STATE_CROSS       = 0,   /**< 0000 十字路口（全黑） */
    LINE_TRACK_STATE_LEFT_90_A   = 1,   /**< 0001 左直角弯 A */
    LINE_TRACK_STATE_LEFT_90_B   = 3,   /**< 0011 左直角弯 B */
    LINE_TRACK_STATE_RIGHT_90_A  = 8,   /**< 1000 右直角弯 A */
    LINE_TRACK_STATE_RIGHT_90_B  = 12,  /**< 1100 右直角弯 B */
    LINE_TRACK_STATE_LEFT_BIG    = 7,   /**< 0111 左大弯 */
    LINE_TRACK_STATE_RIGHT_BIG   = 14,  /**< 1110 右大弯 */
    LINE_TRACK_STATE_LEFT_SMALL  = 11,  /**< 1011 左微调 */
    LINE_TRACK_STATE_RIGHT_SMALL = 13,  /**< 1101 右微调 */
    LINE_TRACK_STATE_STRAIGHT    = 9,   /**< 1001 直行 */
    LINE_TRACK_STATE_LOST        = 15   /**< 1111 丢线（全白） */
} line_track_state_t;

/* ======================== 输出结构体 ======================== */

/**
 * @brief 循迹算法单次更新的输出结果。
 */
typedef struct {
    float left_target_speed;    /**< 左轮目标速度 (m/s) */
    float right_target_speed;   /**< 右轮目标速度 (m/s) */
    float turn_diff;            /**< 转向量（正=左转，负=右转） */
    float base_speed_mm;        /**< 基础前进速度 (mm/s) */
    uint8_t sensor_bits;        /**< 4bit 状态字（0=黑,1=白，bit3=最左） */
    uint8_t ir_raw[4];          /**< BSP_IR 原始读取值（1=黑,0=白，索引=通道号） */
    int current_state;          /**< 当前路况状态码（对应 line_track_state_t） */
    bool turn90_active;         /**< 直角弯记忆机制是否正在生效 */
} line_track_output_t;

/* ======================== 可调参数结构体 ======================== */

/**
 * @brief 循迹可调参数集合，支持运行时修改（如蓝牙在线调参）。
 */
typedef struct {
    float turn90_angle;     /**< 直角弯转向量 */
    float turn_max_angle;   /**< 大弯转向量 */
    float turn_mid_angle;   /**< 中弯转向量（丢线恢复用） */
    float turn_min_angle;   /**< 微调转向量 */
    float base_speed;       /**< 基础巡线速度 (mm/s) */
    float forward_limit;    /**< 前进限速阈值 */
} line_track_params_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief 初始化循迹模块，将参数重置为宏默认值，清零内部状态。
 */
void app_line_track_init(void);

/**
 * @brief 重置内部状态机（last_state、直角弯计数器等）。
 * @note  在进入/退出循迹模式时调用，避免残留状态影响下次运行。
 */
void app_line_track_reset(void);

/**
 * @brief 执行一次循迹状态机更新。
 * @param out 输出结构体指针，不可为 NULL。
 * @note  调用频率影响直角弯记忆时序，参见 LINE_TRACK_TURN90_HOLD_CYCLES 说明。
 */
void app_line_track_update(line_track_output_t *out);

/**
 * @brief 获取当前可调参数的指针（可用于读取或在线修改）。
 * @return 参数结构体指针。
 */
line_track_params_t *app_line_track_get_params(void);

/**
 * @brief 将参数恢复为宏默认值。
 */
void app_line_track_restore_default_params(void);

/**
 * @brief 获取路况状态码对应的中文名称字符串。
 * @param state 状态码（line_track_state_t 枚举值）。
 * @return 名称字符串指针（静态存储，无需释放）。
 */
const char *app_line_track_state_name(int state);

#ifdef __cplusplus
}
#endif

#endif /* APP_LINE_TRACK_H */
