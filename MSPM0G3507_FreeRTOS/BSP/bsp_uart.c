/**
 * @file    bsp_uart.c
 * @brief   调试串口驱动实现
 * @note    ISR中仅将字节存入环形缓冲区，不做协议解析
 *         任务中通过bsp_uart_getc逐字节读取
 *         printf重定向需在应用层实现(app_main.c中重写fputc)
 */
#include "bsp_uart.h"
#include "hal_uart.h"
#include "osal_api.h"
#include "project_config.h"
#include "ti_msp_dl_config.h"
#include <string.h>

/* ======================== 私有变量 ======================== */

/** 接收环形缓冲区存储区 */
static uint8_t s_rx_buf[BSP_UART_RX_BUF_SIZE];

/** 接收环形缓冲区实例 */
static bsp_ringbuf_t s_rx_ringbuf;

/** 初始化标志 */
static bool s_is_inited = false;

/* ======================== DMA发送标志位 ======================== */

/** UART发送完成标志(EOT: FIFO空，可以发送新数据) */
static volatile bool s_uart_tx_complete = true;

/** DMA搬运完成标志(DMA_DONE_TX: 数据已复制到FIFO) */
static volatile bool s_uart_tx_dma_done = true;

/* ======================== DMA发送安全缓冲区 ======================== */

/** DMA发送bounce buffer：防止调用方在DMA完成前覆盖源数据 */
#define BSP_DMA_TX_BUF_SIZE     512U
static uint8_t s_dma_tx_buf[BSP_DMA_TX_BUF_SIZE];

/* ======================== 调试计数器 ======================== */
volatile uint32_t dbg_isr_count = 0;
volatile uint32_t dbg_uart_isr_total = 0;
volatile uint32_t dbg_iidx_value = 0;
volatile uint32_t dbg_eot_count = 0;
volatile uint32_t dbg_rx_count = 0;
volatile uint32_t dbg_other_count = 0;
volatile uint32_t dbg_rx_overflow = 0;

/* ======================== 函数实现 ======================== */

bsp_status_t bsp_uart_init(void)
{
    if (s_is_inited) {
        return BSP_OK;
    }

    /* 初始化接收环形缓冲区 */
    bsp_ringbuf_init(&s_rx_ringbuf, s_rx_buf, BSP_UART_RX_BUF_SIZE);

    /* 使能UART NVIC中断 — RX中断已在SysConfig中使能 */
    hal_uart_enable_irq(PRJ_UART_DEBUG_ID);

    /* 初始化DMA发送标志位 */
    s_uart_tx_complete = true;
    s_uart_tx_dma_done = true;

    s_is_inited = true;
    return BSP_OK;
}

void bsp_uart_deinit(void)
{
    if (!s_is_inited) {
        return;
    }

    hal_uart_disable_irq(PRJ_UART_DEBUG_ID);
    bsp_ringbuf_flush(&s_rx_ringbuf);
    s_is_inited = false;
}

bsp_status_t bsp_uart_putc(uint8_t data)
{
    hal_status_t ret = hal_uart_transmit(PRJ_UART_DEBUG_ID, data);
    return (HAL_OK == ret) ? BSP_OK : BSP_ERR_HW_FAULT;
}

bsp_status_t bsp_uart_puts(const char *str)
{
    if (NULL == str) {
        return BSP_ERR_NULL_PTR;
    }

    while ('\0' != *str) {
        if (BSP_OK != bsp_uart_putc((uint8_t)*str)) {
            return BSP_ERR_HW_FAULT;
        }
        str++;
    }
    return BSP_OK;
}

bsp_status_t bsp_uart_getc(uint8_t *data)
{
    if (NULL == data) {
        return BSP_ERR_NULL_PTR;
    }

    if (bsp_ringbuf_get(&s_rx_ringbuf, data)) {
        return BSP_OK;
    }
    return BSP_ERR_BUF_EMPTY;
}

uint32_t bsp_uart_rx_count(void)
{
    return bsp_ringbuf_count(&s_rx_ringbuf);
}

void bsp_uart_rx_flush(void)
{
    bsp_ringbuf_flush(&s_rx_ringbuf);
}

/* ======================== DMA发送实现 ======================== */

bsp_status_t bsp_uart_send_dma(const uint8_t *data, uint16_t len)
{
    if (data == NULL) {
        return BSP_ERR_NULL_PTR;
    }
    if (len == 0U) {
        return BSP_OK;
    }

    /* 非阻塞检查：上一次发送未完成，直接返回 */
    if (!s_uart_tx_complete) {
        return BSP_ERR_BUSY;
    }

    /* 清除标志位 */
    s_uart_tx_complete = false;
    s_uart_tx_dma_done = false;

    /* 拷贝到bounce buffer，防止调用方在DMA完成前覆盖源数据 */
    uint32_t src_addr;
    if (len <= BSP_DMA_TX_BUF_SIZE) {
        memcpy(s_dma_tx_buf, data, len);
        src_addr = (uint32_t)s_dma_tx_buf;
    } else {
        src_addr = (uint32_t)data;
    }

    /* 配置DMA传输：从bounce buffer发送 */
    DL_DMA_setSrcAddr(DMA, DMA_CH1_CHAN_ID, src_addr);
    DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID,
        (uint32_t)(&UART0->TXDATA));
    DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, (uint32_t)len);
    DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);

    return BSP_OK;
}

bool bsp_uart_tx_idle(void)
{
    return s_uart_tx_complete;
}

void bsp_uart_irq_handler(void)
{
    dbg_uart_isr_total++;
    DL_UART_IIDX idx = DL_UART_getPendingInterrupt(UART0);
    dbg_iidx_value = (uint32_t)idx;

    switch (idx) {
        case DL_UART_IIDX_DMA_DONE_TX:
            /* DMA搬运完成：数据已复制到UART FIFO */
            s_uart_tx_dma_done = true;
            dbg_isr_count++;
            break;

        case DL_UART_IIDX_EOT_DONE:
            /* 发送完成：UART FIFO空，可以发送新数据 */
            s_uart_tx_complete = true;
            dbg_eot_count++;
            break;

        case DL_UART_IIDX_RX:
        case DL_UART_IIDX_RX_TIMEOUT_ERROR:
            /* 接收中断：循环读取FIFO直到为空 */
            {
                uint8_t data;
                while (DL_UART_receiveDataCheck(UART0, &data)) {
                    if (!bsp_ringbuf_put(&s_rx_ringbuf, data)) {
                        dbg_rx_overflow++;
                        break;
                    }
                }
                dbg_rx_count++;
            }
            break;

        default:
            dbg_other_count++;
            break;
    }
}

void DMA_IRQHandler(void)
{
    /* 清除DMA中断标志，防止进入Default_Handler死循环 */
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL1); /* CH0 (RX) */
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0); /* CH1 (TX) */
}
