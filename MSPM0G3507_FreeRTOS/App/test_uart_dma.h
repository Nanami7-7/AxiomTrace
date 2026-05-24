/**
 * @file    test_uart_dma.h
 * @brief   串口DMA功能测试接口
 */
#ifndef TEST_UART_DMA_H
#define TEST_UART_DMA_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  运行所有DMA测试
 * @note   在main.c中调用，在调度器启动前执行
 *         使用osal_delay_ms()进行延时（忙等，不需要调度器）
 */
void test_uart_dma_run(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_UART_DMA_H */
