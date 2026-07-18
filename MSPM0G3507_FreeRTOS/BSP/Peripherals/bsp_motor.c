/**
 * @file    bsp_motor.c
 * @brief   电机驱动实现
 * @note    基于hal_timer(PWM控速) + hal_gpio(IN1/IN2控方向)实现
 *          四电机驱动(TB6612FNG)
 *
 *          核心逻辑: 每个电机1个PWM通道(控速) + 2个GPIO(控方向)
 *          方向由IN1/IN2组合决定, 速度由PWM占空比决定
 *
 *          TB6612FNG真值表:
 *          | IN1 | IN2 | PWM  | 模式 |
 *          |-----|-----|------|------|
 *          |  1  |  0  | >0   | 正转 |
 *          |  0  |  1  | >0   | 反转 |
 *          |  1  |  1  |  0   | 刹车 |
 *          |  0  |  0  |  0   | 滑行 |
 */
#include "bsp_motor.h"

#ifdef BSP_MOTOR_ENABLE

#include "hal_timer.h"
#include "hal_gpio.h"
#include "osal_api.h"
#include "axiomtrace.h"
#include <stdio.h>

/* ======================== 私有变量 ======================== */

/** 电机配置表(由应用层传入) */
static const bsp_motor_config_t *s_motor_cfg = NULL;

/** 电机配置数量 */
static uint32_t s_motor_cfg_count = 0U;

/** PWM定时器实例 */
static hal_timer_id_t s_pwm_timer;

/** PWM周期(占空比最大值) */
static uint32_t s_pwm_period = 0U;

/** 电机初始化标志 */
static bool s_motor_inited = false;

/** 各电机上次方向(+1=正转, -1=反转, 0=刹车/停止) */
static int8_t s_motor_prev_dir[BSP_MOTOR_COUNT] = {0};

/* ======================== 私有函数 ======================== */

/**
 * @brief  设置电机方向引脚为滑行状态(IN1=0, IN2=0)
 */
static void set_coast(bsp_motor_id_t motor)
{
    const bsp_motor_config_t *p = &s_motor_cfg[motor];
    (void)hal_gpio_clear_pin(p->in1_port, p->in1_pin);
    (void)hal_gpio_clear_pin(p->in2_port, p->in2_pin);
}

/**
 * @brief  设置电机方向引脚为刹车状态(IN1=1, IN2=1)
 */
static void set_brake(bsp_motor_id_t motor)
{
    const bsp_motor_config_t *p = &s_motor_cfg[motor];
    (void)hal_gpio_set_pin(p->in1_port, p->in1_pin);
    (void)hal_gpio_set_pin(p->in2_port, p->in2_pin);
}

/**
 * @brief  设置电机正转方向(IN1=1, IN2=0)
 */
static void set_forward(bsp_motor_id_t motor)
{
    const bsp_motor_config_t *p = &s_motor_cfg[motor];
    (void)hal_gpio_set_pin(p->in1_port, p->in1_pin);
    (void)hal_gpio_clear_pin(p->in2_port, p->in2_pin);
}

/**
 * @brief  设置电机反转方向(IN1=0, IN2=1)
 * @param  motor 电机编号
 */
static void set_reverse(bsp_motor_id_t motor)
{
    const bsp_motor_config_t *p = &s_motor_cfg[motor];
    (void)hal_gpio_clear_pin(p->in1_port, p->in1_pin);
    (void)hal_gpio_set_pin(p->in2_port, p->in2_pin);
}

/**
 * @brief  限幅函数
 * @param  val 输入值
 * @param  min 最小值
 * @param  max 最大值
 * @retval 限幅后的值
 */
static inline int32_t clamp_int32(int32_t val, int32_t min_val,
                                   int32_t max_val)
{
    if (val < min_val) { return min_val; }
    if (val > max_val) { return max_val; }
    return val;
}

/* ======================== 公共函数实现 ======================== */

bsp_status_t bsp_motor_init(const bsp_motor_config_t *cfg,
                             uint32_t count,
                             hal_timer_id_t pwm_timer,
                             uint32_t pwm_period)
{
    if (s_motor_inited) {
        return BSP_OK;
    }

    if ((cfg == NULL) || (count == 0U) ||
        (count > BSP_MOTOR_COUNT) || (pwm_period == 0U)) {
        return BSP_ERR_INVALID_PARAM;
    }

    s_motor_cfg = cfg;
    s_motor_cfg_count = count;
    s_pwm_timer = pwm_timer;
    s_pwm_period = pwm_period;

    /* 启动PWM定时器(SysConfig已配置,仅需启动计数) */
    hal_status_t ret = hal_timer_start(s_pwm_timer);
    if (ret != HAL_OK) {
        return BSP_ERR_HW_FAULT;
    }

    /* 所有电机初始化为滑行状态, PWM占空比清零 */
    for (uint32_t i = 0; i < s_motor_cfg_count; i++) {
        set_coast((bsp_motor_id_t)i);
        (void)hal_timer_set_pwm_duty(s_pwm_timer,
            s_motor_cfg[i].pwm_ch, 0U);
        s_motor_prev_dir[i] = 0;
    }

    s_motor_inited = true;
    return BSP_OK;
}

bsp_status_t bsp_motor_set_speed(bsp_motor_id_t motor, int32_t speed)
{
    if ((uint32_t)motor >= s_motor_cfg_count) {
        return BSP_ERR_INVALID_PARAM;
    }

    if (!s_motor_inited) {
        return BSP_ERR_NOT_INIT;
    }

    int8_t dir_sign = s_motor_cfg[motor].dir_sign;
    if (dir_sign == 0) {
        dir_sign = 1;
    }

    int32_t signed_speed = speed * (int32_t)dir_sign;

    /* 限幅到PWM周期 */
    int32_t clamped_speed = clamp_int32(signed_speed,
        -(int32_t)s_pwm_period,
         (int32_t)s_pwm_period);

    int8_t new_dir;
    if (clamped_speed > 0) {
        new_dir = 1;
    } else if (clamped_speed < 0) {
        new_dir = -1;
    } else {
        new_dir = 0;
    }

    /*
     * 方向切换死区保护:
     * 当方向从正转↔反转变化时, 先刹车1个控制周期(PWM=0),
     * 避免H桥上下管直通. 本周期仅刹车, 不输出PWM,
     * 下次调用时 s_motor_prev_dir 已更新, 正常输出.
     */
    if (new_dir != 0 && s_motor_prev_dir[motor] != 0 &&
        new_dir != s_motor_prev_dir[motor]) {
        /* 方向突变: 紧急刹车 */
        {
            char dbg[48];
            (void)snprintf(dbg, sizeof(dbg),
                "Motor %lu dir-reversal brake",
                (unsigned long)motor);
            AX_LOG_DEBUG(dbg);
        }
        set_brake(motor);
        (void)hal_timer_set_pwm_duty(s_pwm_timer,
            s_motor_cfg[motor].pwm_ch, 0U);
        s_motor_prev_dir[motor] = 0;
        return BSP_OK;
    }

    s_motor_prev_dir[motor] = new_dir;

    if (clamped_speed > 0) {
        /* 正转: IN1=1, IN2=0, PWM=占空比 */
        set_forward(motor);
        (void)hal_timer_set_pwm_duty(s_pwm_timer,
            s_motor_cfg[motor].pwm_ch,
            (uint32_t)clamped_speed);
    } else if (clamped_speed < 0) {
        /* 反转: IN1=0, IN2=1, PWM=占空比 */
        set_reverse(motor);
        (void)hal_timer_set_pwm_duty(s_pwm_timer,
            s_motor_cfg[motor].pwm_ch,
            (uint32_t)(-clamped_speed));
    } else {
        /* 速度为0: 刹车, PWM=0 */
        set_brake(motor);
        (void)hal_timer_set_pwm_duty(s_pwm_timer,
            s_motor_cfg[motor].pwm_ch, 0U);
    }

    return BSP_OK;
}

bsp_status_t bsp_motor_stop(bsp_motor_id_t motor,
                             bsp_motor_stop_mode_t mode)
{
    if ((uint32_t)motor >= s_motor_cfg_count) {
        return BSP_ERR_INVALID_PARAM;
    }

    if (!s_motor_inited) {
        return BSP_ERR_NOT_INIT;
    }

    /* 先将PWM占空比清零 */
    (void)hal_timer_set_pwm_duty(s_pwm_timer,
        s_motor_cfg[motor].pwm_ch, 0U);

    /* 设置方向引脚 */
    if (mode == BSP_MOTOR_MODE_BRAKE) {
        set_brake(motor);
    } else {
        set_coast(motor);
    }

    /* 重置方向记录, 避免重启时触发不必要的方向切换死区 */
    s_motor_prev_dir[motor] = 0;

    return BSP_OK;
}

void bsp_motor_stop_all(void)
{
    for (uint32_t i = 0; i < s_motor_cfg_count; i++) {
        (void)bsp_motor_stop((bsp_motor_id_t)i,
                              BSP_MOTOR_MODE_BRAKE);
    }
}

uint32_t bsp_motor_get_duty_max(void)
{
    return s_pwm_period;
}

uint32_t bsp_motor_percent_to_duty(uint32_t percent)
{
    if (s_pwm_period == 0U) {
        return 0U;
    }

    if (percent > 100U) {
        percent = 100U;
    }

    return (percent * s_pwm_period) / 100U;
}

#endif /* BSP_MOTOR_ENABLE */
