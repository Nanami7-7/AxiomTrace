/**
 * @file    hal_adc.c
 * @brief   ADC HAL implementation for MSPM0G3507
 */

#include "hal_conf.h"

#ifdef HAL_ADC_MODULE_ENABLED

#include "hal_adc.h"

/* ---------- Callback storage ---------- */
static hal_adc_conv_cb_t s_adc_callback = NULL;

/* ========== API Implementation ========== */

hal_status_t hal_adc_init(const hal_adc_config_t *config)
{
    HAL_CHECK_PTR(config);

    DL_ADC12_InitTypeDef adc_init = {0};

    /* Resolution */
    switch (config->resolution) {
    case HAL_ADC_RES_12BIT: adc_init.resolution = DL_ADC12_RESOLUTION_12BIT; break;
    case HAL_ADC_RES_10BIT: adc_init.resolution = DL_ADC12_RESOLUTION_10BIT; break;
    case HAL_ADC_RES_8BIT:  adc_init.resolution = DL_ADC12_RESOLUTION_8BIT;  break;
    default: return HAL_INVALID;
    }

    /* Reference */
    switch (config->reference) {
    case HAL_ADC_REF_VDD: adc_init.vref = DL_ADC12_VREF_VDDA; break;
    case HAL_ADC_REF_INT: adc_init.vref = DL_ADC12_VREF_INT;  break;
    case HAL_ADC_REF_EXT: adc_init.vref = DL_ADC12_VREF_EXT;  break;
    default: return HAL_INVALID;
    }

    DL_ADC12_init(config->instance, &adc_init);
    DL_ADC12_enable(config->instance);

    /* Enable FIFO and interrupt */
    DL_ADC12_enableFIFO(config->instance);
    DL_ADC12_enableInterrupt(config->instance, DL_ADC12_INTERRUPT_MEM_RESULT_FULL);

    NVIC_SetPriority(ADC0_INT_IRQn, config->irq_priority);
    NVIC_EnableIRQ(ADC0_INT_IRQn);

    return HAL_OK;
}

hal_status_t hal_adc_deinit(uint32_t instance)
{
    DL_ADC12_disable(instance);
    DL_ADC12_reset(instance);
    return HAL_OK;
}

hal_status_t hal_adc_config_channel(const hal_adc_channel_t *ch_config)
{
    HAL_CHECK_PTR(ch_config);

    DL_ADC12_configMem(ch_config->instance, ch_config->channel, &(DL_ADC12_MemConfig){
        .inputChannel = ch_config->channel,
    });

    return HAL_OK;
}

hal_status_t hal_adc_read_channel(uint32_t instance, uint32_t channel, uint32_t *value, uint32_t timeout_ms)
{
    HAL_CHECK_PTR(value);

    DL_ADC12_startConversion(instance);

    while (!DL_ADC12_isConversionComplete(instance, channel)) {}

    *value = DL_ADC12_getMemResult(instance, channel);

    return HAL_OK;
}

hal_status_t hal_adc_start_continuous(uint32_t instance, uint32_t channel)
{
    DL_ADC12_startConversion(instance);
    DL_ADC12_setRepeatMode(instance, DL_ADC12_REPEAT_MODE_CONTINUOUS);
    return HAL_OK;
}

hal_status_t hal_adc_stop_continuous(uint32_t instance)
{
    DL_ADC12_stopConversion(instance);
    DL_ADC12_setRepeatMode(instance, DL_ADC12_REPEAT_MODE_SINGLE);
    return HAL_OK;
}

hal_status_t hal_adc_register_callback(uint32_t instance, hal_adc_conv_cb_t cb)
{
    HAL_CHECK_PTR(cb);
    s_adc_callback = cb;
    return HAL_OK;
}

/* ---------- ISR ---------- */
void ADC0_IRQHandler(void)
{
    if (DL_ADC12_getEnabledInterruptStatus(ADC0_INST, DL_ADC12_INTERRUPT_MEM_RESULT_FULL)) {
        DL_ADC12_clearInterruptStatus(ADC0_INST, DL_ADC12_INTERRUPT_MEM_RESULT_FULL);
        if (s_adc_callback != NULL) {
            uint32_t value = DL_ADC12_getMemResult(ADC0_INST, 0);
            s_adc_callback(ADC0_INST, 0, value);
        }
    }
}

#endif /* HAL_ADC_MODULE_ENABLED */
