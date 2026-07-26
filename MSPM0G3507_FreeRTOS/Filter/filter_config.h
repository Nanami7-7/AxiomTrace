/**
 * @file    filter_config.h
 * @brief   ???????? API ???
 *
 * 设计原则：
 *   1. 所有参数标明来源（论文/经验值/传感器手册）
 *   2. 所有参数有有效范围和默认值
 *   3. 提供退化策略（传感器数据质量差时的降级方案）
 *   4. 支持运行时参数验证
 *
 * 参数来源标注格式：
 *   [来源类型] 来源名称 | 推荐值 | 说明
 *   来源类型：PAPER(论文) / EMPIRICAL(经验值) / DATASHEET(数据手册) / TUNED(调优)
 */

#ifndef FILTER_CONFIG_H
#define FILTER_CONFIG_H

#include "filter.h"
#include "filter_tuning.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * API函数声明
 * ============================================================ */

/**
 * @brief 获取参数描述表
 * @param type  滤波器类型
 * @param count 输出参数数量
 * @return 参数描述数组指针（静态存储，无需释放）
 */
const filter_param_desc_t* filter_config_get_params(filter_type_t type, int *count);

/**
 * @brief 获取参数默认值
 * @param type  滤波器类型
 * @param param 参数枚举
 * @return 默认值
 */
float filter_config_get_default(filter_type_t type, filter_param_t param);

/**
 * @brief 验证参数值是否在有效范围内
 * @param type  滤波器类型
 * @param param 参数枚举
 * @param value 参数值
 * @return 1=有效, 0=无效
 */
int filter_config_validate(filter_type_t type, filter_param_t param, float value);

/**
 * @brief 钳位参数值到有效范围
 * @param type  滤波器类型
 * @param param 参数枚举
 * @param value 输入值
 * @return 钳位后的值
 */
float filter_config_clamp(filter_type_t type, filter_param_t param, float value);

/**
 * @brief 获取退化策略配置
 * @param mode  退化模式
 * @return 退化配置指针（静态存储，无需释放）
 */
const degrade_config_t* filter_config_get_degrade(degrade_mode_t mode);

/**
 * @brief 评估传感器数据质量
 * @param ax, ay, az  加速度（g）
 * @param gx, gy, gz  角速度（dps）
 * @return 传感器质量状态
 */
sensor_quality_t filter_config_assess_quality(float ax, float ay, float az,
                                               float gx, float gy, float gz);

/**
 * @brief 根据传感器质量确定退化模式
 * @param acc_quality   加速度计质量
 * @param gyro_quality  陀螺仪质量
 * @return 推荐的退化模式
 */
degrade_mode_t filter_config_select_degrade(sensor_quality_t acc_quality,
                                            sensor_quality_t gyro_quality);

/**
 * @brief 应用预设配置到滤波器
 * @param f       滤波器实例
 * @param preset  预设类型
 */
void filter_config_apply_preset(filter_t *f, filter_preset_t preset);

/**
 * @brief 获取预设配置名称
 * @param preset  预设类型
 * @return 名称字符串
 */
const char* filter_config_preset_name(filter_preset_t preset);

/**
 * @brief 打印参数配置信息（调试用）
 * @param type  滤波器类型
 */
void filter_config_print(filter_type_t type);


#ifdef __cplusplus
}
#endif

#endif /* FILTER_CONFIG_H */
