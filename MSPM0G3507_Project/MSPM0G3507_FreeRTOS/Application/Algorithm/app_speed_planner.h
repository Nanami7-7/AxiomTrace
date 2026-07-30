/**
 * @file    app_speed_planner.h
 * @brief   速度目标 Jerk 限制 S 型规划器（纯算法、无硬件和 RTOS 依赖）
 * @note    业务层只需要 init、update、reset 三个接口：
 *          1. init() 设置最大加速度和最大 Jerk；
 *          2. 周期调用 update()，输入最终目标 RPM，返回平滑目标 RPM；
 *          3. 急停、禁用、故障或切换控制模式时调用 reset()。
 *
 *          普通“目标改为 0”允许平滑减速；显式急停必须绕过本规划器，
 *          立即关闭电机输出后再 reset()，不能等待 S 型曲线结束。
 *
 *          为避免离散周期在目标附近来回摆动，最后一个周期允许将目标速度
 *          吸附到终值并清零内部加速度；该末端动作不会产生超过一个规划周期
 *          可达速度步进的目标跳变，但不参与严格 Jerk 连续性统计。
 */
#ifndef APP_SPEED_PLANNER_H
#define APP_SPEED_PLANNER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** 速度误差小于该值且加速度也足够小时，吸附到目标，避免零速附近抖动。 */
#ifndef APP_SPEED_PLANNER_SPEED_EPS_RPM
#define APP_SPEED_PLANNER_SPEED_EPS_RPM       (0.5f)
#endif

/** 判定规划完成的加速度阈值，单位 RPM/s。 */
#ifndef APP_SPEED_PLANNER_ACCEL_EPS_RPM_S
#define APP_SPEED_PLANNER_ACCEL_EPS_RPM_S     (1.0f)
#endif

typedef struct {
    float max_accel_rpm_s;  /**< 最大加速度绝对值，单位 RPM/s。 */
    float max_jerk_rpm_s2;  /**< 最大 Jerk 绝对值，单位 RPM/s^2。 */
    float speed_rpm;        /**< 当前规划速度。 */
    float accel_rpm_s;      /**< 当前规划加速度。 */
    bool valid;             /**< 参数和内部状态是否有效。 */
} app_speed_planner_t;

/**
 * @brief 初始化速度 S 型规划器。
 * @param planner           规划器对象。
 * @param max_accel_rpm_s   最大加速度，必须大于 0。
 * @param max_jerk_rpm_s2   最大 Jerk，必须大于 0。
 * @note  参数非法时 valid=false；调用者可自动回退到原线性斜坡。
 */
void app_speed_planner_init(app_speed_planner_t *planner,
                            float max_accel_rpm_s,
                            float max_jerk_rpm_s2);

/**
 * @brief 重置规划状态，但保留 init() 设置的限值。
 * @param planner            规划器对象。
 * @param current_speed_rpm  新起点速度；急停/禁用时传 0，
 *                           从其他控制模式平滑切回速度模式时可传实测 RPM。
 */
void app_speed_planner_reset(app_speed_planner_t *planner,
                             float current_speed_rpm);

/**
 * @brief 更新一次规划器并返回平滑后的速度目标。
 * @param planner          规划器对象。
 * @param target_speed_rpm 最终目标速度 RPM，可正可负，也可运行中改变。
 * @param dt_s             本规划器实际调用周期，单位秒。
 * @return 本周期平滑目标 RPM；输入或状态非法时返回 0 并置 valid=false。
 */
float app_speed_planner_update(app_speed_planner_t *planner,
                               float target_speed_rpm,
                               float dt_s);

/** 获取当前规划速度。 */
float app_speed_planner_get_speed(const app_speed_planner_t *planner);

/** 获取当前规划加速度。 */
float app_speed_planner_get_accel(const app_speed_planner_t *planner);

/** 参数和内部状态是否有效。 */
bool app_speed_planner_is_valid(const app_speed_planner_t *planner);

/** 当前速度和加速度是否已经稳定在目标附近。 */
bool app_speed_planner_is_settled(const app_speed_planner_t *planner,
                                  float target_speed_rpm);

#ifdef __cplusplus
}
#endif

#endif /* APP_SPEED_PLANNER_H */
