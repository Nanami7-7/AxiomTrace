/**
 * @file    bsp_ble_uart.c
 * @brief   UART2 BLE 板级传输实现（DX-BT311 专用）。
 * @details
 * UART2 接收采用中断搬运到软件环形缓冲区，发送采用 HAL 轮询接口。该层
 * 不解析 AT 命令，协议事务由上层 app_dx_ble_service 和 dx_bt311 驱动负责。
 *
 * @note 当前 SysConfig 将 UART2 配置为 9600 baud、8N1，
 *       PB15=TX、PB16=RX，与 DX-BT311 默认波特率一致。
 */
#include "bsp_ble_uart.h"
#include "hal_uart.h"
#include "osal_api.h"
#include "project_config.h"
#include "ti_msp_dl_config.h"

/*
 * SysConfig 生成的实例宏采用 UART_2_INST 命名；历史 BLE 代码使用
 * UART2_INST。这里仅做局部兼容映射，避免修改自动生成的配置文件。
 */
#ifndef UART2_INST
#if defined(UART_2_INST)
#define UART2_INST UART_2_INST
#else
#error "UART2_INST/UART_2_INST is not defined; check SysConfig UART2 configuration"
#endif
#endif

/** @brief UART2 BLE 软件接收环形缓冲存储区。 */
static uint8_t s_rx_storage[BSP_BLE_UART_RX_BUF_SIZE];

/** @brief UART2 BLE 软件接收环形缓冲控制块。 */
static bsp_ringbuf_t s_rx_ring;

/** @brief UART2 BLE 板级传输是否已初始化。 */
static bool s_is_inited;

/** @brief 由中断更新、由诊断接口读取的传输统计。 */
static volatile bsp_ble_uart_diag_t s_diag;

/**
 * @brief 将 HAL 状态码映射为 BSP 状态码。
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
 * @brief 初始化 UART2 BLE 传输层。
 * @details 初始化软件接收环形缓冲，丢弃初始化前残留字节并使能 UART2 中断。
 *          SysConfig 已将 UART2 配置为 9600 baud，无需运行时覆盖。
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

    /* 丢弃初始化前残留字节 */
    while (DL_UART_receiveDataCheck(UART_2_INST, &stale)) {
    }

    /* 仅打开 NVIC 不够，还必须打开 UART2 外设内部的接收中断。 */
    DL_UART_Main_enableInterrupt(UART_2_INST, DL_UART_MAIN_INTERRUPT_RX);

    hal_ret = hal_uart_enable_irq(PRJ_UART_BLE_ID);
    if (hal_ret != HAL_OK) {
        return map_hal_status(hal_ret);
    }

    s_is_inited = true;
    return BSP_OK;
}

/**
 * @brief 反初始化 UART2 BLE 传输层。
 */
void bsp_ble_uart_deinit(void)
{
    if (!s_is_inited) {
        return;
    }
    DL_UART_Main_disableInterrupt(UART_2_INST, DL_UART_MAIN_INTERRUPT_RX);
    (void)hal_uart_disable_irq(PRJ_UART_BLE_ID);
    bsp_ringbuf_flush(&s_rx_ring);
    s_is_inited = false;
}

/**
 * @brief 通过 HAL 发送 UART2 字节流。
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
 */
uint32_t bsp_ble_uart_available(void)
{
    return s_is_inited ? bsp_ringbuf_count(&s_rx_ring) : 0U;
}

/**
 * @brief 在临界区内清空 UART2 软件接收缓冲。
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
 * @brief 读取 UART2 接收和中断诊断计数。
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
 * @brief 处理 UART2 接收相关中断。
 * @details RX 字节会被搬运到环形缓冲区；缓冲区满时增加溢出计数。
 */
void bsp_ble_uart_irq_handler(void)
{
    DL_UART_IIDX idx = DL_UART_getPendingInterrupt(UART_2_INST);

    s_diag.irq_count++;
    switch (idx) {
    case DL_UART_IIDX_RX:
    case DL_UART_IIDX_RX_TIMEOUT_ERROR:
        {
            uint8_t data;
            while (DL_UART_receiveDataCheck(UART_2_INST, &data)) {
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
        break;

    default:
        s_diag.ignored_irq_count++;
        break;
    }
}