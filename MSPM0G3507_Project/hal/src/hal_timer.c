/**
 * @file    hal_timer.c
 * @brief   Timer HAL implementation for MSPM0G3507
 */

#include "hal_conf.h"

#ifdef HAL_TIMER_MODULE_ENABLED

#include "hal_timer.h"

/* ---------- Callback storage ---------- */
#define HAL_TIMER_MAX_INSTANCES  4

typedef struct {
    hal_timer_cb_t callback;
} hal_timer_ctx_t;

static hal_timer_ctx_t s_timer_ctx[HAL_TIMER_MAX_INSTANCES];

static uint32_t timer_idx_from_instance(uint32_t instance)
{
    /* Map instance to 0-based index */
    if (instance == TIMA0_INST) return 0;
    if (instance == TIMA1_INST) return 1;
    if (instance == TIMG0_INST) return 2;
    if (instance == TIMG1_INST) return 3;
    return 0;
}

/* ========== API Implementation ========== */

hal_status_t hal_timer_init(const hal_timer_config_t *config)
{
    HAL_CHECK_PTR(config);

    DL_TimerG_InitTypeDef timer_init = {0};

    timer_init.mode = (config->mode == HAL_TIMER_MODE_PERIODIC)
                      ? DL_TIMERG_MODE_PERIODIC
                      : DL_TIMERG_MODE_ONE_SHOT;

    switch (config->clk_src) {
    case HAL_TIMER_CLK_BUSCLK: timer_init.clockSource = DL_TIMERG_CLOCK_BUSCLK; break;
    case HAL_TIMER_CLK_LFCLK:  timer_init.clockSource = DL_TIMERG_CLOCK_LFCLK;  break;
    case HAL_TIMER_CLK_MFCLK:  timer_init.clockSource = DL_TIMERG_CLOCK_MFCLK;  break;
    default: return HAL_INVALID;
    }

    timer_init.clockDiv = config->clk_div;
    timer_init.period = config->period_us;

    DL_TimerG_init(config->instance, &timer_init);
    DL_TimerG_enable(config->instance);

    /* Enable interrupt */
    DL_TimerG_enableInterrupt(config->instance, DL_TIMERG_INTERRUPT_ZERO_EVENT);

    uint32_t idx = timer_idx_from_instance(config->instance);
    uint32_t irqn;
    switch (idx) {
    case 0: irqn = TIMA0_INT_IRQn; break;
    case 1: irqn = TIMA1_INT_IRQn; break;
    case 2: irqn = TIMG0_INT_IRQn; break;
    case 3: irqn = TIMG1_INT_IRQn; break;
    default: irqn = TIMG0_INT_IRQn; break;
    }
    NVIC_SetPriority(irqn, config->irq_priority);
    NVIC_EnableIRQ(irqn);

    return HAL_OK;
}

hal_status_t hal_timer_deinit(uint32_t instance)
{
    DL_TimerG_disable(instance);
    DL_TimerG_reset(instance);
    return HAL_OK;
}

hal_status_t hal_timer_start(uint32_t instance)
{
    DL_TimerG_startCounter(instance);
    return HAL_OK;
}

hal_status_t hal_timer_stop(uint32_t instance)
{
    DL_TimerG_stopCounter(instance);
    return HAL_OK;
}

hal_status_t hal_timer_set_period(uint32_t instance, uint32_t period_us)
{
    DL_TimerG_setLoadValue(instance, period_us);
    return HAL_OK;
}

uint32_t hal_timer_get_counter(uint32_t instance)
{
    return DL_TimerG_getCounterValue(instance);
}

hal_status_t hal_timer_register_callback(uint32_t instance, hal_timer_cb_t cb)
{
    HAL_CHECK_PTR(cb);

    uint32_t idx = timer_idx_from_instance(instance);
    if (idx >= HAL_TIMER_MAX_INSTANCES) {
        return HAL_INVALID;
    }
    s_timer_ctx[idx].callback = cb;
    return HAL_OK;
}

/* ---------- ISR dispatchers ---------- */
void TIMG0_IRQHandler(void)
{
    if (DL_TimerG_getEnabledInterruptStatus(TIMG0_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT)) {
        DL_TimerG_clearInterruptStatus(TIMG0_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
        if (s_timer_ctx[2].callback != NULL) {
            s_timer_ctx[2].callback(TIMG0_INST);
        }
    }
}

void TIMG1_IRQHandler(void)
{
    if (DL_TimerG_getEnabledInterruptStatus(TIMG1_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT)) {
        DL_TimerG_clearInterruptStatus(TIMG1_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
        if (s_timer_ctx[3].callback != NULL) {
            s_timer_ctx[3].callback(TIMG1_INST);
        }
    }
}

#endif /* HAL_TIMER_MODULE_ENABLED */
