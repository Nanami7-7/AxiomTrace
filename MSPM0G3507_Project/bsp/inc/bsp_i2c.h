/**
 * @file    bsp_i2c.h
 * @brief   BSP I2C interface (board-level I2C devices)
 */

#ifndef BSP_I2C_H
#define BSP_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ========== API ========== */

hal_status_t bsp_i2c_init(void);

hal_status_t bsp_i2c0_write(uint16_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
hal_status_t bsp_i2c0_read(uint16_t dev_addr, uint8_t *buf, uint16_t len, uint32_t timeout_ms);
hal_status_t bsp_i2c0_write_read(uint16_t dev_addr,
                                  const uint8_t *wr_data, uint16_t wr_len,
                                  uint8_t *rd_buf, uint16_t rd_len,
                                  uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* BSP_I2C_H */
