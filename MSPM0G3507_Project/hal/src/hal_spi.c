/**
 * @file    hal_spi.c
 * @brief   SPI HAL implementation for MSPM0G3507
 */

#include "hal_conf.h"

#ifdef HAL_SPI_MODULE_ENABLED

#include "hal_spi.h"
#include "hal_common.h"

/* ---------- Maximum SPI instances ---------- */
#define HAL_SPI_MAX_INSTANCES  2

/* ---------- Callback storage ---------- */
typedef struct {
    hal_spi_rx_cb_t rx_cb;
    hal_spi_tx_cb_t tx_cb;
} hal_spi_callbacks_t;

static hal_spi_callbacks_t s_spi_callbacks[HAL_SPI_MAX_INSTANCES];

/* ---------- Helper: get instance index ---------- */
static uint32_t spi_get_idx(uint32_t instance)
{
    return (instance == SPI_0_INST) ? 0 : 1;
}

/* ========== API Implementation ========== */

hal_status_t hal_spi_init(const hal_spi_config_t *config)
{
    HAL_CHECK_PTR(config);

    DL_SPI_InitTypeDef spi_init = {0};

    /* Role */
    spi_init.mode = (config->role == HAL_SPI_ROLE_CONTROLLER)
                    ? DL_SPI_MODE_CONTROLLER
                    : DL_SPI_MODE_PERIPHERAL;

    /* Clock mode (CPOL/CPHA) */
    switch (config->mode) {
    case HAL_SPI_MODE_0: spi_init.clockMode = DL_SPI_CLOCK_MODE_0; break;
    case HAL_SPI_MODE_1: spi_init.clockMode = DL_SPI_CLOCK_MODE_1; break;
    case HAL_SPI_MODE_2: spi_init.clockMode = DL_SPI_CLOCK_MODE_2; break;
    case HAL_SPI_MODE_3: spi_init.clockMode = DL_SPI_CLOCK_MODE_3; break;
    default: return HAL_INVALID;
    }

    /* Data size */
    spi_init.dataSize = (config->data_size == HAL_SPI_DATASIZE_16)
                        ? DL_SPI_DATA_SIZE_16
                        : DL_SPI_DATA_SIZE_8;

    /* Bit order */
    spi_init.bitOrder = (config->bit_order == HAL_SPI_BITORDER_LSB)
                        ? DL_SPI_BIT_ORDER_LSB
                        : DL_SPI_BIT_ORDER_MSB;

    /* Configure clock divider based on target frequency */
    spi_init.clockDiv = DL_SPI_CLOCK_DIV_1; /* Simplified; compute from config->clock_freq */

    DL_SPI_init(config->instance, &spi_init);
    DL_SPI_enable(config->instance);

    /* Enable interrupts */
    uint32_t irqn = (config->instance == SPI_0_INST) ? SPI0_INT_IRQn : SPI1_INT_IRQn;
    NVIC_SetPriority(irqn, config->irq_priority);
    NVIC_EnableIRQ(irqn);

    return HAL_OK;
}

hal_status_t hal_spi_deinit(uint32_t instance)
{
    DL_SPI_disable(instance);
    DL_SPI_reset(instance);
    return HAL_OK;
}

hal_status_t hal_spi_transfer(uint32_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len, uint32_t timeout_ms)
{
    HAL_CHECK_PTR(tx_data);

    for (uint16_t i = 0; i < len; i++) {
        /* Timeout-protected wait for SPI not busy */
        uint32_t start = hal_get_tick();
        while (DL_SPI_isBusy(instance)) {
            if (hal_timeout_expired(start, timeout_ms)) {
                return HAL_TIMEOUT;
            }
        }

        DL_SPI_transmitData(instance, tx_data[i]);

        /* Timeout-protected wait for RX data ready */
        start = hal_get_tick();
        while (DL_SPI_isRXFIFOEmpty(instance)) {
            if (hal_timeout_expired(start, timeout_ms)) {
                return HAL_TIMEOUT;
            }
        }

        uint16_t rx_val = DL_SPI_receiveData(instance);
        if (rx_data != NULL) {
            rx_data[i] = (uint8_t)rx_val;
        }
    }
    return HAL_OK;
}

hal_status_t hal_spi_write(uint32_t instance, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return hal_spi_transfer(instance, data, NULL, len, timeout_ms);
}

hal_status_t hal_spi_read(uint32_t instance, uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
    HAL_CHECK_PTR(buf);

    static const uint8_t dummy_tx = 0xFF;
    for (uint16_t i = 0; i < len; i++) {
        hal_status_t status = hal_spi_transfer(instance, &dummy_tx, &buf[i], 1, timeout_ms);
        if (status != HAL_OK) {
            return status;
        }
    }
    return HAL_OK;
}

hal_status_t hal_spi_register_rx_callback(uint32_t instance, hal_spi_rx_cb_t cb)
{
    uint32_t idx = spi_get_idx(instance);
    if (idx >= HAL_SPI_MAX_INSTANCES) return HAL_INVALID;
    s_spi_callbacks[idx].rx_cb = cb;
    return HAL_OK;
}

hal_status_t hal_spi_register_tx_callback(uint32_t instance, hal_spi_tx_cb_t cb)
{
    uint32_t idx = spi_get_idx(instance);
    if (idx >= HAL_SPI_MAX_INSTANCES) return HAL_INVALID;
    s_spi_callbacks[idx].tx_cb = cb;
    return HAL_OK;
}

#endif /* HAL_SPI_MODULE_ENABLED */
