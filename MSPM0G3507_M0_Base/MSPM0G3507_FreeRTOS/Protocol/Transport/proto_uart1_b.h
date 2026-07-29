/**
 * @file    proto_uart1_b.h
 * @brief   Board B UART1 板间协议传输层。
 *
 * UART1 只负责 Board B 与 Board A 的二进制协议字节流，不输出调试文本。
 */
#ifndef PROTO_UART1_B_H
#define PROTO_UART1_B_H

#include <stdint.h>
#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_UART1_B_RX_BUF_SIZE (512U)

typedef struct {
    uint32_t rx_bytes;
    uint32_t rx_overflow;
    uint32_t irq_count;
    uint32_t ignored_irq_count;
} proto_uart1_b_diag_t;

bsp_status_t proto_uart1_b_init(void);
void proto_uart1_b_deinit(void);
bsp_status_t proto_uart1_b_write(const uint8_t *data, uint16_t len);
bsp_status_t proto_uart1_b_getc(uint8_t *data);
uint32_t proto_uart1_b_available(void);
void proto_uart1_b_flush_rx(void);
bsp_status_t proto_uart1_b_get_diag(proto_uart1_b_diag_t *diag);
void proto_uart1_b_irq_handler(void);

#ifdef __cplusplus
}
#endif
#endif /* PROTO_UART1_B_H */
