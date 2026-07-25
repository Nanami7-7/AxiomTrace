/**
 * @file    bsp_adc.c
 * @brief   ADC采样驱动实现
 * @note    基于ADC1实现5通道采样(4路电流+1路电压)
 *          repeat模式下ADC自动循环转换5个MEM通道
 *          MEM4(最后一个)完成中断触发ISR, 一次性读取5个结果
 *
 *          电流换算公式:
 *            I(mA) = raw × VREF_mV / RESOLUTION / SHUNT_OHM / AMPLIFY
 *                  = raw × 3300 / 4096 / 0.15 / 10
 *                  = raw × 0.537 mA
 *
 *          采样时序:
 *            sampleTime = 5 cycles @ 4MHz = 1.25us/通道
 *            5通道总转换时间 ≈ 6.25us
 *
 *          竞态保护:
 *            s_last_raw[] 由ISR写入, 任务读取. ARM Cortex-M0+上
 *            32位对齐的uint16_t读写是原子的, 无需额外保护.
 *            s_adc_done 标志使用volatile, 确保编译器不优化掉读取.
 */
#include "bsp_adc.h"
#include "hal_adc.h"
#include "ti_msp_dl_config.h"
#include "project_config.h"

/* ======================== 常量定义 ======================== */

/** ADC参考电压(mV) */
#define BSP_ADC_VREF_MV          ADC_VREF_MV

/** ADC分辨率(12位) */
#define BSP_ADC_RESOLUTION       ADC_RESOLUTION

/** 电流采样电阻值(欧姆) */
#define BSP_ADC_SHUNT_OHM        PRJ_ADC_CURRENT_SHUNT_OHM

/** 电流放大倍数 */
#define BSP_ADC_AMPLIFY          PRJ_ADC_CURRENT_AMPLIFY

/** mA/raw换算系数 */
#define BSP_ADC_MA_PER_RAW       ((float)BSP_ADC_VREF_MV \
                                  / (float)BSP_ADC_RESOLUTION \
                                  / BSP_ADC_SHUNT_OHM \
                                  / BSP_ADC_AMPLIFY)

/* ======================== 私有变量 ======================== */

/**
 * @brief 最近一次ADC转换原始值(ISR写入, 任务读取)
 * @note  ARM Cortex-M0+上uint16_t读写原子, 加volatile防止编译器缓存
 */
static volatile uint16_t s_last_raw[BSP_ADC_CH_COUNT] = {0};

/**
 * @brief ADC转换完成标志(ISR置位, 任务清除)
 */
static volatile bool s_adc_done = false;

/* ======================== 函数实现 ======================== */

bsp_status_t bsp_adc_init(void)
{
    /* ADC硬件已由SYSCFG_DL_ADC_VOLTAGE_init()配置
     * 此处仅清零软件状态 */
    for (uint32_t i = 0; i < BSP_ADC_CH_COUNT; i++) {
        s_last_raw[i] = 0;
    }
    s_adc_done = false;
    return BSP_OK;
}

bsp_status_t bsp_adc_start_all(void)
{
    /* 清除done标志, 启动新一轮转换 */
    s_adc_done = false;
    DL_ADC12_clearInterruptStatus(ADC_VOLTAGE_INST,
        DL_ADC12_INTERRUPT_MEM4_RESULT_LOADED);
    DL_ADC12_startConversion(ADC_VOLTAGE_INST);
    return BSP_OK;
}

bool bsp_adc_is_conversion_done(void)
{
    return s_adc_done;
}

void bsp_adc_clear_done_flag(void)
{
    s_adc_done = false;
}

void bsp_adc_irq_handler(void)
{
    DL_ADC12_IIDX iidx = DL_ADC12_getPendingInterrupt(ADC_VOLTAGE_INST);

    if (iidx == DL_ADC12_IIDX_MEM4_RESULT_LOADED) {
        /* MEM4是最后一个通道, 此刻5个MEM结果都已就绪 */
        s_last_raw[BSP_ADC_CH_CURRENT_1] =
            (uint16_t)DL_ADC12_getMemResult(ADC_VOLTAGE_INST,
                DL_ADC12_MEM_IDX_0);
        s_last_raw[BSP_ADC_CH_CURRENT_2] =
            (uint16_t)DL_ADC12_getMemResult(ADC_VOLTAGE_INST,
                DL_ADC12_MEM_IDX_1);
        s_last_raw[BSP_ADC_CH_CURRENT_3] =
            (uint16_t)DL_ADC12_getMemResult(ADC_VOLTAGE_INST,
                DL_ADC12_MEM_IDX_2);
        s_last_raw[BSP_ADC_CH_CURRENT_4] =
            (uint16_t)DL_ADC12_getMemResult(ADC_VOLTAGE_INST,
                DL_ADC12_MEM_IDX_3);
        s_last_raw[BSP_ADC_CH_VOLTAGE] =
            (uint16_t)DL_ADC12_getMemResult(ADC_VOLTAGE_INST,
                DL_ADC12_MEM_IDX_4);

        s_adc_done = true;
    }
    /* 其他中断源忽略, DL_ADC12_getPendingInterrupt会自动清除标志 */
}

uint16_t bsp_adc_get_last_raw(bsp_adc_channel_t channel)
{
    if ((uint32_t)channel >= BSP_ADC_CH_COUNT) {
        return 0;
    }
    return s_last_raw[channel];
}

uint32_t bsp_adc_get_last_voltage(bsp_adc_channel_t channel)
{
    if ((uint32_t)channel >= BSP_ADC_CH_COUNT) {
        return 0;
    }
    uint16_t raw = s_last_raw[channel];
    return ((uint32_t)raw * BSP_ADC_VREF_MV) / BSP_ADC_RESOLUTION;
}

uint16_t bsp_adc_get_last_current_raw(uint8_t motor_idx)
{
    if (motor_idx >= 4) {
        return 0;
    }
    /* 电机0~3对应通道0~3 */
    return s_last_raw[motor_idx];
}

float bsp_adc_get_last_current_ma(uint8_t motor_idx)
{
    if (motor_idx >= 4) {
        return 0.0f;
    }
    return (float)s_last_raw[motor_idx] * BSP_ADC_MA_PER_RAW;
}

void bsp_adc_get_all_currents_ma(float currents_ma[4])
{
    if (currents_ma == NULL) return;
    for (uint32_t i = 0; i < 4; i++) {
        currents_ma[i] = (float)s_last_raw[i] * BSP_ADC_MA_PER_RAW;
    }
}

uint32_t bsp_adc_get_bus_voltage_mv(void)
{
    return ((uint32_t)s_last_raw[BSP_ADC_CH_VOLTAGE] * BSP_ADC_VREF_MV)
           / BSP_ADC_RESOLUTION;
}

/* ---- 旧接口实现(保留向后兼容) ---- */

bsp_status_t bsp_adc_read_raw(bsp_adc_channel_t channel,
                                uint16_t *raw_val)
{
    if (raw_val == NULL) {
        return BSP_ERR_NULL_PTR;
    }
    if ((uint32_t)channel >= BSP_ADC_CH_COUNT) {
        return BSP_ERR_INVALID_PARAM;
    }

    /* 启动转换 */
    s_adc_done = false;
    DL_ADC12_clearInterruptStatus(ADC_VOLTAGE_INST,
        DL_ADC12_INTERRUPT_MEM4_RESULT_LOADED);
    DL_ADC12_startConversion(ADC_VOLTAGE_INST);

    /* 轮询等待完成(最大等待约100us) */
    uint32_t timeout = 5000U;
    while (!s_adc_done && timeout-- > 0) {
        /* 紧凑循环等待 */
    }

    if (!s_adc_done) {
        return BSP_ERR_TIMEOUT;
    }

    *raw_val = s_last_raw[channel];
    return BSP_OK;
}

bsp_status_t bsp_adc_read_voltage(bsp_adc_channel_t channel,
                                    uint32_t *voltage)
{
    if (voltage == NULL) {
        return BSP_ERR_NULL_PTR;
    }
    if ((uint32_t)channel >= BSP_ADC_CH_COUNT) {
        return BSP_ERR_INVALID_PARAM;
    }

    uint16_t raw;
    bsp_status_t status = bsp_adc_read_raw(channel, &raw);
    if (status != BSP_OK) {
        return status;
    }

    *voltage = ((uint32_t)raw * BSP_ADC_VREF_MV) / BSP_ADC_RESOLUTION;
    return BSP_OK;
}

bsp_status_t bsp_adc_start_conversion(bsp_adc_channel_t channel)
{
    (void)channel;  /* repeat模式下所有通道一起转换 */
    s_adc_done = false;
    DL_ADC12_clearInterruptStatus(ADC_VOLTAGE_INST,
        DL_ADC12_INTERRUPT_MEM4_RESULT_LOADED);
    DL_ADC12_startConversion(ADC_VOLTAGE_INST);
    return BSP_OK;
}
