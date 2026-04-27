/**
 * @file    hal_i2c.c
 * @brief   I2C HAL implementation for MSPM0G3507
 */

#include "hal_conf.h"

#ifdef HAL_I2C_MODULE_ENABLED

#include "hal_i2c.h"
#include "hal_common.h"

/* ---------- Maximum I2C instances ---------- */
#define HAL_I2C_MAX_INSTANCES  2

/* ---------- Callback storage ---------- */
typedef struct {
    hal_i2c_rx_cb_t rx_cb;
    hal_i2c_tx_cb_t tx_cb;
} hal_i2c_callbacks_t;

static hal_i2c_callbacks_t s_i2c_callbacks[HAL_I2C_MAX_INSTANCES];

/* ---------- Helper: get instance index ---------- */
static uint32_t i2c_get_idx(uint32_t instance)
{
    return (instance == I2C_0_INST) ? 0 : 1;
}

/* ========== API Implementation ========== */

hal_status_t hal_i2c_init(const hal_i2c_config_t *config)
{
    HAL_CHECK_PTR(config);

    DL_I2C_InitTypeDef i2c_init = {0};

    i2c_init.mode = (config->role == HAL_I2C_ROLE_CONTROLLER)
                    ? DL_I2C_MODE_CONTROLLER
                    : DL_I2C_MODE_TARGET;

    /* Speed / timing */
    switch (config->speed) {
    case HAL_I2C_SPEED_STANDARD:
        i2c_init.clockSpeed = DL_I2C_CLOCK_SPEED_100K;
        break;
    case HAL_I2C_SPEED_FAST:
        i2c_init.clockSpeed = DL_I2C_CLOCK_SPEED_400K;
        break;
    case HAL_I2C_SPEED_FAST_PLUS:
        i2c_init.clockSpeed = DL_I2C_CLOCK_SPEED_1M;
        break;
    default:
        return HAL_INVALID;
    }

    i2c_init.targetAddress = config->own_address;

    DL_I2C_init(config->instance, &i2c_init);
    DL_I2C_enable(config->instance);

    /* Enable interrupts */
    uint32_t irqn = (config->instance == I2C_0_INST) ? I2C0_INT_IRQn : I2C1_INT_IRQn;
    NVIC_SetPriority(irqn, config->irq_priority);
    NVIC_EnableIRQ(irqn);

    return HAL_OK;
}

hal_status_t hal_i2c_deinit(uint32_t instance)
{
    DL_I2C_disable(instance);
    DL_I2C_reset(instance);
    return HAL_OK;
}

hal_status_t hal_i2c_master_write(uint32_t instance, uint16_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    HAL_CHECK_PTR(data);

    DL_I2C_fillControllerTXFIFO(instance, data, len);

    DL_I2C_setControllerTargetAddress(instance, dev_addr);

    if (DL_I2C_startControllerTransfer(instance, dev_addr, DL_I2C_CONTROLLER_DIRECTION_TX, len)
        != DL_I2C_STATUS_SUCCESS) {
        return HAL_ERROR;
    }

    /* Timeout-protected wait */
    uint32_t start = hal_get_tick();
    while (DL_I2C_isControllerBusy(instance)) {
        if (hal_timeout_expired(start, timeout_ms)) {
            DL_I2C_stopControllerTransfer(instance);
            return HAL_TIMEOUT;
        }
    }

    if (DL_I2C_getControllerStatus(instance) & DL_I2C_CONTROLLER_STATUS_NACK) {
        DL_I2C_clearInterruptStatus(instance, DL_I2C_CONTROLLER_STATUS_NACK);
        return HAL_ERROR;
    }

    return HAL_OK;
}

hal_status_t hal_i2c_master_read(uint32_t instance, uint16_t dev_addr, uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
    HAL_CHECK_PTR(buf);

    DL_I2C_setControllerTargetAddress(instance, dev_addr);

    if (DL_I2C_startControllerTransfer(instance, dev_addr, DL_I2C_CONTROLLER_DIRECTION_RX, len)
        != DL_I2C_STATUS_SUCCESS) {
        return HAL_ERROR;
    }

    /* Timeout-protected wait */
    uint32_t start = hal_get_tick();
    while (DL_I2C_isControllerBusy(instance)) {
        if (hal_timeout_expired(start, timeout_ms)) {
            DL_I2C_stopControllerTransfer(instance);
            return HAL_TIMEOUT;
        }
    }

    DL_I2C_receiveControllerData(instance, buf, len);

    return HAL_OK;
}

hal_status_t hal_i2c_master_write_read(uint32_t instance, uint16_t dev_addr,
                                        const uint8_t *wr_data, uint16_t wr_len,
                                        uint8_t *rd_buf, uint16_t rd_len,
                                        uint32_t timeout_ms)
{
    hal_status_t status;

    status = hal_i2c_master_write(instance, dev_addr, wr_data, wr_len, timeout_ms);
    if (status != HAL_OK) {
        return status;
    }

    return hal_i2c_master_read(instance, dev_addr, rd_buf, rd_len, timeout_ms);
}

hal_status_t hal_i2c_register_rx_callback(uint32_t instance, hal_i2c_rx_cb_t cb)
{
    uint32_t idx = i2c_get_idx(instance);
    if (idx >= HAL_I2C_MAX_INSTANCES) return HAL_INVALID;
    s_i2c_callbacks[idx].rx_cb = cb;
    return HAL_OK;
}

hal_status_t hal_i2c_register_tx_callback(uint32_t instance, hal_i2c_tx_cb_t cb)
{
    uint32_t idx = i2c_get_idx(instance);
    if (idx >= HAL_I2C_MAX_INSTANCES) return HAL_INVALID;
    s_i2c_callbacks[idx].tx_cb = cb;
    return HAL_OK;
}

#endif /* HAL_I2C_MODULE_ENABLED */
