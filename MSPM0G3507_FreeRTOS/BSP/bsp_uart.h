/**
 * @file    bsp_uart.h
 * @brief   调试串口驱动接口
 * @note    基于hal_uart + 环形缓冲区实现中断接收/阻塞发送
 *          支持printf重定向和逐字节/逐行读取
 *          UART0已在SYSCFG_DL_init()中完成波特率/格式配置
 */
#ifndef BSP_UART_H
#define BSP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "bsp_common.h"

/* ======================== 配置宏 ======================== */

/** 接收环形缓冲区大小(字节,必须为2的幂) */
#define BSP_UART_RX_BUF_SIZE    128U

/** 换行符 */
#define BSP_UART_NEWLINE        '\n'

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化调试串口
 * @note   使能UART RX中断，清空接收缓冲区
 *         UART外设已由SYSCFG_DL_init()配置，此函数仅补充NVIC使能
 * @retval BSP_OK 初始化成功
 */
bsp_status_t bsp_uart_init(void);

/**
 * @brief  反初始化调试串口
 * @note   禁止UART中断，清空缓冲区
 */
void bsp_uart_deinit(void);

/**
 * @brief  发送单字节(阻塞)
 * @param  data 待发送字节
 * @retval BSP_OK 发送成功
 */
bsp_status_t bsp_uart_putc(uint8_t data);

/**
 * @brief  发送字符串(阻塞)
 * @param  str 以'\0'结尾的字符串
 * @retval BSP_OK          发送成功
 * @retval BSP_ERR_NULL_PTR 空指针
 */
bsp_status_t bsp_uart_puts(const char *str);

/**
 * @brief  从接收缓冲区读取一个字节(非阻塞)
 * @param  data 读取字节存放地址
 * @retval BSP_OK           读取成功
 * @retval BSP_ERR_BUF_EMPTY 缓冲区为空
 */
bsp_status_t bsp_uart_getc(uint8_t *data);

/**
 * @brief  查询接收缓冲区中的数据长度
 * @retval 缓冲区中有效数据字节数
 */
uint32_t bsp_uart_rx_count(void);

/**
 * @brief  清空接收缓冲区
 */
void bsp_uart_rx_flush(void);

/**
 * @brief  通过DMA发送数据(非阻塞)
 * @note   数据被拷贝到内部bounce buffer，调用者可立即重用data指向的内存
 *         如果上一次DMA未完成，返回BSP_ERR_BUSY
 * @param  data 数据指针
 * @param  len  数据长度（最大BSP_DMA_TX_BUF_SIZE，超过则直接使用data指针）
 * @retval BSP_OK          启动DMA成功
 * @retval BSP_ERR_BUSY    上一次发送未完成
 * @retval BSP_ERR_NULL_PTR 空指针
 */
bsp_status_t bsp_uart_send_dma(const uint8_t *data, uint16_t len);

/**
 * @brief  查询DMA发送是否空闲
 * @retval true  上一次DMA发送已完成(EOT)
 * @retval false DMA正在发送
 */
bool bsp_uart_tx_idle(void);

/**
 * @brief  UART中断服务函数
 * @note   在UART0_IRQHandler中调用此函数
 *         ISR中仅将接收字节存入环形缓冲区
 *         处理DMA发送完成中断
 */
void bsp_uart_irq_handler(void);

/* ======================== 调试计数器 ======================== */
extern volatile uint32_t dbg_isr_count;
extern volatile uint32_t dbg_uart_isr_total;
extern volatile uint32_t dbg_iidx_value;
extern volatile uint32_t dbg_eot_count;
extern volatile uint32_t dbg_rx_count;
extern volatile uint32_t dbg_other_count;
extern volatile uint32_t dbg_rx_overflow;

#ifdef __cplusplus
}
#endif

#endif /* BSP_UART_H */
