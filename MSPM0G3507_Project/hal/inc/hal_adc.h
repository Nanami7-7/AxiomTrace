/**
 * @file    hal_adc.h
 * @brief   ADC hardware abstraction layer interface
 */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- ADC resolution ---------- */
typedef enum {
    HAL_ADC_RES_12BIT = 12,
    HAL_ADC_RES_10BIT = 10,
    HAL_ADC_RES_8BIT  = 8,
} hal_adc_resolution_t;

/* ---------- ADC reference ---------- */
typedef enum {
    HAL_ADC_REF_VDD  = 0,
    HAL_ADC_REF_INT  = 1,
    HAL_ADC_REF_EXT  = 2,
} hal_adc_reference_t;

/* ---------- ADC configuration ---------- */
typedef struct {
    uint32_t             instance;      /* ADC0_INST etc. */
    hal_adc_resolution_t resolution;
    hal_adc_reference_t  reference;
    hal_irq_priority_t   irq_priority;
} hal_adc_config_t;

/* ---------- ADC channel config ---------- */
typedef struct {
    uint32_t instance;
    uint32_t channel;    /* ADC input channel number */
} hal_adc_channel_t;

/* ---------- ADC callback ---------- */
typedef void (*hal_adc_conv_cb_t)(uint32_t instance, uint32_t channel, uint32_t value);

/* ========== API ========== */

hal_status_t hal_adc_init(const hal_adc_config_t *config);
hal_status_t hal_adc_deinit(uint32_t instance);

hal_status_t hal_adc_config_channel(const hal_adc_channel_t *ch_config);
hal_status_t hal_adc_read_channel(uint32_t instance, uint32_t channel, uint32_t *value, uint32_t timeout_ms);

hal_status_t hal_adc_start_continuous(uint32_t instance, uint32_t channel);
hal_status_t hal_adc_stop_continuous(uint32_t instance);

hal_status_t hal_adc_register_callback(uint32_t instance, hal_adc_conv_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ADC_H */
