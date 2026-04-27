/**
 * @file    bsp_adc.h
 * @brief   BSP ADC interface (board-level analog inputs)
 */

#ifndef BSP_ADC_H
#define BSP_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- ADC channel enumeration ---------- */
typedef enum {
    BSP_ADC_CH_INTERNAL_TEMP = 0,
    BSP_ADC_CH_AIN0         = 1,
} bsp_adc_channel_t;

/* ========== API ========== */

hal_status_t bsp_adc_init(void);

hal_status_t bsp_adc_read_channel(bsp_adc_channel_t channel, uint32_t *raw_value, uint32_t timeout_ms);

/**
 * @brief  Read ADC channel and convert to millivolts
 */
hal_status_t bsp_adc_read_voltage_mv(bsp_adc_channel_t channel, uint32_t *voltage_mv, uint32_t timeout_ms);

/**
 * @brief  Read internal temperature sensor (result in 0.1°C units)
 */
hal_status_t bsp_adc_read_temperature(bsp_adc_channel_t channel, int32_t *temp_deci_celsius, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_H */
