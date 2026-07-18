/**
 * @file    bsp_adc.h
 * @brief   ADC采样驱动接口
 * @note    基于hal_adc实现ADC0单次采样和电压转换
 *          ADC0已由SYSCFG_DL_init()配置: 12位, 通道0(PA27), VDDA参考3.3V
 */
#ifndef BSP_ADC_H
#define BSP_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "bsp_common.h"

/* ======================== 类型定义 ======================== */

/** ADC通道编号枚举 */
typedef enum {
    BSP_ADC_CH_VOLTAGE = 0,  /**< 电压采样通道 */
    BSP_ADC_CH_COUNT         /**< 通道总数 */
} bsp_adc_channel_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化ADC驱动
 * @note   使能ADC0中断(如需中断模式), ADC已由SYSCFG_DL_init()配置
 * @retval BSP_OK 初始化成功
 */
bsp_status_t bsp_adc_init(void);

/**
 * @brief  启动ADC单次转换并读取结果(阻塞轮询)
 * @note   启动转换后轮询等待完成, 适用于低频采样场景
 *         高频采样建议使用中断模式
 * @param  channel ADC通道编号
 * @param  raw_val 原始ADC值输出指针(12位: 0~4095)
 * @retval BSP_OK           读取成功
 * @retval BSP_ERR_NULL_PTR raw_val为空
 * @retval BSP_ERR_TIMEOUT  转换超时
 */
bsp_status_t bsp_adc_read_raw(bsp_adc_channel_t channel,
                                uint16_t *raw_val);

/**
 * @brief  启动ADC单次转换并读取电压值(mV)
 * @note   基于raw值和参考电压换算: voltage = raw * VREF / 4096
 * @param  channel  ADC通道编号
 * @param  voltage  电压值输出指针(单位:mV)
 * @retval BSP_OK           读取成功
 * @retval BSP_ERR_NULL_PTR voltage为空
 * @retval BSP_ERR_TIMEOUT  转换超时
 */
bsp_status_t bsp_adc_read_voltage(bsp_adc_channel_t channel,
                                    uint32_t *voltage);

/**
 * @brief  启动ADC转换(非阻塞, 配合中断使用)
 * @note   调用后ADC开始转换, 结果通过中断回调获取
 * @param  channel ADC通道编号
 * @retval BSP_OK 启动成功
 */
bsp_status_t bsp_adc_start_conversion(bsp_adc_channel_t channel);

/**
 * @brief  ADC中断处理函数
 * @note   在ADC0_IRQHandler中调用
 *         ISR中仅保存原始值, 不做浮点运算
 */
void bsp_adc_irq_handler(void);

/**
 * @brief  获取最近一次ADC转换的原始值(中断模式下使用)
 * @param  channel ADC通道编号
 * @retval 最近一次转换的原始值(0~4095), 未转换时返回0
 */
uint16_t bsp_adc_get_last_raw(bsp_adc_channel_t channel);

/**
 * @brief  获取最近一次ADC转换的电压值(mV, 中断模式下使用)
 * @param  channel ADC通道编号
 * @retval 电压值(mV), 未转换时返回0
 */
uint32_t bsp_adc_get_last_voltage(bsp_adc_channel_t channel);

/**
 * @brief  查询ADC转换是否完成(中断模式下使用)
 * @retval true  转换完成, 可读取结果
 * @retval false 转换未完成
 */
bool bsp_adc_is_conversion_done(void);

/**
 * @brief  清除ADC转换完成标志
 * @note   每次读取结果后调用, 准备下一次转换
 */
void bsp_adc_clear_done_flag(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_H */
