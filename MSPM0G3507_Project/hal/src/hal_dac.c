/**
 * @file    hal_dac.c
 * @brief   DAC HAL implementation for MSPM0G3507
 */

#include "hal_conf.h"

#ifdef HAL_DAC_MODULE_ENABLED

#include "hal_dac.h"

/* ---------- DAC reference voltage in millivolts ---------- */
#define HAL_DAC_VDD_MV      3300
#define HAL_DAC_INTERNAL_MV 2500
#define HAL_DAC_MAX_VALUE   4095  /* 12-bit DAC */

/* ---------- Callback storage ---------- */
static hal_dac_cb_t s_dac_callback = NULL;

/* ========== API Implementation ========== */

hal_status_t hal_dac_init(const hal_dac_config_t *config)
{
    HAL_CHECK_PTR(config);

    DL_DAC_InitTypeDef dac_init = {0};

    switch (config->reference) {
    case HAL_DAC_REF_VDD: dac_init.vref = DL_DAC_VREF_VDDA; break;
    case HAL_DAC_REF_INT: dac_init.vref = DL_DAC_VREF_INT;  break;
    case HAL_DAC_REF_EXT: dac_init.vref = DL_DAC_VREF_EXT;  break;
    default: return HAL_INVALID;
    }

    DL_DAC_init(config->instance, &dac_init);
    DL_DAC_enable(config->instance);

    return HAL_OK;
}

hal_status_t hal_dac_deinit(uint32_t instance)
{
    DL_DAC_disable(instance);
    DL_DAC_reset(instance);
    return HAL_OK;
}

hal_status_t hal_dac_set_value(uint32_t instance, uint32_t value)
{
    if (value > HAL_DAC_MAX_VALUE) {
        return HAL_INVALID;
    }

    DL_DAC_output12(instance, value);
    return HAL_OK;
}

hal_status_t hal_dac_set_voltage(uint32_t instance, uint32_t millivolts)
{
    uint32_t vref_mv = HAL_DAC_VDD_MV;  /* Default VDD reference */
    uint32_t value = (uint32_t)((uint64_t)millivolts * HAL_DAC_MAX_VALUE / vref_mv);

    if (value > HAL_DAC_MAX_VALUE) {
        value = HAL_DAC_MAX_VALUE;
    }

    return hal_dac_set_value(instance, value);
}

hal_status_t hal_dac_start(uint32_t instance)
{
    DL_DAC_enableOutput(instance);
    return HAL_OK;
}

hal_status_t hal_dac_stop(uint32_t instance)
{
    DL_DAC_disableOutput(instance);
    return HAL_OK;
}

hal_status_t hal_dac_register_callback(uint32_t instance, hal_dac_cb_t cb)
{
    HAL_CHECK_PTR(cb);
    s_dac_callback = cb;
    return HAL_OK;
}

#endif /* HAL_DAC_MODULE_ENABLED */
