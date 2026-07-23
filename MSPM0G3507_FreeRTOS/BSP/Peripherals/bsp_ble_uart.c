/**
 * @file    bsp_ble_uart.c
 * @brief   JDY-23 使用的独立 UART1 板级传输实现。
 * @details
 * UART1 接收采用中断搬运到软件环形缓冲区，发送采用 HAL 轮询接口。该层
 * 不解析 AT 命令，协议事务由上层 app_ble_service 和 jdy23 驱动负责。
 */
#include "bsp_ble_uart.h"
#include "hal_uart.h"
#include "osal_api.h"
#include "project_config.h"
#include "ti_msp_dl_config.h"

/** @brief UART1 BLE 软件接收环形缓冲存储区。 */
static uint8_t s_rx_storage[BSP_BLE_UART_RX_BUF_SIZE];

/** @brief UART1 BLE 软件接收环形缓冲控制块。 */
static bsp_ringbuf_t s_rx_ring;

/** @brief UART1 BLE 板级传输是否已初始化。 */
static bool s_is_inited;

/** @brief 由中断更新、由诊断接口读取的传输统计。 */
static volatile bsp_ble_uart_diag_t s_diag;

/**
 * @brief 将 HAL 状态码映射为 BSP 状态码。
 * @param status HAL 层状态码。
 * @return 对应的 BSP 层状态码。
 */
static bsp_status_t map_hal_status(hal_status_t status)
{
    switch (status) {
    case HAL_OK:
        return BSP_OK;
    case HAL_ERR_INVALID_PARAM:
        return BSP_ERR_INVALID_PARAM;
    case HAL_ERR_BUSY:
        return BSP_ERR_BUSY;
    case HAL_ERR_TIMEOUT:
        return BSP_ERR_TIMEOUT;
    case HAL_ERR_NOT_INIT:
        return BSP_ERR_NOT_INIT;
    case HAL_ERR_UNSUPPORTED:
        return BSP_ERR_UNSUPPORTED;
    default:
        return BSP_ERR_HW_FAULT;
    }
}

/**
 * @brief 初始化 UART1 BLE 传输层。
 * @details 初始化软件接收环形缓冲，丢弃初始化前残留字节并使能 UART1 中断。
 * @return BSP_OK 表示成功；否则返回 HAL 状态映射后的错误码。
 */
bsp_status_t bsp_ble_uart_init(void)
{
    uint8_t stale;
    hal_status_t hal_ret;

    if (s_is_inited) {
        return BSP_OK;
    }

    bsp_ringbuf_init(&s_rx_ring, s_rx_storage, BSP_BLE_UART_RX_BUF_SIZE);
    while (DL_UART_receiveDataCheck(UART1_INST, &stale)) {
        /* Discard bytes received before the software consumer was ready. */
    }

    hal_ret = hal_uart_enable_irq(PRJ_UART_BLE_ID);
    if (hal_ret != HAL_OK) {
        return map_hal_status(hal_ret);
    }

    s_is_inited = true;
    return BSP_OK;
}

/**
 * @brief 反初始化 UART1 BLE 传输层。
 * @details 禁用 UART1 中断、清空软件接收缓冲并标记为未初始化。
 */
void bsp_ble_uart_deinit(void)
{
    if (!s_is_inited) {
        return;
    }

    (void)hal_uart_disable_irq(PRJ_UART_BLE_ID);
    bsp_ringbuf_flush(&s_rx_ring);
    s_is_inited = false;
}

/**
 * @brief 通过 HAL 发送 UART1 字节流。
 * @param data 待发送数据。
 * @param len 数据长度，单位为字节。
 * @return BSP_OK 或对应的参数、忙、超时和硬件错误码。
 */
bsp_status_t bsp_ble_uart_write(const uint8_t *data, uint16_t len)
{
    if (!s_is_inited) {
        return BSP_ERR_NOT_INIT;
    }
    if ((data == NULL) && (len != 0U)) {
        return BSP_ERR_NULL_PTR;
    }
    if (len == 0U) {
        return BSP_OK;
    }

    return map_hal_status(hal_uart_transmit_buf(PRJ_UART_BLE_ID, data, len));
}

/**
 * @brief 非阻塞地从软件接收缓冲区取出一个字节。
 * @param[out] data 输出字节。
 * @return BSP_OK、BSP_ERR_BUF_EMPTY、BSP_ERR_NOT_INIT 或参数错误。
 */
bsp_status_t bsp_ble_uart_getc(uint8_t *data)
{
    if (data == NULL) {
        return BSP_ERR_NULL_PTR;
    }
    if (!s_is_inited) {
        return BSP_ERR_NOT_INIT;
    }

    return bsp_ringbuf_get(&s_rx_ring, data) ? BSP_OK : BSP_ERR_BUF_EMPTY;
}

/**
 * @brief 查询软件接收缓冲区的当前字节数。
 * @return 待处理字节数；未初始化时为 0。
 */
uint32_t bsp_ble_uart_available(void)
{
    return s_is_inited ? bsp_ringbuf_count(&s_rx_ring) : 0U;
}

/**
 * @brief 在临界区内清空 UART1 软件接收缓冲。
 */
void bsp_ble_uart_flush_rx(void)
{
    if (s_is_inited) {
        OSAL_CRITICAL_SECTION {
            bsp_ringbuf_flush(&s_rx_ring);
        }
    }
}

/**
 * @brief 读取 UART1 接收和中断诊断计数。
 * @param[out] diag 输出快照。
 * @return BSP_OK；diag 为 NULL 时返回 BSP_ERR_NULL_PTR。
 */
bsp_status_t bsp_ble_uart_get_diag(bsp_ble_uart_diag_t *diag)
{
    if (diag == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    OSAL_CRITICAL_SECTION {
        diag->rx_bytes = s_diag.rx_bytes;
        diag->rx_overflow = s_diag.rx_overflow;
        diag->irq_count = s_diag.irq_count;
        diag->ignored_irq_count = s_diag.ignored_irq_count;
    }
    return BSP_OK;
}

/**
 * @brief 处理 UART1 接收相关中断。
 * @details RX 字节会被搬运到环形缓冲区；缓冲区满时增加溢出计数。TX 完成类
 *          中断在当前轮询发送实现中仅被安全忽略。
 */
void bsp_ble_uart_irq_handler(void)
{
    DL_UART_IIDX idx = DL_UART_getPendingInterrupt(UART1_INST);

    s_diag.irq_count++;
    switch (idx) {
    case DL_UART_IIDX_RX:
    case DL_UART_IIDX_RX_TIMEOUT_ERROR:
        {
            uint8_t data;
            while (DL_UART_receiveDataCheck(UART1_INST, &data)) {
                if (bsp_ringbuf_put(&s_rx_ring, data)) {
                    s_diag.rx_bytes++;
                } else {
                    s_diag.rx_overflow++;
                }
            }
        }
        break;

    case DL_UART_IIDX_DMA_DONE_TX:
    case DL_UART_IIDX_EOT_DONE:
        /* UART1 TX is polling in this first integration; these are harmless. */
        break;

    default:
        s_diag.ignored_irq_count++;
        break;
    }
}
