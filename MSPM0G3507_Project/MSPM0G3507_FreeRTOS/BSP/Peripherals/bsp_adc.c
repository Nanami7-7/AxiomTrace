/**
 * @file    bsp_adc.c
 * @brief   ADC采样驱动实现
 * 说明：ADC采样相关处理。
 * 说明：ADC采样相关处理。
 */
#include "bsp_adc.h"
#include "hal_adc.h"
#include "project_config.h"
#include "osal_api.h"
#include "ti_msp_dl_config.h"

/* ======================== 私有常量 ======================== */

/* 说明：ADC采样相关处理。 */
#define ADC_POLL_TIMEOUT  (4000000U)

/* ======================== 私有类型 ======================== */

/**
 * @brief ADC通道配置映射
 * @note  将BSP通道枚举映射到HAL ADC实例
 */
typedef struct {
    hal_adc_id_t hal_id;  /**< HAL ADC实例编号 */
    uint32_t mem_idx;     /**< ADC MEM 索引 */
} adc_channel_config_t;

/* ======================== 私有变量 ======================== */

/* 说明：ADC采样相关处理。 */
static const adc_channel_config_t s_adc_channels[BSP_ADC_CH_COUNT] = {
    { PRJ_ADC_VOLTAGE_ID, 0U }, /* M1 current / PA15 */
    { PRJ_ADC_VOLTAGE_ID, 1U }, /* M2 current / PA16 */
    { PRJ_ADC_VOLTAGE_ID, 2U }, /* M3 current / PA17 */
    { PRJ_ADC_VOLTAGE_ID, 3U }, /* M4 current / PA22 */
    { PRJ_ADC_VOLTAGE_ID, 4U }, /* battery / PB18 */
};

/* 说明：ADC采样相关处理。 */
static volatile uint16_t s_last_raw[BSP_ADC_CH_COUNT] = {0};

/** 转换完成标志(ISR置位, 任务清除) */
static volatile bool s_adc_done = false;

/* 说明：ADC采样相关处理。 */
static bool s_adc_inited = false;

/* ======================== 公共函数实现 ======================== */

bsp_status_t bsp_adc_init(void)
{
    if (s_adc_inited) {
        return BSP_OK;
    }

    /* 说明：ADC采样相关处理。 */
    for (uint32_t i = 0; i < BSP_ADC_CH_COUNT; i++) {
        s_last_raw[i] = 0U;
    }

    /* 使能ADC中断 */
    NVIC_ClearPendingIRQ(ADC_VOLTAGE_INST_INT_IRQN);
    NVIC_EnableIRQ(ADC_VOLTAGE_INST_INT_IRQN);

    s_adc_inited = true;
    return BSP_OK;
}

bsp_status_t bsp_adc_start_all(void)
{
    /* Repeat/sequence configuration converts MEM0..MEM4 as one transaction. */
    return bsp_adc_start_conversion(BSP_ADC_CH_M1_CURRENT);
}

uint16_t bsp_adc_get_last_current_raw(uint8_t motor_idx)
{
    if (motor_idx >= 4U) {
        return 0U;
    }
    return s_last_raw[motor_idx];
}

float bsp_adc_get_last_current_ma(uint8_t motor_idx)
{
    return (float)bsp_adc_get_last_current_raw(motor_idx) *
           PRJ_ADC_CURRENT_MA_PER_RAW;
}

void bsp_adc_get_all_currents_ma(float currents_ma[4])
{
    uint16_t raw[4];
    if (currents_ma == NULL) {
        return;
    }

    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0U; i < 4U; i++) {
            raw[i] = s_last_raw[i];
        }
    }

    for (uint32_t i = 0U; i < 4U; i++) {
        currents_ma[i] = (float)raw[i] * PRJ_ADC_CURRENT_MA_PER_RAW;
    }
}

uint32_t bsp_adc_get_bus_voltage_mv(void)
{
    uint16_t raw;
    OSAL_CRITICAL_SECTION {
        raw = s_last_raw[BSP_ADC_CH_BATTERY];
    }
    return (uint32_t)raw * PRJ_ADC_VREF_MV / PRJ_ADC_RESOLUTION;
}

bsp_status_t bsp_adc_read_raw(bsp_adc_channel_t channel,
                                uint16_t *raw_val)
{
    if (raw_val == NULL) {
        return BSP_ERR_NULL_PTR;
    }
    if ((uint32_t)channel >= BSP_ADC_CH_COUNT) {
        return BSP_ERR_INVALID_PARAM;
    }

    /* 说明：ADC采样相关处理。 */
    bsp_adc_clear_done_flag();
    if (bsp_adc_start_conversion(channel) != BSP_OK) {
        return BSP_ERR_HW_FAULT;
    }

    uint32_t timeout = ADC_POLL_TIMEOUT;
    while (!bsp_adc_is_conversion_done()) {
        if (timeout == 0U) {
            return BSP_ERR_TIMEOUT;
        }
        timeout--;
    }

    *raw_val = bsp_adc_get_last_raw(channel);
    return BSP_OK;
}

bsp_status_t bsp_adc_read_voltage(bsp_adc_channel_t channel,
                                    uint32_t *voltage)
{
    if (voltage == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    uint16_t raw;

    bsp_status_t ret = bsp_adc_read_raw(channel, &raw);
    if (ret != BSP_OK) {
        return ret;
    }

    /* voltage(mV) = raw * VREF(mV) / RESOLUTION */
    *voltage = (uint32_t)raw * PRJ_ADC_VREF_MV
               / PRJ_ADC_RESOLUTION;

    return BSP_OK;
}

bsp_status_t bsp_adc_start_conversion(bsp_adc_channel_t channel)
{
    if ((uint32_t)channel >= BSP_ADC_CH_COUNT) {
        return BSP_ERR_INVALID_PARAM;
    }

    hal_status_t ret = hal_adc_start_conversion(
        s_adc_channels[channel].hal_id);
    if (ret != HAL_OK) {
        return BSP_ERR_HW_FAULT;
    }

    return BSP_OK;
}

bsp_status_t bsp_adc_read_sequence(uint16_t raw_values[], uint32_t count)
{
    if (raw_values == NULL) {
        return BSP_ERR_NULL_PTR;
    }
    if (count < BSP_ADC_CH_COUNT) {
        return BSP_ERR_INVALID_PARAM;
    }

    bsp_adc_clear_done_flag();
    if (bsp_adc_start_conversion(BSP_ADC_CH_M1_CURRENT) != BSP_OK) {
        return BSP_ERR_HW_FAULT;
    }

    uint32_t timeout = ADC_POLL_TIMEOUT;
    while (!bsp_adc_is_conversion_done()) {
        if (timeout == 0U) {
            return BSP_ERR_TIMEOUT;
        }
        timeout--;
    }

    for (uint32_t i = 0U; i < BSP_ADC_CH_COUNT; i++) {
        raw_values[i] = s_last_raw[i];
    }
    return BSP_OK;
}

void bsp_adc_irq_handler(void)
{
    /*
     * IIDX reports the highest-priority pending flag, including MEM0~MEM3
     * flags that are not enabled in IMASK. In a sequence conversion those
     * flags can mask MEM4, so use MIS to test the enabled end-of-sequence IRQ.
     */
    const uint32_t end_irq = DL_ADC12_INTERRUPT_MEM4_RESULT_LOADED;
    if (DL_ADC12_getEnabledInterruptStatus(ADC_VOLTAGE_INST, end_irq) == 0U) {
        return;
    }

    for (uint32_t i = 0U; i < BSP_ADC_CH_COUNT; i++) {
        uint16_t result = 0U;
        hal_status_t ret = hal_adc_read_mem_result(
            s_adc_channels[i].hal_id,
            s_adc_channels[i].mem_idx,
            &result);
        if (ret != HAL_OK) {
            return;
        }
        s_last_raw[i] = result;
    }

    /* Clear all result flags from this sequence before accepting the next one. */
    DL_ADC12_clearInterruptStatus(
        ADC_VOLTAGE_INST,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM1_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM2_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM3_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM4_RESULT_LOADED);
    s_adc_done = true;
}

bool bsp_adc_is_conversion_done(void)
{
    bool done;
    OSAL_CRITICAL_SECTION {
        done = s_adc_done;
    }
    return done;
}

void bsp_adc_clear_done_flag(void)
{
    OSAL_CRITICAL_SECTION {
        s_adc_done = false;
    }
}

void ADC_VOLTAGE_INST_IRQHandler(void)
{
    bsp_adc_irq_handler();
}

uint16_t bsp_adc_get_last_raw(bsp_adc_channel_t channel)
{
    if ((uint32_t)channel >= BSP_ADC_CH_COUNT) {
        return 0U;
    }

    return s_last_raw[channel];
}

uint32_t bsp_adc_get_last_voltage_mv(bsp_adc_channel_t channel)
{
    if ((uint32_t)channel >= BSP_ADC_CH_COUNT) {
        return 0U;
    }

    uint16_t raw = s_last_raw[channel];
    return (uint32_t)raw * PRJ_ADC_VREF_MV / PRJ_ADC_RESOLUTION;
}

uint32_t bsp_adc_get_last_voltage(bsp_adc_channel_t channel)
{
    return bsp_adc_get_last_voltage_mv(channel);
}

