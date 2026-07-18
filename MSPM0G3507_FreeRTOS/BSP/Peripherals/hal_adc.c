/**
 * @file    hal_adc.c
 * @brief   HAL ADC硬件抽象实现
 *
 * @note    封装DL_ADC12_xxx调用，ADC0已在SysConfig中完成初始化
 *          此模块仅提供运行时采样启动和结果读取操作
 */
#include "hal_adc.h"
#include "ti_msp_dl_config.h"

/* ======================== 私有映射表 ======================== */

/**
 * @brief ADC实例寄存器指针映射表
 * @note  顺序必须与hal_adc_id_t枚举一致
 */
static ADC12_Regs *const s_adc_inst_map[HAL_ADC_COUNT] = {
    ADC0,  /**< HAL_ADC_VOLTAGE -> ADC0 */
};

/**
 * @brief ADC中断号映射表
 */
static const IRQn_Type s_adc_irq_map[HAL_ADC_COUNT] = {
    ADC0_INT_IRQn,  /**< HAL_ADC_VOLTAGE -> ADC0中断 */
};

/**
 * @brief ADC内存索引映射表
 * @note  对应SysConfig中配置的ADC通道
 */
static const DL_ADC12_MEM_IDX s_adc_mem_map[HAL_ADC_COUNT] = {
    DL_ADC12_MEM_IDX_0,  /**< HAL_ADC_VOLTAGE -> MEM0 */
};

/* ======================== 内联辅助函数 ======================== */

/**
 * @brief  检查ADC实例枚举是否合法
 */
static inline bool is_adc_valid(hal_adc_id_t id)
{
    return ((uint32_t)id < HAL_ADC_COUNT);
}

/* ======================== 公共函数实现 ======================== */

hal_status_t hal_adc_start_conversion(hal_adc_id_t id)
{
    if (!is_adc_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /*
     * 启动ADC软件触发转换
     * ADC配置为自动采样模式，调用后硬件自动完成采样和转换
     */
    DL_ADC12_startConversion(s_adc_inst_map[id]);

    return HAL_OK;
}

hal_status_t hal_adc_read_result(hal_adc_id_t id, uint16_t *result)
{
    /* 参数校验 */
    if (result == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    if (!is_adc_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /*
     * 读取ADC转换结果
     * 12位分辨率，结果范围0~4095
     */
    *result = (uint16_t)DL_ADC12_getMemResult(
        s_adc_inst_map[id], s_adc_mem_map[id]);

    return HAL_OK;
}

bool hal_adc_is_busy(hal_adc_id_t id)
{
    if (!is_adc_valid(id)) {
        return false;
    }

    return (DL_ADC12_getStatus(s_adc_inst_map[id])
        & DL_ADC12_STATUS_CONVERSION_ACTIVE) != 0U;
}

hal_status_t hal_adc_enable_irq(hal_adc_id_t id)
{
    if (!is_adc_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    NVIC_ClearPendingIRQ(s_adc_irq_map[id]);
    NVIC_EnableIRQ(s_adc_irq_map[id]);

    return HAL_OK;
}

hal_status_t hal_adc_disable_irq(hal_adc_id_t id)
{
    if (!is_adc_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    NVIC_DisableIRQ(s_adc_irq_map[id]);
    return HAL_OK;
}
