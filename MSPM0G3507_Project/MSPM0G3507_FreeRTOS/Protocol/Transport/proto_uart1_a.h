/**
 * @file    proto_uart1_a.h
 * @brief   Board A UART1 板间协议传输层。
 *
 * UART1 只负责板间二进制协议传输，不输出调试文本。当前启用 F4 固定帧协议时，`PRJ_F4_PROTOCOL_ENABLE` 为 1；关闭时保留旧 COBS 协议路径。
 */
#ifndef PROTO_UART1_A_H
#define PROTO_UART1_A_H

#include <stdint.h>
#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_UART1_A_RX_BUF_SIZE (512U)

typedef struct {
    uint32_t rx_bytes;
    uint32_t rx_overflow;
    uint32_t irq_count;
    uint32_t ignored_irq_count;
} proto_uart1_a_diag_t;

bsp_status_t proto_uart1_a_init(void);
void proto_uart1_a_deinit(void);
bsp_status_t proto_uart1_a_write(const uint8_t *data, uint16_t len);
bsp_status_t proto_uart1_a_getc(uint8_t *data);
uint32_t proto_uart1_a_available(void);
void proto_uart1_a_flush_rx(void);
bsp_status_t proto_uart1_a_get_diag(proto_uart1_a_diag_t *diag);
void proto_uart1_a_irq_handler(void);

#ifdef __cplusplus
}
#endif
#endif /* PROTO_UART1_A_H */
