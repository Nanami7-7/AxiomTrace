/**
 * @file    hal_uart.c
 * @brief   HAL UART硬件抽象实现
 *
 * @note    封装DL_UART_Main_xxx调用，通过hal_uart_id_t枚举映射到
 *          UART_Regs*指针。UART0~UART3已在SysConfig中完成初始化。
 *          本文件适配M0_Base工程的4路UART配置。
 */
#include "hal_uart.h"
#include "ti_msp_dl_config.h"

/* ======================== 私有映射表 ======================== */

/**
 * @brief UART实例寄存器指针映射表
 * @note  顺序必须与hal_uart_id_t枚举一致
 */
static UART_Regs *const s_uart_inst_map[HAL_UART_COUNT] = {
    UART0,  /**< HAL_UART_0 / HAL_UART_DEBUG -> UART0 */
    UART1,  /**< HAL_UART_1 / HAL_UART_BLE   -> UART1 */
    UART2,  /**< HAL_UART_2                  -> UART2 */
    UART3,  /**< HAL_UART_3                  -> UART3 */
};

/**
 * @brief UART中断号映射表
 * @note  用于NVIC中断使能/禁止操作
 */
static const IRQn_Type s_uart_irq_map[HAL_UART_COUNT] = {
    UART0_INT_IRQn,  /**< HAL_UART_0 -> UART0中断 */
    UART1_INT_IRQn,  /**< HAL_UART_1 -> UART1中断 */
    UART2_INT_IRQn,  /**< HAL_UART_2 -> UART2中断 */
    UART3_INT_IRQn,  /**< HAL_UART_3 -> UART3中断 */
};

/* ======================== 私有常量 ======================== */

/** UART TX FIFO等待超时循环计数（兼容9600baud，防止硬件异常时永久阻塞） */
#define HAL_UART_TX_TIMEOUT  (1000000U)

/* ======================== 内联辅助函数 ======================== */

static inline bool is_uart_valid(hal_uart_id_t id)
{
    return ((uint32_t)id < HAL_UART_COUNT);
}

/**
 * @brief 执行函数 uart_to_regs，完成对应模块的功能处理。
 * @param id 函数参数 id。
 * @return 函数执行结果。
 */
static inline UART_Regs *uart_to_regs(hal_uart_id_t id)
{
    if (!is_uart_valid(id)) {
        return NULL;
    }
    return s_uart_inst_map[id];
}

/* ======================== 公共函数实现 ======================== */

/**
 * @brief 启用函数 hal_uart_enable_irq，完成对应模块的功能处理。
 * @param id 函数参数 id。
 * @return 函数执行结果。
 */
hal_status_t hal_uart_enable_irq(hal_uart_id_t id)
{
    if (!is_uart_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }
    NVIC_ClearPendingIRQ(s_uart_irq_map[id]);
    NVIC_EnableIRQ(s_uart_irq_map[id]);
    return HAL_OK;
}

/**
 * @brief 禁用函数 hal_uart_disable_irq，完成对应模块的功能处理。
 * @param id 函数参数 id。
 * @return 函数执行结果。
 */
hal_status_t hal_uart_disable_irq(hal_uart_id_t id)
{
    if (!is_uart_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }
    NVIC_DisableIRQ(s_uart_irq_map[id]);
    return HAL_OK;
}

/**
 * @brief 执行函数 hal_uart_transmit，完成对应模块的功能处理。
 * @param id 函数参数 id。
 * @param data 函数参数 data。
 * @return 函数执行结果。
 */
hal_status_t hal_uart_transmit(hal_uart_id_t id, uint8_t data)
{
    UART_Regs *regs = uart_to_regs(id);
    if (regs == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }

    uint32_t timeout = HAL_UART_TX_TIMEOUT;
    while (DL_UART_isTXFIFOFull(regs) == true) {
        if (timeout == 0U) {
            return HAL_ERR_TIMEOUT;
        }
        timeout--;
    }
    DL_UART_Main_transmitData(regs, data);
    return HAL_OK;
}

/**
 * @brief 执行函数 hal_uart_transmit_buf，完成对应模块的功能处理。
 * @param id 函数参数 id。
 * @param data 函数参数 data。
 * @param len 函数参数 len。
 * @return 函数执行结果。
 */
hal_status_t hal_uart_transmit_buf(hal_uart_id_t id, const uint8_t *data,
                                    uint16_t len)
{
    if (data == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    if (len == 0U) {
        return HAL_OK;
    }
    if (!is_uart_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    for (uint16_t i = 0; i < len; i++) {
        hal_status_t ret = hal_uart_transmit(id, data[i]);
        if (ret != HAL_OK) {
            return ret;
        }
    }
    return HAL_OK;
}

/**
 * @brief 接收函数 hal_uart_receive，完成对应模块的功能处理。
 * @param id 函数参数 id。
 * @param data 函数参数 data。
 * @return 函数执行结果。
 */
hal_status_t hal_uart_receive(hal_uart_id_t id, uint8_t *data)
{
    UART_Regs *regs = uart_to_regs(id);
    if (regs == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    if (data == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    *data = (uint8_t)DL_UART_Main_receiveData(regs);
    return HAL_OK;
}

/**
 * @brief 查询忙状态函数 hal_uart_is_busy，完成对应模块的功能处理。
 * @param id 函数参数 id。
 * @return 函数执行结果。
 */
bool hal_uart_is_busy(hal_uart_id_t id)
{
    UART_Regs *regs = uart_to_regs(id);
    if (regs == NULL) {
        return false;
    }
    return DL_UART_isBusy(regs);
}

/**
 * @brief 获取函数 hal_uart_get_irq_flag，完成对应模块的功能处理。
 * @param id 函数参数 id。
 * @return 函数执行结果。
 */
hal_uart_irq_flag_t hal_uart_get_irq_flag(hal_uart_id_t id)
{
    UART_Regs *regs = uart_to_regs(id);
    if (regs == NULL) {
        return HAL_UART_IRQ_NONE;
    }
    DL_UART_IIDX idx = DL_UART_getPendingInterrupt(regs);
    if (idx == DL_UART_IIDX_RX) {
        return HAL_UART_IRQ_RX;
    }
    return HAL_UART_IRQ_NONE;
}

/* ======================== DMA TX 桩函数 ======================== */
/* M0_Base工程未配置DMA，提供桩函数以兼容接口声明 */

/**
 * @brief 执行函数 hal_uart_transmit_dma，完成对应模块的功能处理。
 * @param id 函数参数 id。
 * @param data 函数参数 data。
 * @param len 函数参数 len。
 * @return 函数执行结果。
 */
hal_status_t hal_uart_transmit_dma(hal_uart_id_t id,
                                    const uint8_t *data, uint16_t len)
{
    (void)id; (void)data; (void)len;
    return HAL_ERR_UNSUPPORTED;
}

/**
 * @brief 执行函数 hal_uart_abort_tx_dma，完成对应模块的功能处理。
 * @param id 函数参数 id。
 * @return 函数执行结果。
 */
hal_status_t hal_uart_abort_tx_dma(hal_uart_id_t id)
{
    (void)id;
    return HAL_ERR_UNSUPPORTED;
}