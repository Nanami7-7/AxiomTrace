/**
 * @file    bsp_spi.c
 * @brief   BSP SPI implementation (board-level SPI0 bus)
 */

#include "bsp_spi.h"
#include "bsp_pin_config.h"
#include "bsp_config.h"
#include "hal_spi.h"
#include "hal_gpio.h"

/* ========== API ========== */

hal_status_t bsp_spi_init(void)
{
    hal_spi_config_t cfg = {
        .instance     = BSP_SPI0_INST,
        .role         = HAL_SPI_ROLE_CONTROLLER,
        .mode         = HAL_SPI_MODE_0,
        .data_size    = HAL_SPI_DATASIZE_8,
        .bit_order    = HAL_SPI_BITORDER_MSB,
        .clock_freq   = BSP_SPI0_CLOCK_FREQ_HZ,
        .irq_priority = HAL_IRQ_PRIORITY_MEDIUM,
    };

    hal_status_t status = hal_spi_init(&cfg);

    /* Initialize CS pin as GPIO output (active low) */
    if (status == HAL_OK) {
        hal_gpio_config_t cs_cfg = {
            .port         = BSP_SPI0_CS_PORT,
            .pin          = BSP_SPI0_CS_PIN,
            .mode         = HAL_GPIO_MODE_OUTPUT,
            .pull         = HAL_GPIO_PULL_NONE,
            .irq_trigger  = HAL_GPIO_IRQ_TRIGGER_NONE,
            .irq_priority = HAL_IRQ_PRIORITY_LOW,
        };
        status = hal_gpio_init(&cs_cfg);
        /* Default CS high (deselected) */
        hal_gpio_write_pin(BSP_SPI0_CS_PORT, BSP_SPI0_CS_PIN, HAL_GPIO_PIN_HIGH);
    }

    return status;
}

hal_status_t bsp_spi0_transfer(const uint8_t *tx_data, uint8_t *rx_data, uint16_t len, uint32_t timeout_ms)
{
    return hal_spi_transfer(BSP_SPI0_INST, tx_data, rx_data, len, timeout_ms);
}

hal_status_t bsp_spi0_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return hal_spi_write(BSP_SPI0_INST, data, len, timeout_ms);
}

hal_status_t bsp_spi0_read(uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
    return hal_spi_read(BSP_SPI0_INST, buf, len, timeout_ms);
}

hal_status_t bsp_spi0_cs_low(void)
{
    return hal_gpio_write_pin(BSP_SPI0_CS_PORT, BSP_SPI0_CS_PIN, HAL_GPIO_PIN_LOW);
}

hal_status_t bsp_spi0_cs_high(void)
{
    return hal_gpio_write_pin(BSP_SPI0_CS_PORT, BSP_SPI0_CS_PIN, HAL_GPIO_PIN_HIGH);
}
