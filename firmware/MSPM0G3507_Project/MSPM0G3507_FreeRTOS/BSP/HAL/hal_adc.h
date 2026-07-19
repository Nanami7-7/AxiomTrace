/**
 * @file    hal_adc.h
 * @brief   HAL ADC硬件抽象接口
 *
 * @note    封装DL_ADC12_xxx调用，ADC0已在SYSCFG_DL_init()中
 *          完成时钟/分辨率/通道配置，此接口仅提供运行时采样操作
 */
#ifndef HAL_ADC_H
#define HAL_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "hal_common.h"

/* ======================== 函数接口 ======================== */

/**
 * @brief  启动ADC单次软件转换
 * @note   调用后需等待转换完成再读取结果
 * @param  id ADC实例编号
 * @retval HAL_OK           启动成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_adc_start_conversion(hal_adc_id_t id);

/**
 * @brief  读取ADC转换结果
 * @note   应在转换完成后调用，否则可能读取到旧数据
 * @param  id     ADC实例编号
 * @param  result 转换结果存放指针(12位无符号值:0~4095)
 * @retval HAL_OK           读取成功
 * @retval HAL_ERR_INVALID_PARAM 参数无效
 */
hal_status_t hal_adc_read_result(hal_adc_id_t id, uint16_t *result);

/**
 * @brief  查询ADC是否正在转换
 * @param  id ADC实例编号
 * @retval true  转换进行中
 * @retval false 转换完成或空闲
 */
bool hal_adc_is_busy(hal_adc_id_t id);

/**
 * @brief  使能ADC中断
 * @param  id ADC实例编号
 * @retval HAL_OK           使能成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_adc_enable_irq(hal_adc_id_t id);

/**
 * @brief  禁止ADC中断
 * @param  id ADC实例编号
 * @retval HAL_OK           禁止成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_adc_disable_irq(hal_adc_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ADC_H */
