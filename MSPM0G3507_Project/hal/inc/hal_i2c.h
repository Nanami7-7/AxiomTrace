/**
 * @file    hal_i2c.h
 * @brief   I2C hardware abstraction layer interface
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- I2C role ---------- */
typedef enum {
    HAL_I2C_ROLE_CONTROLLER = 0,
    HAL_I2C_ROLE_TARGET     = 1,
} hal_i2c_role_t;

/* ---------- I2C speed ---------- */
typedef enum {
    HAL_I2C_SPEED_STANDARD = 100000,   /* 100 kHz */
    HAL_I2C_SPEED_FAST     = 400000,   /* 400 kHz */
    HAL_I2C_SPEED_FAST_PLUS = 1000000, /* 1 MHz   */
} hal_i2c_speed_t;

/* ---------- I2C configuration ---------- */
typedef struct {
    uint32_t          instance;      /* I2C_0_INST / I2C_1_INST */
    hal_i2c_role_t    role;
    hal_i2c_speed_t   speed;
    uint32_t          own_address;   /* Target mode address (7-bit) */
    hal_irq_priority_t irq_priority;
} hal_i2c_config_t;

/* ---------- I2C callbacks ---------- */
typedef void (*hal_i2c_rx_cb_t)(uint32_t instance);
typedef void (*hal_i2c_tx_cb_t)(uint32_t instance);

/* ========== API (Controller mode) ========== */

hal_status_t hal_i2c_init(const hal_i2c_config_t *config);
hal_status_t hal_i2c_deinit(uint32_t instance);

hal_status_t hal_i2c_master_write(uint32_t instance, uint16_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
hal_status_t hal_i2c_master_read(uint32_t instance, uint16_t dev_addr, uint8_t *buf, uint16_t len, uint32_t timeout_ms);
hal_status_t hal_i2c_master_write_read(uint32_t instance, uint16_t dev_addr,
                                        const uint8_t *wr_data, uint16_t wr_len,
                                        uint8_t *rd_buf, uint16_t rd_len,
                                        uint32_t timeout_ms);

hal_status_t hal_i2c_register_rx_callback(uint32_t instance, hal_i2c_rx_cb_t cb);
hal_status_t hal_i2c_register_tx_callback(uint32_t instance, hal_i2c_tx_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_H */
