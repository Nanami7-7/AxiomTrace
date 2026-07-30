/**
 * @file    app_speed_planner.c
 * @brief   速度目标 Jerk 限制 S 型规划器实现
 */
#include "app_speed_planner.h"

#include <math.h>
#include <stddef.h>

static float speed_planner_abs(float value)
{
    return (value >= 0.0f) ? value : -value;
}

static float speed_planner_clamp(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float speed_planner_slew(float current, float target, float max_step)
{
    if (target > current + max_step) return current + max_step;
    if (target < current - max_step) return current - max_step;
    return target;
}

void app_speed_planner_init(app_speed_planner_t *planner,
                            float max_accel_rpm_s,
                            float max_jerk_rpm_s2)
{
    if (planner == NULL) return;

    planner->max_accel_rpm_s = max_accel_rpm_s;
    planner->max_jerk_rpm_s2 = max_jerk_rpm_s2;
    planner->speed_rpm = 0.0f;
    planner->accel_rpm_s = 0.0f;
    planner->valid = isfinite(max_accel_rpm_s) &&
                     isfinite(max_jerk_rpm_s2) &&
                     (max_accel_rpm_s > 0.0f) &&
                     (max_jerk_rpm_s2 > 0.0f);
}

void app_speed_planner_reset(app_speed_planner_t *planner,
                             float current_speed_rpm)
{
    if (planner == NULL) return;

    if (!isfinite(current_speed_rpm)) {
        current_speed_rpm = 0.0f;
        planner->valid = false;
    }

    planner->speed_rpm = current_speed_rpm;
    planner->accel_rpm_s = 0.0f;

    /* reset 不掩盖配置错误；配置错误必须重新调用 init 修正。 */
    if (!isfinite(planner->max_accel_rpm_s) ||
        !isfinite(planner->max_jerk_rpm_s2) ||
        (planner->max_accel_rpm_s <= 0.0f) ||
        (planner->max_jerk_rpm_s2 <= 0.0f)) {
        planner->valid = false;
    }
}

float app_speed_planner_update(app_speed_planner_t *planner,
                               float target_speed_rpm,
                               float dt_s)
{
    if ((planner == NULL) || !planner->valid ||
        !isfinite(target_speed_rpm) || !isfinite(dt_s) ||
        !isfinite(planner->speed_rpm) ||
        !isfinite(planner->accel_rpm_s) ||
        (dt_s <= 0.0f)) {
        if (planner != NULL) {
            planner->speed_rpm = 0.0f;
            planner->accel_rpm_s = 0.0f;
            planner->valid = false;
        }
        return 0.0f;
    }

    const float speed_error = target_speed_rpm - planner->speed_rpm;
    if ((speed_planner_abs(speed_error) <=
         APP_SPEED_PLANNER_SPEED_EPS_RPM) &&
        (speed_planner_abs(planner->accel_rpm_s) <=
         APP_SPEED_PLANNER_ACCEL_EPS_RPM_S)) {
        planner->speed_rpm = target_speed_rpm;
        planner->accel_rpm_s = 0.0f;
        return planner->speed_rpm;
    }

    /*
     * 若现在开始用最大反向 Jerk 把加速度收回到 0，最终将到达 stop_speed。
     * 比较 stop_speed 与目标即可决定本周期应增加还是减小加速度。
     * 该算法只依赖速度、加速度和 dt，目标运行中改变或反向时无需重启。
     */
    const float stop_speed = planner->speed_rpm +
        planner->accel_rpm_s * speed_planner_abs(planner->accel_rpm_s) /
        (2.0f * planner->max_jerk_rpm_s2);

    float desired_accel = 0.0f;
    if (target_speed_rpm > stop_speed) {
        desired_accel = planner->max_accel_rpm_s;
    } else if (target_speed_rpm < stop_speed) {
        desired_accel = -planner->max_accel_rpm_s;
    }

    const float old_speed = planner->speed_rpm;
    const float old_accel = planner->accel_rpm_s;
    const float max_accel_step = planner->max_jerk_rpm_s2 * dt_s;
    float new_accel = speed_planner_slew(
        old_accel, desired_accel, max_accel_step);
    new_accel = speed_planner_clamp(
        new_accel,
        -planner->max_accel_rpm_s,
         planner->max_accel_rpm_s);

    /* 梯形积分比直接使用 new_accel 更平滑。 */
    float new_speed = old_speed + 0.5f * (old_accel + new_accel) * dt_s;

    /*
     * 离散周期最后可能只剩很小的目标误差。禁止穿过目标后反复回摆；
     * 吸附只修正最后一个很小的速度步进，不用于普通急停。
     */
    const float old_error = target_speed_rpm - old_speed;
    const float new_error = target_speed_rpm - new_speed;
    if ((old_error != 0.0f) && ((old_error * new_error) <= 0.0f)) {
        new_speed = target_speed_rpm;
        new_accel = 0.0f;
    }

    if (!isfinite(new_speed) || !isfinite(new_accel)) {
        planner->speed_rpm = 0.0f;
        planner->accel_rpm_s = 0.0f;
        planner->valid = false;
        return 0.0f;
    }

    planner->speed_rpm = new_speed;
    planner->accel_rpm_s = new_accel;
    return planner->speed_rpm;
}

float app_speed_planner_get_speed(const app_speed_planner_t *planner)
{
    return (planner != NULL) ? planner->speed_rpm : 0.0f;
}

float app_speed_planner_get_accel(const app_speed_planner_t *planner)
{
    return (planner != NULL) ? planner->accel_rpm_s : 0.0f;
}

bool app_speed_planner_is_valid(const app_speed_planner_t *planner)
{
    return (planner != NULL) && planner->valid;
}

bool app_speed_planner_is_settled(const app_speed_planner_t *planner,
                                  float target_speed_rpm)
{
    if ((planner == NULL) || !planner->valid ||
        !isfinite(target_speed_rpm)) {
        return false;
    }

    return (speed_planner_abs(target_speed_rpm - planner->speed_rpm) <=
            APP_SPEED_PLANNER_SPEED_EPS_RPM) &&
           (speed_planner_abs(planner->accel_rpm_s) <=
            APP_SPEED_PLANNER_ACCEL_EPS_RPM_S);
}
