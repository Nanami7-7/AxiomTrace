/**
 * @file    app_planner.c
 * @brief   梯形速度规划器实现(纯算法,无硬件依赖)
 * @note    生成梯形速度曲线: 加速→巡航→减速→到位保持
 *
 *          算法核心:
 *          1. 加速段: speed += accel*dt, 达到 cruise_speed 切巡航
 *          2. 巡航段: 保持 cruise_speed, 检查剩余距离
 *          3. 减速段: speed -= accel*dt, 到位切 DONE
 *          4. 减速点计算: decel_dist = v²/(2a)
 *             (从当前速度减速到0所需距离, 无 sqrt)
 *
 *          短距离处理:
 *          若 加速段中 |remaining| <= decel_dist, 直接切减速段
 *          形成三角速度曲线(无巡航段)
 *
 *          过冲保护:
 *          位置超出 target 时, 强制钳位并进入 DONE
 */
#include "app_planner.h"
#include <stddef.h>   /* NULL */
#include <math.h>

/* ======================== 私有函数 ======================== */

/**
 * @brief  限幅函数
 */
static inline float clamp_f(float val, float min_val,
                              float max_val)
{
    if (val < min_val) { return min_val; }
    if (val > max_val) { return max_val; }
    return val;
}

/**
 * @brief  计算从当前速度减速到0所需距离
 * @note   公式: d = v² / (2a), 恒为正
 *         v=0 或 a<=0 时返回 0
 */
static inline float calc_decel_distance(float speed,
                                          float accel)
{
    if (accel <= 0.0f) { return 0.0f; }
    float v2 = speed * speed;
    return v2 / (2.0f * accel);
}

/* ======================== 公共函数实现 ======================== */

void app_planner_init(app_planner_t *p, float accel,
                      float out_min, float out_max)
{
    if (p == NULL) { return; }

    p->accel          = (accel > 0.0f) ? accel : 1.0f;
    p->out_min        = out_min;
    p->out_max        = out_max;
    p->done_threshold = 5.0f;  /* 默认5个单位(脉冲或度) */

    p->target         = 0.0f;
    p->cruise_speed   = 0.0f;
    p->state          = APP_PLANNER_STATE_IDLE;
    p->current_pos    = 0.0f;
    p->current_speed  = 0.0f;
}

void app_planner_start(app_planner_t *p, float target,
                       float cruise_speed)
{
    if (p == NULL) { return; }

    /* NaN/Inf 保护 */
    if (!isfinite(target) || !isfinite(cruise_speed)) {
        p->state         = APP_PLANNER_STATE_IDLE;
        p->current_pos   = 0.0f;
        p->current_speed = 0.0f;
        return;
    }

    /* 加速度与巡航速度必须有效；零速度无法完成非零位移。 */
    if (p->accel <= 0.0f) {
        p->state         = APP_PLANNER_STATE_IDLE;
        p->current_pos   = 0.0f;
        p->current_speed = 0.0f;
        return;
    }
    if (fabsf(cruise_speed) < 1e-6f) {
        p->state         = APP_PLANNER_STATE_IDLE;
        p->current_pos   = 0.0f;
        p->current_speed = 0.0f;
        return;
    }

    /* cruise_speed 符号跟随 target */
    if (target < 0.0f) {
        cruise_speed = -fabsf(cruise_speed);
    } else {
        cruise_speed = fabsf(cruise_speed);
    }

    /* 限幅 */
    cruise_speed = clamp_f(cruise_speed,
                           p->out_min, p->out_max);

    p->target         = target;
    p->cruise_speed   = cruise_speed;
    p->current_pos    = 0.0f;
    p->current_speed  = 0.0f;

    /* 距离太近, 直接到位 */
    if (fabsf(target) <= p->done_threshold) {
        p->current_pos = target;
        p->state       = APP_PLANNER_STATE_DONE;
    } else {
        p->state = APP_PLANNER_STATE_ACCEL;
    }
}

float app_planner_update(app_planner_t *p, float dt)
{
    if (p == NULL || !isfinite(dt) || dt <= 0.0f) {
        return 0.0f;
    }

    /* IDLE/DONE 状态: 速度为0, 不更新位置 */
    if (p->state == APP_PLANNER_STATE_IDLE ||
        p->state == APP_PLANNER_STATE_DONE) {
        p->current_speed = 0.0f;
        return 0.0f;
    }

    float remaining = p->target - p->current_pos;
    float abs_rem   = fabsf(remaining);
    /* 方向: +1 正向, -1 反向 */
    float dir       = (p->target >= 0.0f) ? 1.0f : -1.0f;

    switch (p->state) {
    case APP_PLANNER_STATE_ACCEL: {
        /* 加速 */
        p->current_speed += dir * p->accel * dt;

        /* 限幅到 cruise_speed */
        if (dir > 0.0f && p->current_speed >= p->cruise_speed) {
            p->current_speed = p->cruise_speed;
            p->state         = APP_PLANNER_STATE_CRUISE;
        } else if (dir < 0.0f &&
                   p->current_speed <= p->cruise_speed) {
            p->current_speed = p->cruise_speed;
            p->state         = APP_PLANNER_STATE_CRUISE;
        }

        /*
         * 检查是否需要提前减速(短距离无巡航段)
         * 即使刚切入巡航也立即检查, 形成三角曲线
         */
        float decel_dist = calc_decel_distance(
            p->current_speed, p->accel);
        if (abs_rem <= decel_dist) {
            p->state = APP_PLANNER_STATE_DECEL;
        }
        break;
    }

    case APP_PLANNER_STATE_CRUISE: {
        /* 保持巡航速度, 检查减速点 */
        p->current_speed = p->cruise_speed;

        float decel_dist = calc_decel_distance(
            p->current_speed, p->accel);
        if (abs_rem <= decel_dist) {
            p->state = APP_PLANNER_STATE_DECEL;
        }
        break;
    }

    case APP_PLANNER_STATE_DECEL: {
        /* 减速 */
        p->current_speed -= dir * p->accel * dt;

        /* 速度归零或过冲检查 */
        bool speed_zero = false;
        if (dir > 0.0f && p->current_speed <= 0.0f) {
            speed_zero = true;
        } else if (dir < 0.0f && p->current_speed >= 0.0f) {
            speed_zero = true;
        }

        /* 到位检查 */
        if (abs_rem <= p->done_threshold || speed_zero) {
            p->current_speed = 0.0f;
            p->current_pos   = p->target;
            p->state         = APP_PLANNER_STATE_DONE;
            break;
        }
        break;
    }

    default:
        p->current_speed = 0.0f;
        break;
    }

    /* 更新位置 */
    p->current_pos += p->current_speed * dt;

    /* 过冲保护: 位置不能超过 target */
    if (dir > 0.0f && p->current_pos > p->target) {
        p->current_pos   = p->target;
        p->current_speed = 0.0f;
        p->state         = APP_PLANNER_STATE_DONE;
    } else if (dir < 0.0f && p->current_pos < p->target) {
        p->current_pos   = p->target;
        p->current_speed = 0.0f;
        p->state         = APP_PLANNER_STATE_DONE;
    }

    /* NaN/Inf 保护 */
    if (!isfinite(p->current_speed) ||
        !isfinite(p->current_pos)) {
        p->current_speed = 0.0f;
        p->current_pos   = p->target;
        p->state         = APP_PLANNER_STATE_DONE;
    }

    return p->current_speed;
}

bool app_planner_is_done(const app_planner_t *p)
{
    if (p == NULL) { return true; }
    return (p->state == APP_PLANNER_STATE_DONE);
}

float app_planner_get_pos(const app_planner_t *p)
{
    if (p == NULL) { return 0.0f; }
    return p->current_pos;
}

float app_planner_get_speed(const app_planner_t *p)
{
    if (p == NULL) { return 0.0f; }
    return p->current_speed;
}

app_planner_state_t app_planner_get_state(const app_planner_t *p)
{
    if (p == NULL) { return APP_PLANNER_STATE_IDLE; }
    return p->state;
}

void app_planner_set_done_threshold(app_planner_t *p,
                                     float threshold)
{
    if (p == NULL) { return; }
    if (threshold > 0.0f && isfinite(threshold)) {
        p->done_threshold = threshold;
    }
}

void app_planner_reset(app_planner_t *p)
{
    if (p == NULL) { return; }
    p->state         = APP_PLANNER_STATE_IDLE;
    p->current_pos   = 0.0f;
    p->current_speed = 0.0f;
    p->target        = 0.0f;
    p->cruise_speed  = 0.0f;
}

void app_planner_abort(app_planner_t *p)
{
    if (p == NULL) { return; }
    p->current_speed = 0.0f;
    p->state         = APP_PLANNER_STATE_DONE;
}
