/**
 * @file    bsp_adc.c
 * @brief   ADC采样驱动实现
 * @note    基于hal_adc实现ADC0单次采样和电压转换
 *          支持阻塞轮询模式和中断模式两种采样方式
 */
#include "bsp_adc.h"
#include "hal_adc.h"
#include "project_config.h"
#include "osal_api.h"
#include "ti_msp_dl_config.h"

/* ======================== 私有常量 ======================== */

/** ADC转换超时循环计数(约1ms @80MHz) */
#define ADC_POLL_TIMEOUT  (80000U)

/* ======================== 私有类型 ======================== */

/**
 * @brief ADC通道配置映射
 * @note  将BSP通道枚举映射到HAL ADC实例
 */
typedef struct {
    hal_adc_id_t hal_id;  /**< HAL ADC实例编号 */
} adc_channel_config_t;

/* ======================== 私有变量 ======================== */

/** ADC通道映射表 */
static const adc_channel_config_t s_adc_channels[BSP_ADC_CH_COUNT] = {
    /* BSP_ADC_CH_VOLTAGE → HAL_ADC_VOLTAGE */
    { PRJ_ADC_VOLTAGE_ID },
};

/** 最近一次转换原始值(ISR写入, 任务读取) */
static volatile uint16_t s_last_raw[BSP_ADC_CH_COUNT] = {0};

/** 转换完成标志(ISR置位, 任务清除) */
static volatile bool s_adc_done = false;

/** 初始化标志 */
static bool s_adc_inited = false;

/* ======================== 公共函数实现 ======================== */

bsp_status_t bsp_adc_init(void)
{
    if (s_adc_inited) {
        return BSP_OK;
    }

    /* 清零原始值 */
    for (uint32_t i = 0; i < BSP_ADC_CH_COUNT; i++) {
        s_last_raw[i] = 0U;
    }

    /* 使能ADC中断 */
    NVIC_ClearPendingIRQ(ADC_VOLTAGE_INST_INT_IRQN);
    NVIC_EnableIRQ(ADC_VOLTAGE_INST_INT_IRQN);

    s_adc_inited = true;
    return BSP_OK;
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

    hal_adc_id_t hal_id = s_adc_channels[channel].hal_id;

    /* 启动软件转换 */
    hal_status_t ret = hal_adc_start_conversion(hal_id);
    if (ret != HAL_OK) {
        return BSP_ERR_HW_FAULT;
    }

    /* 轮询等待转换完成 */
    uint32_t timeout = ADC_POLL_TIMEOUT;

    while (hal_adc_is_busy(hal_id)) {
        if (timeout == 0U) {
            return BSP_ERR_TIMEOUT;
        }
        timeout--;
    }

    /* 转换完成, 读取结果 */
    uint16_t result = 0U;
    ret = hal_adc_read_result(hal_id, &result);
    if (ret != HAL_OK) {
        return BSP_ERR_HW_FAULT;
    }

    *raw_val = result;
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

void bsp_adc_irq_handler(void)
{
    /* 读取IIDX应答中断, 不清零则中断不会再触发 */
    DL_ADC12_IIDX iidx = DL_ADC12_getPendingInterrupt(ADC_VOLTAGE_INST);
    if (iidx != DL_ADC12_IIDX_MEM0_RESULT_LOADED) {
        return;
    }

    /* 读取转换结果(仅通道0) */
    uint16_t result = 0U;
    hal_status_t ret = hal_adc_read_result(
        s_adc_channels[BSP_ADC_CH_VOLTAGE].hal_id, &result);

    if (ret == HAL_OK) {
        s_last_raw[BSP_ADC_CH_VOLTAGE] = result;
    }
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

uint32_t bsp_adc_get_last_voltage(bsp_adc_channel_t channel)
{
    if ((uint32_t)channel >= BSP_ADC_CH_COUNT) {
        return 0U;
    }

    uint16_t raw = s_last_raw[channel];
    return (uint32_t)raw * PRJ_ADC_VREF_MV / PRJ_ADC_RESOLUTION;
}
