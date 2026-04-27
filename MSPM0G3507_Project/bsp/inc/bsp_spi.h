/**
 * @file    bsp_spi.h
 * @brief   BSP SPI interface (board-level SPI devices)
 */

#ifndef BSP_SPI_H
#define BSP_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ========== API ========== */

hal_status_t bsp_spi_init(void);

hal_status_t bsp_spi0_transfer(const uint8_t *tx_data, uint8_t *rx_data, uint16_t len, uint32_t timeout_ms);
hal_status_t bsp_spi0_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms);
hal_status_t bsp_spi0_read(uint8_t *buf, uint16_t len, uint32_t timeout_ms);

/* CS control */
hal_status_t bsp_spi0_cs_low(void);
hal_status_t bsp_spi0_cs_high(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SPI_H */
