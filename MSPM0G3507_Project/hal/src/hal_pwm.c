/**
 * @file    hal_pwm.c
 * @brief   PWM HAL implementation for MSPM0G3507
 * @note    PWM uses Timer hardware in compare/PWM mode
 */

#include "hal_conf.h"

#ifdef HAL_PWM_MODULE_ENABLED

#include "hal_pwm.h"

/* ========== API Implementation ========== */

hal_status_t hal_pwm_init(const hal_pwm_config_t *config)
{
    HAL_CHECK_PTR(config);

    /* Initialize timer in PWM mode */
    DL_TimerG_InitTypeDef pwm_init = {0};
    pwm_init.mode         = DL_TIMERG_MODE_PWM;
    pwm_init.clockSource  = DL_TIMERG_CLOCK_BUSCLK;
    pwm_init.clockDiv     = DL_TIMERG_CLOCK_DIV_1;

    /* Calculate load value from frequency */
    uint32_t busclk = 80000000UL;
    pwm_init.period = busclk / config->freq_hz;

    DL_TimerG_init(config->instance, &pwm_init);
    DL_TimerG_enable(config->instance);

    /* Set initial duty cycle */
    hal_pwm_set_duty(config->instance, config->channel, config->duty_percent);

    return HAL_OK;
}

hal_status_t hal_pwm_deinit(uint32_t instance, uint32_t channel)
{
    DL_TimerG_disable(instance);
    DL_TimerG_reset(instance);
    return HAL_OK;
}

hal_status_t hal_pwm_start(uint32_t instance, uint32_t channel)
{
    DL_TimerG_startCounter(instance);
    return HAL_OK;
}

hal_status_t hal_pwm_stop(uint32_t instance, uint32_t channel)
{
    DL_TimerG_stopCounter(instance);
    return HAL_OK;
}

hal_status_t hal_pwm_set_duty(uint32_t instance, uint32_t channel, uint32_t duty_percent)
{
    if (duty_percent > 100) {
        return HAL_INVALID;
    }

    uint32_t load_val = DL_TimerG_getLoadValue(instance);
    uint32_t cmp_val  = (load_val * duty_percent) / 100;

    DL_TimerG_setCaptureCompareValue(instance, channel, cmp_val);
    return HAL_OK;
}

hal_status_t hal_pwm_set_frequency(uint32_t instance, uint32_t channel, uint32_t freq_hz)
{
    uint32_t busclk   = 80000000UL;
    uint32_t load_val = busclk / freq_hz;

    DL_TimerG_setLoadValue(instance, load_val);
    return HAL_OK;
}

#endif /* HAL_PWM_MODULE_ENABLED */
