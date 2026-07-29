/**
 * @file    bsp_ble_uart.h
 * @brief   外部 BLE 模块使用的 UART2 字节流板级驱动接口。
 * @details
 * 本模块只负责 UART2 的接收中断、软件环形缓冲和轮询发送，不解析 JDY-23
 * AT 命令，也不参与 UART0 调试串口或 DMA 状态管理。
 * @note 当前 SysConfig 硬件映射为 UART2：TX=PB15、RX=PB16、9600 8N1。
 *       如果修改了 SysConfig，请同步检查这里和 BLE 模块实际接线。
 */
#ifndef BSP_BLE_UART_H
#define BSP_BLE_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_common.h"

/**
 * @brief UART2 软件接收环形缓冲区容量，单位为字节。
 */
#define BSP_BLE_UART_RX_BUF_SIZE (256U)

/**
 * @brief UART2 BLE 传输层诊断计数快照。
 */
typedef struct {
    uint32_t rx_bytes;          /**< 成功写入软件接收缓冲区的字节数。 */
    uint32_t rx_overflow;       /**< 缓冲区满导致丢弃的字节数。 */
    uint32_t irq_count;         /**< UART2 中断进入次数。 */
    uint32_t ignored_irq_count; /**< 未处理或未预期中断源次数。 */
} bsp_ble_uart_diag_t;

/**
 * @brief 初始化 UART2 BLE 接收缓冲和中断。
 * @return BSP_OK 表示成功；否则返回 BSP 错误码。
 */
bsp_status_t bsp_ble_uart_init(void);

/**
 * @brief 关闭 UART2 BLE 中断并清空软件接收缓冲区。
 */
void bsp_ble_uart_deinit(void);

/**
 * @brief 通过 UART2 发送一段字节流。
 * @param data 待发送数据；len 为 0 时可为 NULL。
 * @param len 数据长度，单位为字节。
 * @return 发送状态。
 */
bsp_status_t bsp_ble_uart_write(const uint8_t *data, uint16_t len);

/**
 * @brief 非阻塞读取一个接收字节。
 * @param[out] data 输出字节。
 * @return 有数据返回 BSP_OK；无数据返回 BSP_ERR_BUF_EMPTY。
 */
bsp_status_t bsp_ble_uart_getc(uint8_t *data);

/**
 * @brief 查询当前接收缓冲区中的字节数。
 * @return 待读取字节数，未初始化时返回 0。
 */
uint32_t bsp_ble_uart_available(void);

/**
 * @brief 丢弃 UART2 软件接收缓冲区中的全部数据。
 */
void bsp_ble_uart_flush_rx(void);

/**
 * @brief 获取 UART2 BLE 诊断计数的原子快照。
 * @param[out] diag 输出诊断结构体。
 * @return 成功返回 BSP_OK；diag 为 NULL 时返回 BSP_ERR_NULL_PTR。
 */
bsp_status_t bsp_ble_uart_get_diag(bsp_ble_uart_diag_t *diag);

/**
 * @brief UART2 中断服务入口。
 * @details 由对应的 UART2 IRQHandler 调用；正常上下文中不应直接轮询调用。
 */
void bsp_ble_uart_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BLE_UART_H */
