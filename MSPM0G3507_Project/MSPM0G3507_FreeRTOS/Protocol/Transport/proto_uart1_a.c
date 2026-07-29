/**
 * @file    proto_uart1_a.c
 * @brief   Board A UART1 板间协议传输层实现。
 */
#include "proto_uart1_a.h"
#include "hal_uart.h"
#include "osal_api.h"
#include "ti_msp_dl_config.h"

static uint8_t s_rx_storage[PROTO_UART1_A_RX_BUF_SIZE];
static bsp_ringbuf_t s_rx_ring;
static volatile proto_uart1_a_diag_t s_diag;
static bool s_inited;

static bsp_status_t map_hal_status(hal_status_t status)
{
    switch (status) {
    case HAL_OK: return BSP_OK;
    case HAL_ERR_INVALID_PARAM: return BSP_ERR_INVALID_PARAM;
    case HAL_ERR_BUSY: return BSP_ERR_BUSY;
    case HAL_ERR_TIMEOUT: return BSP_ERR_TIMEOUT;
    case HAL_ERR_NOT_INIT: return BSP_ERR_NOT_INIT;
    case HAL_ERR_UNSUPPORTED: return BSP_ERR_UNSUPPORTED;
    default: return BSP_ERR_HW_FAULT;
    }
}

bsp_status_t proto_uart1_a_init(void)
{
    uint8_t stale;
    if (s_inited) return BSP_OK;
    bsp_ringbuf_init(&s_rx_ring, s_rx_storage, PROTO_UART1_A_RX_BUF_SIZE);
    while (DL_UART_receiveDataCheck(UART1_INST, &stale)) {
    }
    /* 打开 UART1 外设 RX 中断，再打开对应 NVIC。 */
    DL_UART_Main_enableInterrupt(UART1_INST, DL_UART_MAIN_INTERRUPT_RX);
    if (hal_uart_enable_irq(HAL_UART_EXT) != HAL_OK) {
        return BSP_ERR_HW_FAULT;
    }
    s_diag.rx_bytes = 0u;
    s_diag.rx_overflow = 0u;
    s_diag.irq_count = 0u;
    s_diag.ignored_irq_count = 0u;
    s_inited = true;
    return BSP_OK;
}

void proto_uart1_a_deinit(void)
{
    if (!s_inited) return;
    DL_UART_Main_disableInterrupt(UART1_INST, DL_UART_MAIN_INTERRUPT_RX);
    (void)hal_uart_disable_irq(HAL_UART_EXT);
    bsp_ringbuf_flush(&s_rx_ring);
    s_inited = false;
}

bsp_status_t proto_uart1_a_write(const uint8_t *data, uint16_t len)
{
    if (!s_inited) return BSP_ERR_NOT_INIT;
    if (data == NULL && len != 0u) return BSP_ERR_NULL_PTR;
    if (len == 0u) return BSP_OK;
    return map_hal_status(hal_uart_transmit_buf(HAL_UART_EXT, data, len));
}

bsp_status_t proto_uart1_a_getc(uint8_t *data)
{
    if (data == NULL) return BSP_ERR_NULL_PTR;
    if (!s_inited) return BSP_ERR_NOT_INIT;
    return bsp_ringbuf_get(&s_rx_ring, data) ? BSP_OK : BSP_ERR_BUF_EMPTY;
}

uint32_t proto_uart1_a_available(void)
{
    return s_inited ? bsp_ringbuf_count(&s_rx_ring) : 0u;
}

void proto_uart1_a_flush_rx(void)
{
    if (s_inited) {
        OSAL_CRITICAL_SECTION {
            bsp_ringbuf_flush(&s_rx_ring);
        }
    }
}

bsp_status_t proto_uart1_a_get_diag(proto_uart1_a_diag_t *diag)
{
    if (diag == NULL) return BSP_ERR_NULL_PTR;
    OSAL_CRITICAL_SECTION {
        *diag = s_diag;
    }
    return BSP_OK;
}

void proto_uart1_a_irq_handler(void)
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
        break;
    default:
        s_diag.ignored_irq_count++;
        break;
    }
}
