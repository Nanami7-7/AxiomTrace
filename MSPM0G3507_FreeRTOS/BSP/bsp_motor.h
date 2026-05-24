/**
 * @file    bsp_motor.h
 * @brief   电机驱动接口
 * @note    基于hal_timer(PWM控速) + hal_gpio(IN1/IN2控方向)实现
 *          四电机驱动(TB6612FNG), 每个电机1个PWM通道 + 2个方向引脚
 *          PWM由TIMA0产生(4通道CC0~CC3)
 *
 *          驱动逻辑(TB6612FNG):
 *          | IN1 | IN2 | PWM  | 模式 |
 *          |-----|-----|------|------|
 *          |  1  |  0  | >0   | 正转 |
 *          |  0  |  1  | >0   | 反转 |
 *          |  1  |  1  |  0   | 刹车 |
 *          |  0  |  0  |  0   | 滑行 |
 */
#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "bsp_common.h"
#include "hal_gpio.h"
#include "hal_timer.h"

/* ======================== 类型定义 ======================== */

/** 电机编号枚举 */
typedef enum {
    BSP_MOTOR_A = 0,   /**< 电机A(左前): CC0=PB8, IN1=PB24, IN2=PB20 */
    BSP_MOTOR_B,       /**< 电机B(左后): CC1=PA22, IN1=PA24, IN2=PA31 */
    BSP_MOTOR_C,       /**< 电机C(右前): CC2=PA15, IN1=PA2, IN2=PA7 */
    BSP_MOTOR_D,       /**< 电机D(右后): CC3=PA17, IN1=PB6, IN2=PB7 */
    BSP_MOTOR_COUNT    /**< 电机总数 */
} bsp_motor_id_t;

/** 电机停止模式 */
typedef enum {
    BSP_MOTOR_MODE_COAST = 0, /**< 滑行(自由停止, IN1=0,IN2=0) */
    BSP_MOTOR_MODE_BRAKE,     /**< 刹车(短接制动, IN1=1,IN2=1) */
} bsp_motor_stop_mode_t;

/** 电机配置结构体 */
typedef struct {
    uint32_t        pwm_ch;    /**< PWM通道索引(CC0~CC3) */
    hal_gpio_port_t in1_port;  /**< IN1引脚端口 */
    uint32_t        in1_pin;   /**< IN1引脚编号 */
    hal_gpio_port_t in2_port;  /**< IN2引脚端口 */
    uint32_t        in2_pin;   /**< IN2引脚编号 */
    int8_t          dir_sign;  /**< 方向修正(+1/-1, 0视为+1) */
} bsp_motor_config_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化电机驱动
 * @param  cfg       电机配置表
 * @param  count     配置数量(<=BSP_MOTOR_COUNT)
 * @param  pwm_timer PWM定时器实例
 * @param  pwm_period PWM周期(占空比最大值)
 * @note   启动PWM定时器, 设置方向引脚为滑行状态, PWM清零
 *         PWM定时器已由SYSCFG_DL_init()配置
 * @retval BSP_OK 初始化成功
 */
bsp_status_t bsp_motor_init(const bsp_motor_config_t *cfg,
                             uint32_t count,
                             hal_timer_id_t pwm_timer,
                             uint32_t pwm_period);

/**
 * @brief  获取PWM占空比最大值
 * @retval 占空比最大值(未初始化返回0)
 */
uint32_t bsp_motor_get_duty_max(void);

/**
 * @brief  占空比百分比换算: percent(0~100) -> duty值
 * @param  percent 占空比百分比
 * @retval duty值
 */
uint32_t bsp_motor_percent_to_duty(uint32_t percent);

/**
 * @brief  设置电机速度和方向
 * @param  motor  电机编号
 * @param  speed  速度值(0~PWM周期)
 *                正值=正转(IN1=1,IN2=0), 负值=反转(IN1=0,IN2=1)
 *                0=刹车(IN1=1,IN2=1, PWM=0)
 * @retval BSP_OK           设置成功
 * @retval BSP_ERR_INVALID_PARAM 电机编号越界
 */
bsp_status_t bsp_motor_set_speed(bsp_motor_id_t motor,
                                  int32_t speed);

/**
 * @brief  停止电机
 * @param  motor 电机编号
 * @param  mode  停止模式(滑行/刹车)
 * @retval BSP_OK           停止成功
 * @retval BSP_ERR_INVALID_PARAM 参数无效
 */
bsp_status_t bsp_motor_stop(bsp_motor_id_t motor,
                             bsp_motor_stop_mode_t mode);

/**
 * @brief  停止所有电机(刹车模式)
 */
void bsp_motor_stop_all(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MOTOR_H */
