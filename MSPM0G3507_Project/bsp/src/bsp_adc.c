/**
 * @file    bsp_adc.c
 * @brief   BSP ADC implementation (board-level analog inputs)
 */

#include "bsp_adc.h"
#include "bsp_pin_config.h"
#include "bsp_config.h"
#include "hal_adc.h"

/* ---------- Channel map ---------- */
typedef struct {
    uint32_t instance;
    uint32_t channel;
} bsp_adc_channel_map_t;

static const bsp_adc_channel_map_t s_adc_channels[] = {
    [BSP_ADC_CH_INTERNAL_TEMP] = { BSP_ADC0_INST, BSP_ADC_TEMP_CHANNEL },
    [BSP_ADC_CH_AIN0]         = { BSP_ADC0_INST, BSP_ADC_AIN0_CHANNEL },
};

/* ========== API ========== */

hal_status_t bsp_adc_init(void)
{
    hal_adc_config_t cfg = {
        .instance     = BSP_ADC0_INST,
        .resolution   = HAL_ADC_RES_12BIT,
        .reference    = HAL_ADC_REF_VDD,
        .irq_priority = HAL_IRQ_PRIORITY_LOW,
    };

    hal_status_t status = hal_adc_init(&cfg);
    if (status != HAL_OK) return status;

    /* Configure channels */
    for (uint32_t i = 0; i < sizeof(s_adc_channels) / sizeof(s_adc_channels[0]); i++) {
        hal_adc_channel_t ch = {
            .instance = s_adc_channels[i].instance,
            .channel  = s_adc_channels[i].channel,
        };
        status = hal_adc_config_channel(&ch);
        if (status != HAL_OK) return status;
    }

    return HAL_OK;
}

hal_status_t bsp_adc_read_channel(bsp_adc_channel_t channel, uint32_t *raw_value, uint32_t timeout_ms)
{
    if (channel >= sizeof(s_adc_channels) / sizeof(s_adc_channels[0])) {
        return HAL_INVALID;
    }

    return hal_adc_read_channel(s_adc_channels[channel].instance,
                                s_adc_channels[channel].channel,
                                raw_value, timeout_ms);
}

hal_status_t bsp_adc_read_voltage_mv(bsp_adc_channel_t channel, uint32_t *voltage_mv, uint32_t timeout_ms)
{
    HAL_CHECK_PTR(voltage_mv);

    uint32_t raw;
    hal_status_t status = bsp_adc_read_channel(channel, &raw, timeout_ms);
    if (status != HAL_OK) return status;

    /* 12-bit ADC: raw = 0~4095, Vref = BSP_ADC_VREF_MV */
    *voltage_mv = (uint32_t)((uint64_t)raw * BSP_ADC_VREF_MV / 4095);

    return HAL_OK;
}

hal_status_t bsp_adc_read_temperature(bsp_adc_channel_t channel, int32_t *temp_deci_celsius, uint32_t timeout_ms)
{
    HAL_CHECK_PTR(temp_deci_celsius);

    uint32_t voltage_mv;
    hal_status_t status = bsp_adc_read_voltage_mv(channel, &voltage_mv, timeout_ms);
    if (status != HAL_OK) return status;

    /* Simplified temperature conversion for internal sensor
     * Production code should use calibration data from TLV */
    *temp_deci_celsius = (int32_t)voltage_mv - 500; /* Example: 500mV = 0°C, 1mV/0.1°C */

    return HAL_OK;
}
