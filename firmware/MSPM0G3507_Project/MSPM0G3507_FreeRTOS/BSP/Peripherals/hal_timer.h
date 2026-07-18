/**
 * @file    hal_timer.h
 * @brief   HAL Timer硬件抽象接口
 * @note    DL_TimerA_xxx/DL_TimerG_xxx均为DL_Timer_xxx的宏别名，
 *          统一使用DL_Timer_xxx接口，无需区分TimerA/TimerG类型。
 *          所有定时器已在SYSCFG_DL_init()中完成时钟/模式/周期配置，
 */
#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "hal_common.h"

/* ======================== 函数接口 ======================== */

/**
 * @brief  使能定时器NVIC中断
 * @note   SYSCFG_DL_init()配置了定时器中断但未使能NVIC，
 *         需要中断时调用此函数
 * @param  id 定时器实例编号
 * @retval HAL_OK           使能成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_timer_enable_irq(hal_timer_id_t id);

/**
 * @brief  禁止定时器NVIC中断
 * @param  id 定时器实例编号
 * @retval HAL_OK           禁止成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_timer_disable_irq(hal_timer_id_t id);

/**
 * @brief  启动定时器计数
 * @param  id 定时器实例编号
 * @retval HAL_OK           启动成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_timer_start(hal_timer_id_t id);

/**
 * @brief  停止定时器计数
 * @param  id 定时器实例编号
 * @retval HAL_OK           停止成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_timer_stop(hal_timer_id_t id);

/**
 * @brief  设置PWM占空比值
 * @note   仅适用于PWM类型的定时器(HAL_TIMER_PWM_MOTOR)
 * @param  id      定时器实例编号
 * @param  channel 通道索引(0~3对应CC0~CC3)
 * @param  value   占空比值(0=0%, period=100%)
 * @retval HAL_OK           设置成功
 * @retval HAL_ERR_INVALID_PARAM 参数无效
 * @retval HAL_ERR_UNSUPPORTED 非PWM定时器
 */
hal_status_t hal_timer_set_pwm_duty(hal_timer_id_t id, uint32_t channel,
                                     uint32_t value);

/**
 * @brief  获取定时器当前计数值
 * @param  id 定时器实例编号
 * @retval 当前计数值
 */
uint32_t hal_timer_get_count(hal_timer_id_t id);

/**
 * @brief  获取定时器中断挂起标志
 * @note   在定时器中断服务函数中调用，判断中断来源
 * @param  id 定时器实例编号
 * @retval hal_timer_irq_flag_t 中断标志位组合
 */
hal_timer_irq_flag_t hal_timer_get_irq_flag(hal_timer_id_t id);

/**
 * @brief  获取编码器捕获值
 * @note   在编码器捕获中断中调用，获取脉冲宽度或周期值
 * @param  id      定时器实例编号
 * @param  channel 捕获通道(0=CC0脉宽, 1=CC1周期)
 * @retval 捕获计数值
 */
uint32_t hal_timer_get_capture_value(hal_timer_id_t id, uint32_t channel);

/**
 * @brief  将定时器计数器归零
 * @note   用于编码器捕获后手动置零，避免溢出处理
 * @param  id 定时器实例编号
 * @retval HAL_OK           设置成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_timer_reset_count(hal_timer_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TIMER_H */
