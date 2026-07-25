/**
 * @file    bsp_adc.h
 * @brief   ADC采样驱动接口
 * @note    基于hal_adc实现ADC1多通道采样
 *          ADC1已由SYSCFG_DL_init()配置: 12位, 5通道(MEM0~MEM4), VDDA参考3.3V
 *          MEM0(PA15/ch0) → 电机1电流
 *          MEM1(PA16/ch1) → 电机2电流
 *          MEM2(PA17/ch2) → 电机3电流
 *          MEM3(PA22/ch8) → 电机4电流
 *          MEM4(PA27/ch0) → 母线电压
 *
 *          电流换算: I(mA) = raw × VREF_mV / RESOLUTION / SHUNT_OHM / AMPLIFY
 *                    = raw × 3300 / 4096 / 0.15 / 10 = raw × 0.537 mA
 *
 *          采样时序: sampleTime=5cycles @ 4MHz ADC clock = 1.25us/通道
 *                    5通道总转换时间 ≈ 6.25us, 适合5ms控制环内非阻塞采样
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
    BSP_ADC_CH_CURRENT_1 = 0,  /**< 电机1电流采样通道(MEM0/PA15) */
    BSP_ADC_CH_CURRENT_2,      /**< 电机2电流采样通道(MEM1/PA16) */
    BSP_ADC_CH_CURRENT_3,      /**< 电机3电流采样通道(MEM2/PA17) */
    BSP_ADC_CH_CURRENT_4,      /**< 电机4电流采样通道(MEM3/PA22) */
    BSP_ADC_CH_VOLTAGE,        /**< 母线电压采样通道(MEM4/PA27) */
    BSP_ADC_CH_COUNT           /**< 通道总数 */
} bsp_adc_channel_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化ADC驱动
 * @note   使能ADC1中断(MEM4完成中断), ADC已由SYSCFG_DL_init()配置
 * @retval BSP_OK 初始化成功
 */
bsp_status_t bsp_adc_init(void);

/**
 * @brief  启动ADC多通道转换(非阻塞)
 * @note   启动后约6us结果就绪, 通过中断或bsp_adc_all_done()查询
 *         适合在5ms控制环内调用
 * @retval BSP_OK 启动成功
 */
bsp_status_t bsp_adc_start_all(void);

/**
 * @brief  查询ADC多通道转换是否完成
 * @retval true  转换完成, 可读取结果
 * @retval false 转换未完成
 */
bool bsp_adc_is_conversion_done(void);

/**
 * @brief  清除ADC转换完成标志
 * @note   每次读取结果后调用, 准备下一次转换
 */
void bsp_adc_clear_done_flag(void);

/**
 * @brief  ADC中断处理函数
 * @note   在ADC1_IRQHandler中调用, 一次性读取5个MEM结果
 */
void bsp_adc_irq_handler(void);

/**
 * @brief  获取最近一次转换的原始值(中断模式)
 * @param  channel ADC通道编号
 * @retval 原始ADC值(0~4095), 未转换时返回0
 */
uint16_t bsp_adc_get_last_raw(bsp_adc_channel_t channel);

/**
 * @brief  获取最近一次转换的电压值(mV)
 * @param  channel ADC通道编号
 * @retval 电压值(mV), 未转换时返回0
 */
uint32_t bsp_adc_get_last_voltage(bsp_adc_channel_t channel);

/**
 * @brief  获取指定电机的电流原始值
 * @param  motor_idx 电机索引(0~3)
 * @retval 原始ADC值(0~4095)
 */
uint16_t bsp_adc_get_last_current_raw(uint8_t motor_idx);

/**
 * @brief  获取指定电机的电流值(mA)
 * @param  motor_idx 电机索引(0~3)
 * @retval 电流值(mA)
 */
float bsp_adc_get_last_current_ma(uint8_t motor_idx);

/**
 * @brief  获取所有4路电机电流值(mA)
 * @param  currents_ma 输出数组, 长度需>=4
 */
void bsp_adc_get_all_currents_ma(float currents_ma[4]);

/**
 * @brief  获取母线电压值(mV)
 * @retval 母线电压(mV)
 */
uint32_t bsp_adc_get_bus_voltage_mv(void);

/* ---- 以下为旧接口, 保留向后兼容 ---- */

/**
 * @brief  启动ADC单次转换并读取结果(阻塞轮询)
 * @note   仅支持电压通道, 高频采样建议使用bsp_adc_start_all
 * @param  channel ADC通道编号
 * @param  raw_val 原始ADC值输出指针(12位, 0~4095)
 * @retval BSP_OK           读取成功
 * @retval BSP_ERR_NULL_PTR raw_val为空
 * @retval BSP_ERR_TIMEOUT  转换超时
 */
bsp_status_t bsp_adc_read_raw(bsp_adc_channel_t channel,
                                uint16_t *raw_val);

/**
 * @brief  启动ADC单次转换并读取电压值(mV, 阻塞轮询)
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
 * @param  channel ADC通道编号
 * @retval BSP_OK 启动成功
 */
bsp_status_t bsp_adc_start_conversion(bsp_adc_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_H */
