/**
 * @file    hal_timer.c
 * @brief   HAL Timer硬件抽象实现
 * @note    封装DL_Timer_xxx调用，通过hal_timer_id_t枚举映射
 *          DL_TimerA_xxx和DL_TimerG_xxx均为DL_Timer_xxx的宏别名，
 *          无需区分TimerA/TimerG类型，统一调用DL_Timer_xxx即可
 *          所有定时器已在SysConfig中完成初始化，此模块仅提供运行时控制
 */
#include "hal_timer.h"
#include "ti_msp_dl_config.h"

/* ======================== 私有类型 ======================== */

/**
 * @brief 定时器实例属性结构体
 * @note  存储每个定时器的寄存器基址、中断号、功能标志等静态信息
 */
typedef struct {
    GPTIMER_Regs *inst;    /**< 定时器寄存器基址指针 */
    IRQn_Type     irqn;    /**< NVIC中断号 */
    bool          pwm_cap; /**< 是否支持PWM输出(捕获定时器为false) */
} timer_instance_t;

/* ======================== 私有映射表 ======================== */

/**
 * @brief 定时器实例属性映射表
 * @note  顺序必须与hal_timer_id_t枚举一致
 */
static const timer_instance_t s_timer_map[HAL_TIMER_COUNT] = {
    /* HAL_TIMER_PWM_MOTOR -> TIMA0, 4通道PWM */
    { (GPTIMER_Regs *)TIMA0, TIMA0_INT_IRQn, true  },
    /* HAL_TIMER_CAPTURE_LF -> TIMG8, 编码器捕获 */
    { (GPTIMER_Regs *)TIMG8, TIMG8_INT_IRQn, false },
    /* HAL_TIMER_CAPTURE_LB -> TIMG7, 编码器捕获 */
    { (GPTIMER_Regs *)TIMG7, TIMG7_INT_IRQn, false },
    /* HAL_TIMER_CAPTURE_RF -> TIMG6, 编码器捕获 */
    { (GPTIMER_Regs *)TIMG6, TIMG6_INT_IRQn, false },
    /* HAL_TIMER_CAPTURE_RB -> TIMG0, 编码器捕获 */
    { (GPTIMER_Regs *)TIMG0, TIMG0_INT_IRQn, false },
    /* HAL_TIMER_SYS_TICK -> TIMA1, 系统节拍 */
    { (GPTIMER_Regs *)TIMA1, TIMA1_INT_IRQn, false },
};

/* ======================== 内联辅助函数 ======================== */

/**
 * @brief  检查定时器实例枚举是否合法
 */
static inline bool is_timer_valid(hal_timer_id_t id)
{
    return ((uint32_t)id < HAL_TIMER_COUNT);
}

/* ======================== 公共函数实现 ======================== */

hal_status_t hal_timer_enable_irq(hal_timer_id_t id)
{
    if (!is_timer_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    NVIC_ClearPendingIRQ(s_timer_map[id].irqn);
    NVIC_EnableIRQ(s_timer_map[id].irqn);

    return HAL_OK;
}

hal_status_t hal_timer_disable_irq(hal_timer_id_t id)
{
    if (!is_timer_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    NVIC_DisableIRQ(s_timer_map[id].irqn);
    return HAL_OK;
}

hal_status_t hal_timer_start(hal_timer_id_t id)
{
    if (!is_timer_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    DL_Timer_startCounter(s_timer_map[id].inst);

    return HAL_OK;
}

hal_status_t hal_timer_stop(hal_timer_id_t id)
{
    if (!is_timer_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    DL_Timer_stopCounter(s_timer_map[id].inst);

    return HAL_OK;
}

hal_status_t hal_timer_set_pwm_duty(hal_timer_id_t id, uint32_t channel,
                                     uint32_t value)
{
    if (!is_timer_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }
    /* 非PWM定时器不支持此操作 */
    if (!s_timer_map[id].pwm_cap) {
        return HAL_ERR_UNSUPPORTED;
    }

    /*
     * 设置捕获比较值(PWM占空比)
     * channel: 0->CC0, 1->CC1, 2->CC2, 3->CC3
     */
    DL_Timer_setCaptureCompareValue(s_timer_map[id].inst, value, channel);

    return HAL_OK;
}

uint32_t hal_timer_get_count(hal_timer_id_t id)
{
    if (!is_timer_valid(id)) {
        return 0U;
    }

    return DL_Timer_getTimerCount(s_timer_map[id].inst);
}

hal_timer_irq_flag_t hal_timer_get_irq_flag(hal_timer_id_t id)
{
    if (!is_timer_valid(id)) {
        return HAL_TIMER_IRQ_NONE;
    }

    hal_timer_irq_flag_t flags = HAL_TIMER_IRQ_NONE;

    /*
     * 统一使用DL_Timer_getPendingInterrupt + DL_TIMER_IIDX_xxx
     * TimerA/TimerG的IIDX宏均为DL_TIMER_IIDX_xxx的别名，无需分派
     */
    switch (DL_Timer_getPendingInterrupt(s_timer_map[id].inst)) {
    case DL_TIMER_IIDX_CC0_UP:
        flags = HAL_TIMER_IRQ_CC0;
        break;
    case DL_TIMER_IIDX_CC1_UP:
        flags = HAL_TIMER_IRQ_CC1;
        break;
    case DL_TIMER_IIDX_LOAD:
        flags = HAL_TIMER_IRQ_LOAD;
        break;
    default:
        break;
    }

    return flags;
}

uint32_t hal_timer_get_capture_value(hal_timer_id_t id, uint32_t channel)
{
    if (!is_timer_valid(id)) {
        return 0U;
    }

    return DL_Timer_getCaptureCompareValue(s_timer_map[id].inst, channel);
}

hal_status_t hal_timer_reset_count(hal_timer_id_t id)
{
    if (!is_timer_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    DL_Timer_setTimerCount(s_timer_map[id].inst, 0U);

    return HAL_OK;
}
