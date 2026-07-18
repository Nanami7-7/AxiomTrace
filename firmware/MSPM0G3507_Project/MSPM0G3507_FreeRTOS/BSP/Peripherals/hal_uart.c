/**
 * @file    hal_uart.c
 * @brief   HAL UART硬件抽象实现
 *
 * @note    封装DL_UART_Main_xxx调用，通过hal_uart_id_t枚举映射到
 *          UART_Regs*指针，UART0已在SysConfig中完成初始化
 */
#include "hal_uart.h"
#include "ti_msp_dl_config.h"

/* ======================== 私有映射表 ======================== */

/**
 * @brief UART实例寄存器指针映射表
 * @note  顺序必须与hal_uart_id_t枚举一致
 */
static UART_Regs *const s_uart_inst_map[HAL_UART_COUNT] = {
    UART0,  /**< HAL_UART_DEBUG -> UART0 */
};

/**
 * @brief UART中断号映射表
 * @note  用于NVIC中断使能/禁止操作
 */
static const IRQn_Type s_uart_irq_map[HAL_UART_COUNT] = {
    UART0_INT_IRQn,  /**< HAL_UART_DEBUG -> UART0中断 */
};

/* ======================== 私有常量 ======================== */

/** UART发送超时循环计数(115200baud@80MHz约7000周期,留余量) */
#define HAL_UART_TX_TIMEOUT  (10000U)

/* ======================== 内联辅助函数 ======================== */

/**
 * @brief  检查UART实例枚举是否合法
 * @param  id UART实例枚举值
 * @retval true  合法
 * @retval false 越界
 */
static inline bool is_uart_valid(hal_uart_id_t id)
{
    return ((uint32_t)id < HAL_UART_COUNT);
}

/**
 * @brief  将HAL UART枚举转换为SDK UART_Regs指针
 * @param  id HAL UART枚举值
 * @retval UART_Regs* 指针，越界返回NULL
 */
static inline UART_Regs *uart_to_regs(hal_uart_id_t id)
{
    if (!is_uart_valid(id)) {
        return NULL;
    }
    return s_uart_inst_map[id];
}

/* ======================== 公共函数实现 ======================== */

hal_status_t hal_uart_enable_irq(hal_uart_id_t id)
{
    if (!is_uart_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /* 清除挂起的中断标志后再使能NVIC */
   // NVIC_ClearPendingIRQ(s_uart_irq_map[id]);
    NVIC_EnableIRQ(s_uart_irq_map[id]);

    return HAL_OK;
}

hal_status_t hal_uart_disable_irq(hal_uart_id_t id)
{
    if (!is_uart_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    NVIC_DisableIRQ(s_uart_irq_map[id]);
    return HAL_OK;
}

hal_status_t hal_uart_transmit(hal_uart_id_t id, uint8_t data)
{
    UART_Regs *regs = uart_to_regs(id);
    if (regs == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }

    /* 阻塞等待UART空闲后发送单字节(带超时) */
    uint32_t timeout = HAL_UART_TX_TIMEOUT;

    while (DL_UART_isBusy(regs) == true) {
        if (timeout == 0U) {
            return HAL_ERR_TIMEOUT;
        }
        timeout--;
    }
    DL_UART_Main_transmitData(regs, data);

    return HAL_OK;
}

hal_status_t hal_uart_transmit_buf(hal_uart_id_t id, const uint8_t *data,
                                    uint16_t len)
{
    /* 参数校验 */
    if (data == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    if (len == 0U) {
        return HAL_OK;
    }
    if (!is_uart_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /* 逐字节发送(检查每次返回值) */
    for (uint16_t i = 0; i < len; i++) {
        hal_status_t ret = hal_uart_transmit(id, data[i]);
        if (ret != HAL_OK) {
            return ret;
        }
    }

    return HAL_OK;
}

hal_status_t hal_uart_receive(hal_uart_id_t id, uint8_t *data)
{
    UART_Regs *regs = uart_to_regs(id);

    /* 参数校验 */
    if (regs == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    if (data == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }

    /* 从UART数据寄存器读取接收到的字节 */
    *data = (uint8_t)DL_UART_Main_receiveData(regs);

    return HAL_OK;
}

bool hal_uart_is_busy(hal_uart_id_t id)
{
    UART_Regs *regs = uart_to_regs(id);
    if (regs == NULL) {
        return false;
    }

    return DL_UART_isBusy(regs);
}

hal_uart_irq_flag_t hal_uart_get_irq_flag(hal_uart_id_t id)
{
    UART_Regs *regs = uart_to_regs(id);
    if (regs == NULL) {
        return HAL_UART_IRQ_NONE;
    }

    /*
     * 读取UART中断挂起标志
     * DL_UART_getPendingInterrupt返回DL_UART_IIDX枚举
     * RX中断返回DL_UART_IIDX_RX
     */
    DL_UART_IIDX idx = DL_UART_getPendingInterrupt(regs);

    if (idx == DL_UART_IIDX_RX) {
        return HAL_UART_IRQ_RX;
    }

    return HAL_UART_IRQ_NONE;
}

/* ======================== DMA TX 实现 ======================== */

hal_status_t hal_uart_transmit_dma(hal_uart_id_t id,
                                    const uint8_t *data, uint16_t len)
{
    UART_Regs *regs = uart_to_regs(id);
    if (regs == NULL || data == NULL || len == 0U) {
        return HAL_ERR_INVALID_PARAM;
    }

    /* 配置DMA传输: 源=缓冲区, 目标=UART TX寄存器 */
    DL_DMA_setSrcAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)data);
    DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID,
        (uint32_t)&(regs->TXDATA));
    DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, (uint32_t)len);

    /* 启动DMA通道 */
    DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);

    return HAL_OK;
}

hal_status_t hal_uart_abort_tx_dma(hal_uart_id_t id)
{
    (void)id;
    DL_DMA_disableChannel(DMA, DMA_CH1_CHAN_ID);
    return HAL_OK;
}
