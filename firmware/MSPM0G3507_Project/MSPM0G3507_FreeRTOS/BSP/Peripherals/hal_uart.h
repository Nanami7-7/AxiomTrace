/**
 * @file    hal_uart.h
 * @brief   HAL UART硬件抽象接口
 *
 * @note    封装DL_UART_Main_xxx调用，UART0已在SYSCFG_DL_init()中
 *          完成时钟/波特率/格式配置，此接口仅补充运行时操作
 */
#ifndef HAL_UART_H
#define HAL_UART_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "hal_common.h"

/* ======================== 函数接口 ======================== */

/**
 * @brief  使能UART中断
 * @note   SYSCFG_DL_init()仅配置了UART外设但未使能NVIC，
 *         需要中断接收时调用此函数使能NVIC中断
 * @param  id UART实例编号
 * @retval HAL_OK           使能成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_uart_enable_irq(hal_uart_id_t id);

/**
 * @brief  禁止UART中断
 * @param  id UART实例编号
 * @retval HAL_OK           禁止成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_uart_disable_irq(hal_uart_id_t id);

/**
 * @brief  发送单字节(阻塞等待,带超时)
 * @note   轮询等待UART空闲后发送，适用于调试打印等非实时场景
 *         RTOS模式下应避免在中断中调用
 * @param  id   UART实例编号
 * @param  data 待发送字节数据
 * @retval HAL_OK           发送成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 * @retval HAL_ERR_TIMEOUT 发送超时(UART硬件故障)
 */
hal_status_t hal_uart_transmit(hal_uart_id_t id, uint8_t data);

/**
 * @brief  发送多字节(阻塞等待)
 * @param  id   UART实例编号
 * @param  data 待发送数据缓冲区指针
 * @param  len  数据长度(字节)
 * @retval HAL_OK           发送成功
 * @retval HAL_ERR_INVALID_PARAM 参数无效
 */
hal_status_t hal_uart_transmit_buf(hal_uart_id_t id, const uint8_t *data,
                                    uint16_t len);

/**
 * @brief  接收单字节(非阻塞，从数据寄存器读取)
 * @note   通常在UART RX中断服务函数中调用
 * @param  id   UART实例编号
 * @param  data 接收数据存放指针
 * @retval HAL_OK           接收成功
 * @retval HAL_ERR_INVALID_PARAM 参数无效
 */
hal_status_t hal_uart_receive(hal_uart_id_t id, uint8_t *data);

/**
 * @brief  查询UART是否忙碌
 * @param  id UART实例编号
 * @retval true  UART正在发送/接收
 * @retval false UART空闲
 */
bool hal_uart_is_busy(hal_uart_id_t id);

/**
 * @brief  获取UART中断挂起标志
 * @note   在UART中断服务函数中调用，判断中断来源
 * @param  id UART实例编号
 * @retval hal_uart_irq_flag_t 中断标志
 */
hal_uart_irq_flag_t hal_uart_get_irq_flag(hal_uart_id_t id);

/**
 * @brief  启动DMA发送(非阻塞)
 * @note   使用DMA CH1(UART0 TX触发)，数据必须在DMA可访问内存中
 * @param  id   UART实例编号
 * @param  data 数据缓冲区指针
 * @param  len  数据长度(字节,最大65535)
 * @retval HAL_OK           启动成功
 * @retval HAL_ERR_BUSY     DMA正在发送
 * @retval HAL_ERR_INVALID_PARAM 参数无效
 */
hal_status_t hal_uart_transmit_dma(hal_uart_id_t id,
                                    const uint8_t *data, uint16_t len);

/**
 * @brief  停止DMA发送
 * @param  id UART实例编号
 * @retval HAL_OK 停止成功
 */
hal_status_t hal_uart_abort_tx_dma(hal_uart_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */
