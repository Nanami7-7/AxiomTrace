/**
 * @file    hal_uart.c
 * @brief   UART HAL implementation for MSPM0G3507
 */

#include "hal_conf.h"

#ifdef HAL_UART_MODULE_ENABLED

#include "hal_uart.h"
#include "hal_common.h"

/* ---------- Maximum UART instances ---------- */
#define HAL_UART_MAX_INSTANCES  2

/* ---------- Callback storage ---------- */
typedef struct {
    hal_uart_rx_cb_t rx_cb;
    hal_uart_tx_cb_t tx_cb;
} hal_uart_callbacks_t;

static hal_uart_callbacks_t s_uart_callbacks[HAL_UART_MAX_INSTANCES];

/* ========== API Implementation ========== */

hal_status_t hal_uart_init(const hal_uart_config_t *config)
{
    HAL_CHECK_PTR(config);

    DL_UART_Main_InitTypeDef uart_init = {0};

    /* Baud rate */
    uart_init.baudRate = config->baudrate;

    /* Data bits */
    uart_init.dataBits = (config->data_bits == HAL_UART_DATABITS_7)
                         ? DL_UART_MAIN_DATA_BITS_7
                         : DL_UART_MAIN_DATA_BITS_8;

    /* Stop bits */
    uart_init.stopBits = (config->stop_bits == HAL_UART_STOPBITS_2)
                         ? DL_UART_MAIN_STOP_BITS_2
                         : DL_UART_MAIN_STOP_BITS_1;

    /* Parity */
    switch (config->parity) {
    case HAL_UART_PARITY_NONE:
        uart_init.parity = DL_UART_MAIN_PARITY_NONE;
        break;
    case HAL_UART_PARITY_ODD:
        uart_init.parity = DL_UART_MAIN_PARITY_ODD;
        break;
    case HAL_UART_PARITY_EVEN:
        uart_init.parity = DL_UART_MAIN_PARITY_EVEN;
        break;
    default:
        return HAL_INVALID;
    }

    DL_UART_Main_init(config->instance, &uart_init);
    DL_UART_Main_enable(config->instance);

    /* Enable RX interrupt by default */
    DL_UART_Main_enableInterrupt(config->instance, DL_UART_MAIN_INTERRUPT_RX);

    uint32_t irqn = (config->instance == UART_0_INST) ? UART0_INT_IRQn : UART1_INT_IRQn;
    NVIC_SetPriority(irqn, config->irq_priority);
    NVIC_EnableIRQ(irqn);

    return HAL_OK;
}

hal_status_t hal_uart_deinit(uint32_t instance)
{
    DL_UART_Main_disable(instance);
    DL_UART_Main_reset(instance);
    return HAL_OK;
}

hal_status_t hal_uart_write(uint32_t instance, const uint8_t *data, uint16_t len)
{
    HAL_CHECK_PTR(data);

    for (uint16_t i = 0; i < len; i++) {
        uint32_t start = hal_get_tick();
        while (!DL_UART_Main_isTXFIFOEmpty(instance)) {
            if (hal_timeout_expired(start, 100)) {  /* 100ms per byte timeout */
                return HAL_TIMEOUT;
            }
        }
        DL_UART_Main_transmitData(instance, data[i]);
    }
    return HAL_OK;
}

hal_status_t hal_uart_read(uint32_t instance, uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
    HAL_CHECK_PTR(buf);

    for (uint16_t i = 0; i < len; i++) {
        uint32_t start = hal_get_tick();

        while (DL_UART_Main_isRXFIFOEmpty(instance)) {
            if (timeout_ms != HAL_WAIT_FOREVER) {
                if (hal_timeout_expired(start, timeout_ms)) {
                    return HAL_TIMEOUT;
                }
            }
        }
        buf[i] = (uint8_t)DL_UART_Main_receiveData(instance);
    }
    return HAL_OK;
}

hal_status_t hal_uart_write_byte(uint32_t instance, uint8_t byte)
{
    while (!DL_UART_Main_isTXFIFOEmpty(instance)) {
    }
    DL_UART_Main_transmitData(instance, byte);
    return HAL_OK;
}

hal_status_t hal_uart_read_byte(uint32_t instance, uint8_t *byte, uint32_t timeout_ms)
{
    HAL_CHECK_PTR(byte);
    return hal_uart_read(instance, byte, 1, timeout_ms);
}

hal_status_t hal_uart_register_rx_callback(uint32_t instance, hal_uart_rx_cb_t cb)
{
    HAL_CHECK_PTR(cb);

    uint32_t idx = (instance == UART_0_INST) ? 0 : 1;
    if (idx >= HAL_UART_MAX_INSTANCES) {
        return HAL_INVALID;
    }
    s_uart_callbacks[idx].rx_cb = cb;
    return HAL_OK;
}

hal_status_t hal_uart_register_tx_callback(uint32_t instance, hal_uart_tx_cb_t cb)
{
    HAL_CHECK_PTR(cb);

    uint32_t idx = (instance == UART_0_INST) ? 0 : 1;
    if (idx >= HAL_UART_MAX_INSTANCES) {
        return HAL_INVALID;
    }
    s_uart_callbacks[idx].tx_cb = cb;
    return HAL_OK;
}

/* ---------- ISR handlers ---------- */
void UART0_IRQHandler(void)
{
    if (DL_UART_Main_getEnabledInterruptStatus(UART_0_INST, DL_UART_MAIN_INTERRUPT_RX)) {
        uint8_t data = (uint8_t)DL_UART_Main_receiveData(UART_0_INST);
        if (s_uart_callbacks[0].rx_cb != NULL) {
            s_uart_callbacks[0].rx_cb(UART_0_INST, data);
        }
    }
    if (DL_UART_Main_getEnabledInterruptStatus(UART_0_INST, DL_UART_MAIN_INTERRUPT_TX)) {
        DL_UART_Main_clearInterruptStatus(UART_0_INST, DL_UART_MAIN_INTERRUPT_TX);
        if (s_uart_callbacks[0].tx_cb != NULL) {
            s_uart_callbacks[0].tx_cb(UART_0_INST);
        }
    }
}

void UART1_IRQHandler(void)
{
    if (DL_UART_Main_getEnabledInterruptStatus(UART_1_INST, DL_UART_MAIN_INTERRUPT_RX)) {
        uint8_t data = (uint8_t)DL_UART_Main_receiveData(UART_1_INST);
        if (s_uart_callbacks[1].rx_cb != NULL) {
            s_uart_callbacks[1].rx_cb(UART_1_INST, data);
        }
    }
    if (DL_UART_Main_getEnabledInterruptStatus(UART_1_INST, DL_UART_MAIN_INTERRUPT_TX)) {
        DL_UART_Main_clearInterruptStatus(UART_1_INST, DL_UART_MAIN_INTERRUPT_TX);
        if (s_uart_callbacks[1].tx_cb != NULL) {
            s_uart_callbacks[1].tx_cb(UART_1_INST);
        }
    }
}

#endif /* HAL_UART_MODULE_ENABLED */
