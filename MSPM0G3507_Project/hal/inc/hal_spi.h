/**
 * @file    hal_spi.h
 * @brief   SPI hardware abstraction layer interface
 */

#ifndef HAL_SPI_H
#define HAL_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- SPI mode (clock polarity / phase) ---------- */
typedef enum {
    HAL_SPI_MODE_0 = 0,  /* CPOL=0, CPHA=0 */
    HAL_SPI_MODE_1 = 1,  /* CPOL=0, CPHA=1 */
    HAL_SPI_MODE_2 = 2,  /* CPOL=1, CPHA=0 */
    HAL_SPI_MODE_3 = 3,  /* CPOL=1, CPHA=1 */
} hal_spi_mode_t;

/* ---------- SPI bit order ---------- */
typedef enum {
    HAL_SPI_BITORDER_MSB = 0,
    HAL_SPI_BITORDER_LSB = 1,
} hal_spi_bitorder_t;

/* ---------- SPI role ---------- */
typedef enum {
    HAL_SPI_ROLE_CONTROLLER = 0,
    HAL_SPI_ROLE_PERIPHERAL = 1,
} hal_spi_role_t;

/* ---------- SPI data size ---------- */
typedef enum {
    HAL_SPI_DATASIZE_8  = 8,
    HAL_SPI_DATASIZE_16 = 16,
} hal_spi_datasize_t;

/* ---------- SPI configuration ---------- */
typedef struct {
    uint32_t            instance;      /* SPI_0_INST / SPI_1_INST */
    hal_spi_role_t      role;
    hal_spi_mode_t      mode;
    hal_spi_datasize_t  data_size;
    hal_spi_bitorder_t  bit_order;
    uint32_t            clock_freq;    /* Target SCLK frequency in Hz */
    hal_irq_priority_t  irq_priority;
} hal_spi_config_t;

/* ---------- SPI callbacks ---------- */
typedef void (*hal_spi_rx_cb_t)(uint32_t instance);
typedef void (*hal_spi_tx_cb_t)(uint32_t instance);

/* ========== API ========== */

hal_status_t hal_spi_init(const hal_spi_config_t *config);
hal_status_t hal_spi_deinit(uint32_t instance);

hal_status_t hal_spi_transfer(uint32_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len, uint32_t timeout_ms);
hal_status_t hal_spi_write(uint32_t instance, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
hal_status_t hal_spi_read(uint32_t instance, uint8_t *buf, uint16_t len, uint32_t timeout_ms);

hal_status_t hal_spi_register_rx_callback(uint32_t instance, hal_spi_rx_cb_t cb);
hal_status_t hal_spi_register_tx_callback(uint32_t instance, hal_spi_tx_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SPI_H */
