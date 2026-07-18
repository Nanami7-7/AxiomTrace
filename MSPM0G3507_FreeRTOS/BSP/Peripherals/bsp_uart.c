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
static uint8_t s_dma_tx_buf[BSP_UART_DMA_TX_MAX_LEN];

/*
 * Polling-byte and DMA-frame transmission share UART0 and its EOT event.
 * The bounded wait preserves the previous fputc failure behavior: debug
 * output must not block a control task indefinitely.
 */
#define BSP_UART_TX_WAIT_SPIN_MAX    80000U

/* ======================== UART TX 运行时诊断 ======================== */

/**
 * @brief UART DMA TX 只读诊断计数器
 * @note  该对象不参与发送准入、DMA 配置或控制决策。
 *        ISR 与多个任务会更新其字段；计数仅用于 best-effort 观测。
 */
static volatile bsp_uart_tx_diag_t s_uart_tx_diag;

/* ======================== 调试统计计数器 ======================== */

/**
 * @brief 启用 UART ISR 调试统计计数器
 * @note  默认关闭以节省 Flash/RAM; 调试 UART 异常时设为 1 启用
 *        启用后可通过读取 dbg_* 变量分析 ISR 触发分布
 */
#ifndef UART_DEBUG_STATS
#define UART_DEBUG_STATS  0
#endif

#if UART_DEBUG_STATS
/** ISR 进入次数(含所有中断源) */
volatile uint32_t dbg_isr_count = 0;
/** ISR 总进入次数 */
volatile uint32_t dbg_uart_isr_total = 0;
/** 最近一次 IIDX 值(用于排查异常中断源) */
volatile uint32_t dbg_iidx_value = 0;
/** EOT(发送完成) 事件计数 */
volatile uint32_t dbg_eot_count = 0;
/** RX 事件计数 */
volatile uint32_t dbg_rx_count = 0;
/** 其他(未识别) 事件计数 */
volatile uint32_t dbg_other_count = 0;
/** RX 环形缓冲区溢出计数 */
volatile uint32_t dbg_rx_overflow = 0;
#endif /* UART_DEBUG_STATS */

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
    uint32_t timeout = BSP_UART_TX_WAIT_SPIN_MAX;

    /*
     * A polling byte may enter TXDATA only after the preceding UART
     * transaction reached EOT.  The critical section covers only the atomic
     * check -> claim -> register-write operation; it never waits with
     * interrupts masked, so EOT delivery cannot be deadlocked.
     */
    while (timeout-- > 0U) {
        bool accepted = false;

        OSAL_CRITICAL_SECTION {
            if (s_uart_tx_complete) {
                s_uart_tx_complete = false;
                DL_UART_Main_transmitData(UART_0_DEBUG_INST, data);
                accepted = true;
            }
        }

        if (accepted) {
            return BSP_OK;
        }
    }

    /* Bounded failure: diagnostic output may be dropped, never block forever. */
    return BSP_ERR_BUSY;
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
    bool accepted = false;

    if (data == NULL) {
        s_uart_tx_diag.null_reject_count++;
        return BSP_ERR_NULL_PTR;
    }
    if (len == 0U) {
        s_uart_tx_diag.zero_length_count++;
        return BSP_OK;
    }
    if (len > BSP_UART_DMA_TX_MAX_LEN) {
        /* Never DMA directly from caller-owned storage: its lifetime is unknown. */
        s_uart_tx_diag.oversize_submit_count++;
        return BSP_ERR_INVALID_PARAM;
    }

    /*
     * Share the same admission flag as bsp_uart_putc/fputc.  Bounce-buffer
     * copying, DMA programming, and channel enable form one critical
     * transaction so two producers cannot reuse the single buffer or
     * reconfigure DMA while a polling byte owns UART0.
     */
    OSAL_CRITICAL_SECTION {
        if (!s_uart_tx_complete) {
            s_uart_tx_diag.busy_reject_count++;
        } else {
            uint32_t src_addr;

            s_uart_tx_complete = false;
            s_uart_tx_dma_done = false;

            memcpy(s_dma_tx_buf, data, len);
            src_addr = (uint32_t)s_dma_tx_buf;

            DL_DMA_setSrcAddr(DMA, DMA_CH1_CHAN_ID, src_addr);
            DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID,
                (uint32_t)(&UART0->TXDATA));
            DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, (uint32_t)len);
            DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);

            s_uart_tx_diag.dma_start_count++;
            s_uart_tx_diag.last_submit_len = len;
            if (len > s_uart_tx_diag.max_submit_len) {
                s_uart_tx_diag.max_submit_len = len;
            }
            accepted = true;
        }
    }

    return accepted ? BSP_OK : BSP_ERR_BUSY;
}

bool bsp_uart_tx_idle(void)
{
    return s_uart_tx_complete;
}

bsp_status_t bsp_uart_get_tx_diag(bsp_uart_tx_diag_t *diag)
{
    if (diag == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    /* 仅在任务上下文使用：确保读取到字段一致的快照。 */
    OSAL_CRITICAL_SECTION {
        diag->dma_start_count = s_uart_tx_diag.dma_start_count;
        diag->dma_done_count = s_uart_tx_diag.dma_done_count;
        diag->eot_done_count = s_uart_tx_diag.eot_done_count;
        diag->busy_reject_count = s_uart_tx_diag.busy_reject_count;
        diag->null_reject_count = s_uart_tx_diag.null_reject_count;
        diag->zero_length_count = s_uart_tx_diag.zero_length_count;
        diag->oversize_submit_count = s_uart_tx_diag.oversize_submit_count;
        diag->last_submit_len = s_uart_tx_diag.last_submit_len;
        diag->max_submit_len = s_uart_tx_diag.max_submit_len;
    }

    return BSP_OK;
}

void bsp_uart_irq_handler(void)
{
#if UART_DEBUG_STATS
    dbg_uart_isr_total++;
    DL_UART_IIDX idx = DL_UART_getPendingInterrupt(UART0);
    dbg_iidx_value = (uint32_t)idx;
#else
    DL_UART_IIDX idx = DL_UART_getPendingInterrupt(UART0);
#endif

    switch (idx) {
        case DL_UART_IIDX_DMA_DONE_TX:
            /* DMA搬运完成：数据已复制到UART FIFO */
            s_uart_tx_dma_done = true;
            s_uart_tx_diag.dma_done_count++;
#if UART_DEBUG_STATS
            dbg_isr_count++;
#endif
            break;

        case DL_UART_IIDX_EOT_DONE:
            /* 发送完成：UART FIFO空，可以发送新数据 */
            s_uart_tx_complete = true;
            s_uart_tx_diag.eot_done_count++;
#if UART_DEBUG_STATS
            dbg_eot_count++;
#endif
            break;

        case DL_UART_IIDX_RX:
        case DL_UART_IIDX_RX_TIMEOUT_ERROR:
            /* 接收中断：循环读取FIFO直到为空 */
            {
                uint8_t data;
                while (DL_UART_receiveDataCheck(UART0, &data)) {
                    if (!bsp_ringbuf_put(&s_rx_ringbuf, data)) {
#if UART_DEBUG_STATS
                        dbg_rx_overflow++;
#endif
                        break;
                    }
                }
#if UART_DEBUG_STATS
                dbg_rx_count++;
#endif
            }
            break;

        default:
#if UART_DEBUG_STATS
            dbg_other_count++;
#endif
            break;
    }
}

void DMA_IRQHandler(void)
{
    /* 清除DMA中断标志，防止进入Default_Handler死循环 */
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL1); /* CH1 (TX) */
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0); /* CH0 (RX) */
}
