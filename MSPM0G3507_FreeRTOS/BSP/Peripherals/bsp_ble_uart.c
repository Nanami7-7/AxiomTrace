/**
 * @file    bsp_ble_uart.c
 * @brief   Independent UART1 transport used by the JDY-23 BLE module.
 */
#include "bsp_ble_uart.h"
#include "hal_uart.h"
#include "osal_api.h"
#include "project_config.h"
#include "ti_msp_dl_config.h"

static uint8_t s_rx_storage[BSP_BLE_UART_RX_BUF_SIZE];
static bsp_ringbuf_t s_rx_ring;
static bool s_is_inited;
static volatile bsp_ble_uart_diag_t s_diag;

static bsp_status_t map_hal_status(hal_status_t status)
{
    switch (status) {
    case HAL_OK:
        return BSP_OK;
    case HAL_ERR_INVALID_PARAM:
        return BSP_ERR_INVALID_PARAM;
    case HAL_ERR_BUSY:
        return BSP_ERR_BUSY;
    case HAL_ERR_TIMEOUT:
        return BSP_ERR_TIMEOUT;
    case HAL_ERR_NOT_INIT:
        return BSP_ERR_NOT_INIT;
    case HAL_ERR_UNSUPPORTED:
        return BSP_ERR_UNSUPPORTED;
    default:
        return BSP_ERR_HW_FAULT;
    }
}

bsp_status_t bsp_ble_uart_init(void)
{
    uint8_t stale;
    hal_status_t hal_ret;

    if (s_is_inited) {
        return BSP_OK;
    }

    bsp_ringbuf_init(&s_rx_ring, s_rx_storage, BSP_BLE_UART_RX_BUF_SIZE);
    while (DL_UART_receiveDataCheck(UART1_INST, &stale)) {
        /* Discard bytes received before the software consumer was ready. */
    }

    hal_ret = hal_uart_enable_irq(PRJ_UART_BLE_ID);
    if (hal_ret != HAL_OK) {
        return map_hal_status(hal_ret);
    }

    s_is_inited = true;
    return BSP_OK;
}

void bsp_ble_uart_deinit(void)
{
    if (!s_is_inited) {
        return;
    }

    (void)hal_uart_disable_irq(PRJ_UART_BLE_ID);
    bsp_ringbuf_flush(&s_rx_ring);
    s_is_inited = false;
}

bsp_status_t bsp_ble_uart_write(const uint8_t *data, uint16_t len)
{
    if (!s_is_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if ((data == NULL) && (len != 0U)) {
        return BSP_ERR_NULL_PTR;
    }
    if (len == 0U) {
        return BSP_OK;
    }

    return map_hal_status(hal_uart_transmit_buf(PRJ_UART_BLE_ID, data, len));
}

bsp_status_t bsp_ble_uart_getc(uint8_t *data)
{
    if (data == NULL) {
        return BSP_ERR_NULL_PTR;
    }
    if (!s_is_inited) {
        return BSP_ERR_NOT_INIT;
    }

    return bsp_ringbuf_get(&s_rx_ring, data) ? BSP_OK : BSP_ERR_BUF_EMPTY;
}

uint32_t bsp_ble_uart_available(void)
{
    return s_is_inited ? bsp_ringbuf_count(&s_rx_ring) : 0U;
}

void bsp_ble_uart_flush_rx(void)
{
    if (s_is_inited) {
        OSAL_CRITICAL_SECTION {
            bsp_ringbuf_flush(&s_rx_ring);
        }
    }
}

bsp_status_t bsp_ble_uart_get_diag(bsp_ble_uart_diag_t *diag)
{
    if (diag == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    OSAL_CRITICAL_SECTION {
        diag->rx_bytes = s_diag.rx_bytes;
        diag->rx_overflow = s_diag.rx_overflow;
        diag->irq_count = s_diag.irq_count;
        diag->ignored_irq_count = s_diag.ignored_irq_count;
    }
    return BSP_OK;
}

void bsp_ble_uart_irq_handler(void)
{
    DL_UART_IIDX idx = DL_UART_getPendingInterrupt(UART1_INST);

    s_diag.irq_count++;
    switch (idx) {
    case DL_UART_IIDX_RX:
    case DL_UART_IIDX_RX_TIMEOUT_ERROR:
        {
            uint8_t data;
            while (DL_UART_receiveDataCheck(UART1_INST, &data)) {
                if (bsp_ringbuf_put(&s_rx_ring, data)) {
                    s_diag.rx_bytes++;
                } else {
                    s_diag.rx_overflow++;
                }
            }
        }
        break;

    case DL_UART_IIDX_DMA_DONE_TX:
    case DL_UART_IIDX_EOT_DONE:
        /* UART1 TX is polling in this first integration; these are harmless. */
        break;

    default:
        s_diag.ignored_irq_count++;
        break;
    }
}
