/**
 * @file    bsp_i2c.c
 * @brief   BSP I2C implementation (board-level I2C0 bus)
 */

#include "bsp_i2c.h"
#include "bsp_pin_config.h"
#include "bsp_config.h"
#include "hal_i2c.h"

/* ========== API ========== */

hal_status_t bsp_i2c_init(void)
{
    hal_i2c_config_t cfg = {
        .instance     = BSP_I2C0_INST,
        .role         = HAL_I2C_ROLE_CONTROLLER,
        .speed        = HAL_I2C_SPEED_FAST,
        .own_address  = 0,
        .irq_priority = HAL_IRQ_PRIORITY_MEDIUM,
    };

    return hal_i2c_init(&cfg);
}

hal_status_t bsp_i2c0_write(uint16_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return hal_i2c_master_write(BSP_I2C0_INST, dev_addr, data, len, timeout_ms);
}

hal_status_t bsp_i2c0_read(uint16_t dev_addr, uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
    return hal_i2c_master_read(BSP_I2C0_INST, dev_addr, buf, len, timeout_ms);
}

hal_status_t bsp_i2c0_write_read(uint16_t dev_addr,
                                  const uint8_t *wr_data, uint16_t wr_len,
                                  uint8_t *rd_buf, uint16_t rd_len,
                                  uint32_t timeout_ms)
{
    return hal_i2c_master_write_read(BSP_I2C0_INST, dev_addr,
                                     wr_data, wr_len,
                                     rd_buf, rd_len,
                                     timeout_ms);
}
