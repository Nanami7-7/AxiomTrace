/**
 * @file    bsp_debug.h
 * @brief   UART0 调试输出和调试命令接收接口。
 *
 * UART0 固定用于调试：TX=PA10，RX=PA11，9600 8N1。
 * printf() 继续从 UART0 输出；本文件新增一个很小的 UART0 接收缓冲，
 * 供调试命令使用。接收中断只搬运字节，不在中断中解析命令。
 */
#ifndef BSP_DEBUG_H
#define BSP_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_common.h"

/** UART0 调试命令接收缓冲区容量，必须是 2 的幂。 */
#define BSP_DEBUG_RX_BUF_SIZE (256U)

/** 初始化 UART0 调试接收功能。 */
bsp_status_t bsp_debug_init(void);

/** 非阻塞读取一个 UART0 接收字节。 */
bsp_status_t bsp_debug_getc(uint8_t *data);

/** 查询 UART0 接收缓冲区中的字节数。 */
uint32_t bsp_debug_available(void);

/** 清空 UART0 接收缓冲区。 */
void bsp_debug_flush_rx(void);

/** UART0 中断服务入口，由 Config/board.c 调用。 */
void bsp_debug_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DEBUG_H */