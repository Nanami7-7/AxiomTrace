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

/* ======================== 私有变量 ======================== */

/** 接收环形缓冲区存储区 */
static uint8_t s_rx_buf[BSP_UART_RX_BUF_SIZE];

/** 接收环形缓冲区实例 */
static bsp_ringbuf_t s_rx_ringbuf;

/** 初始化标志 */
static bool s_is_inited = false;

/* ======================== 函数实现 ======================== */

bsp_status_t bsp_uart_init(void)
{
    if (s_is_inited) {
        return BSP_OK;
    }

    /* 初始化环形缓冲区 */
    bsp_ringbuf_init(&s_rx_ringbuf, s_rx_buf,
                     BSP_UART_RX_BUF_SIZE);

    /* 使能UART中断(NVIC) — SYSCFG_DL_init()未做此步 */
    hal_uart_enable_irq(PRJ_UART_DEBUG_ID);

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

void bsp_uart_irq_handler(void)
{
    hal_uart_irq_flag_t flag =
        hal_uart_get_irq_flag(PRJ_UART_DEBUG_ID);

    if (HAL_UART_IRQ_RX == flag) {
        uint8_t data;
        if (HAL_OK == hal_uart_receive(PRJ_UART_DEBUG_ID, &data)) {
            /* ISR中仅存入缓冲区，丢弃满时的数据 */
            (void)bsp_ringbuf_put(&s_rx_ringbuf, data);
        }
    }
}
