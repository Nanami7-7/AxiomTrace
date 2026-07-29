/**
 * @file    bsp_debug.c
 * @brief   UART0 调试输出和调试命令接收实现。
 */
#include "bsp_debug.h"
#include "hal_uart.h"
#include "osal_api.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>

/* Arm Compiler 5 兼容代码；Arm Compiler 6/armclang 不需要该 pragma。 */
#if defined(__CC_ARM) && !defined(__clang__)
#pragma import(__use_no_semihosting)
#if !defined(__MICROLIB)
struct __FILE {
    int handle;
};
FILE __stdout;

/** 关闭半主机模式。 */
void _sys_exit(int status)
{
    (void)status;
    for (;;) {
    }
}
#endif
#endif

/** UART0 调试命令接收存储区。 */
static uint8_t s_rx_storage[BSP_DEBUG_RX_BUF_SIZE];

/** UART0 调试命令接收环形缓冲。 */
static bsp_ringbuf_t s_rx_ring;

/** UART0 接收功能是否已经初始化。 */
static bool s_is_inited;

/** 初始化 UART0 接收中断。 */
bsp_status_t bsp_debug_init(void)
{
    uint8_t stale;

    if (s_is_inited) {
        return BSP_OK;
    }

    bsp_ringbuf_init(&s_rx_ring, s_rx_storage, BSP_DEBUG_RX_BUF_SIZE);

    /* 丢弃初始化前可能残留的输入字节，避免误组成第一条命令。 */
    while (DL_UART_receiveDataCheck(UART_0_INST, &stale)) {
    }

    /* SysConfig 已配置 UART0 的 RX 中断，这里确保运行时保持开启。 */
    DL_UART_Main_enableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_RX);
    if (hal_uart_enable_irq(HAL_UART_0) != HAL_OK) {
        return BSP_ERR_HW_FAULT;
    }

    s_is_inited = true;
    return BSP_OK;
}

/** 非阻塞读取一个 UART0 字节。 */
bsp_status_t bsp_debug_getc(uint8_t *data)
{
    if (data == NULL) {
        return BSP_ERR_NULL_PTR;
    }
    if (!s_is_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if (bsp_ringbuf_get(&s_rx_ring, data)) {
        return BSP_OK;
    }

    /*
     * 额外保留一次硬件轮询兜底：如果某些 SysConfig/启动文件版本没有
     * 正确接通 UART0 NVIC，任务仍然可以直接读到 UART0 FIFO 中的字节。
     * 正常情况下优先使用中断环形缓冲，不影响原有实时接收路径。
     */
    if (DL_UART_receiveDataCheck(UART_0_INST, data)) {
        return BSP_OK;
    }

    return BSP_ERR_BUF_EMPTY;
}

/** 查询 UART0 接收缓冲区字节数。 */
uint32_t bsp_debug_available(void)
{
    return s_is_inited ? bsp_ringbuf_count(&s_rx_ring) : 0U;
}

/** 清空 UART0 接收缓冲区。 */
void bsp_debug_flush_rx(void)
{
    if (s_is_inited) {
        OSAL_CRITICAL_SECTION {
            bsp_ringbuf_flush(&s_rx_ring);
        }
    }
}

/**
 * @brief UART0 中断服务入口。
 * @note  中断中只收字节，不打印、不解析、不调用 FreeRTOS API。
 */
void bsp_debug_irq_handler(void)
{
    DL_UART_IIDX idx = DL_UART_getPendingInterrupt(UART_0_INST);

    if ((idx == DL_UART_IIDX_RX) ||
        (idx == DL_UART_IIDX_RX_TIMEOUT_ERROR)) {
        uint8_t data;
        while (DL_UART_receiveDataCheck(UART_0_INST, &data)) {
            if (s_is_inited) {
                (void)bsp_ringbuf_put(&s_rx_ring, data);
            }
        }
    }
}

/** printf 的 UART0 输出重定向。 */
int fputc(int ch, FILE *stream)
{
    (void)stream;
    DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t)ch);
    return ch;
}