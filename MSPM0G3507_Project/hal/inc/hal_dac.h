/**
 * @file    hal_dac.h
 * @brief   DAC hardware abstraction layer interface
 */

#ifndef HAL_DAC_H
#define HAL_DAC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- DAC reference ---------- */
typedef enum {
    HAL_DAC_REF_VDD  = 0,
    HAL_DAC_REF_INT  = 1,
    HAL_DAC_REF_EXT  = 2,
} hal_dac_reference_t;

/* ---------- DAC configuration ---------- */
typedef struct {
    uint32_t          instance;      /* DAC0_INST */
    hal_dac_reference_t reference;
    hal_irq_priority_t irq_priority;
} hal_dac_config_t;

/* ---------- DAC callback ---------- */
typedef void (*hal_dac_cb_t)(uint32_t instance);

/* ========== API ========== */

hal_status_t hal_dac_init(const hal_dac_config_t *config);
hal_status_t hal_dac_deinit(uint32_t instance);

hal_status_t hal_dac_set_value(uint32_t instance, uint32_t value);
hal_status_t hal_dac_set_voltage(uint32_t instance, uint32_t millivolts);

hal_status_t hal_dac_start(uint32_t instance);
hal_status_t hal_dac_stop(uint32_t instance);

hal_status_t hal_dac_register_callback(uint32_t instance, hal_dac_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DAC_H */
