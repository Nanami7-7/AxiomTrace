/**
 * @file    bsp_encoder.h
 * @brief   编码器驱动接口
 * @note    基于TIMG7/TIMA1/TIMG6/TIMG0四个定时器的CC0捕获模式实现四路编码器
 *          A相接CAPTURE(CC0), B相接GPIO输入
 *
 *          判向原理(CC0中断触发时):
 *          读取B相GPIO电平判别旋转方向
 *          - B=高 → 正转(count++)
 *          - B=低 → 反转(count--)
 *
 *          如果定时器配置为组合捕获模式(同时使能CC0+CC1),
 *          则CC0为A相下降沿, CC1为A相上升沿, 等效二倍频
 */
#ifndef BSP_ENCODER_H
#define BSP_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "bsp_common.h"
#include "hal_gpio.h"
#include "hal_timer.h"

/* ======================== 类型定义 ======================== */

/** 编码器编号枚举 */
typedef enum {
    BSP_ENCODER_LF = 0,  /**< 左前编码器(TIMG7) */
    BSP_ENCODER_LB,      /**< 左后编码器(TIMA1) */
    BSP_ENCODER_RF,      /**< 右前编码器(TIMG6) */
    BSP_ENCODER_RB,      /**< 右后编码器(TIMG0) */
    BSP_ENCODER_COUNT    /**< 编码器总数 */
} bsp_encoder_id_t;

/** 编码器数据结构体(预留) */
typedef struct {
    int32_t  count;   /**< 累计脉冲计数(有符号, 正=正转) */
} bsp_encoder_data_t;

/** 编码器配置结构体 */
typedef struct {
    hal_timer_id_t  timer;    /**< 关联的捕获定时器 */
    hal_gpio_port_t b_port;   /**< B相GPIO端口 */
    uint32_t        b_pin;    /**< B相GPIO引脚 */
    int8_t          dir_sign; /**< 方向修正(+1/-1, 0视为+1) */
} bsp_encoder_config_t;

/** 编码器诊断快照(ISR变量快照,不清零) */
typedef struct {
    int32_t  count;      /**< s_encoder_count */
    int32_t  total;      /**< s_encoder_total */
    int8_t   sign;       /**< s_encoder_sign */
    bool     has_edge;   /**< s_mt_has_edge */
    uint16_t first_cnt;  /**< s_mt_first_cnt */
    uint16_t last_cnt;   /**< s_mt_last_cnt */
    int32_t  last_dir;   /**< s_mt_last_dir */
    uint16_t period;     /**< s_mt_last_period */
    uint32_t overflow;   /**< s_mt_overflow_cnt */
    uint32_t last_abs;   /**< s_mt_last_abs */
    uint32_t time_since; /**< 距末边沿的ticks(当前timer - last_abs), 诊断用 */
} bsp_encoder_diag_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化编码器驱动
 * @param  cfg  编码器配置表
 * @param  count 配置数量(<=BSP_ENCODER_COUNT)
 * @param  pulses_per_rev 每转总脉冲数
 * @note   使能配置数量的编码器定时器NVIC中断, 清零计数
 *         定时器已由SYSCFG_DL_init()配置为组合捕获模式
 * @retval BSP_OK 初始化成功
 */
bsp_status_t bsp_encoder_init(const bsp_encoder_config_t *cfg,
                               uint32_t count,
                               uint32_t pulses_per_rev);

/**
 * @brief  获取单个编码器累计脉冲计数(不清零)
 * @param  id 编码器编号
 * @retval 累计脉冲值(正=正转, 负=反转)
 */
int32_t bsp_encoder_get_count(bsp_encoder_id_t id);

/**
 * @brief  获取所有编码器累计脉冲计数(不清零)
 * @param  counts 输出数组, 长度需>=BSP_ENCODER_COUNT
 * @retval BSP_OK           成功
 * @retval BSP_ERR_NULL_PTR counts为空
 */
bsp_status_t bsp_encoder_get_all_counts(int32_t counts[]);

/**
 * @brief  获取所有编码器累计总脉冲(不清零, 不受RPM周期清零影响)
 * @param  totals 输出数组, 长度需>=BSP_ENCODER_COUNT
 * @retval BSP_OK           成功
 * @retval BSP_ERR_NULL_PTR totals为空
 */
bsp_status_t bsp_encoder_get_all_totals(int32_t totals[]);

/**
 * @brief  清零单个编码器累计脉冲
 * @param  id 编码器编号
 */
void bsp_encoder_clear_count(bsp_encoder_id_t id);

/**
 * @brief  清零所有编码器累计脉冲
 */
void bsp_encoder_clear_all_counts(void);

/**
 * @brief  读取并清零单个编码器脉冲增量
 * @note   典型用法: 固定周期测速任务中, 读取增量后清零
 * @param  id 编码器编号
 * @retval 脉冲增量值
 */
int32_t bsp_encoder_get_and_clear_count(bsp_encoder_id_t id);

/**
 * @brief  读取并清零所有编码器脉冲增量
 * @param  deltas 输出数组, 长度需>=BSP_ENCODER_COUNT
 * @retval BSP_OK           成功
 * @retval BSP_ERR_NULL_PTR deltas为空
 */
bsp_status_t bsp_encoder_get_and_clear_all(int32_t deltas[]);

/**
 * @brief  获取单个编码器的转速(RPM)
 * @note   基于M法测速: 读取并清零脉冲增量, 换算RPM
 *         RPM = (delta / PPR) * (60000 / dt_ms)
 * @param  id    编码器编号
 * @param  dt_ms 采样周期(毫秒)
 * @retval 转速值(RPM), 正=正转, 负=反转
 */
int32_t bsp_encoder_get_rpm(bsp_encoder_id_t id, uint32_t dt_ms);

/**
 * @brief  获取所有编码器的转速(RPM)
 * @note   基于M法测速: 读取并清零脉冲增量, 换算RPM
 * @param  rpms  输出数组, 长度需>=BSP_ENCODER_COUNT
 * @param  dt_ms 采样周期(毫秒)
 * @retval BSP_OK           成功
 * @retval BSP_ERR_NULL_PTR rpms为空
 */
bsp_status_t bsp_encoder_get_all_rpm(int32_t rpms[], uint32_t dt_ms);

/**
 * @brief  获取所有编码器的转速(RPM) - M/T法
 * @note   基于M/T法测速: 高速用M/T混合, 低速用T法, 无脉冲时平滑衰减
 *         had_edge输出可用于堵转检测
 * @param  rpms     输出数组, 长度需>=BSP_ENCODER_COUNT
 * @param  had_edge 输出数组, 指示本周期是否有边沿, 可为NULL
 * @retval BSP_OK           成功
 * @retval BSP_ERR_NULL_PTR rpms为空
 */
bsp_status_t bsp_encoder_get_all_rpm_mt(int32_t rpms[], bool had_edge[]);

/**
 * @brief  获取编码器每转总脉冲数
 * @retval 每转总脉冲数
 */
uint32_t bsp_encoder_get_pulses_per_rev(void);

/**
 * @brief  脉冲增量转RPM
 * @param  delta 脉冲增量
 * @param  dt_ms 采样周期(ms)
 * @retval RPM值
 */
int32_t bsp_encoder_counts_to_rpm(int32_t delta, uint32_t dt_ms);

/**
 * @brief  RPM转脉冲增量(指定采样周期)
 * @param  rpm   转速(RPM)
 * @param  dt_ms 采样周期(ms)
 * @retval 脉冲增量(浮点)
 */
float bsp_encoder_rpm_to_pulse(float rpm, uint32_t dt_ms);

/**
 * @brief  读取编码器诊断快照(不清零)
 * @note   在临界区内读取ISR变量的当前快照,不清零任何变量
 *         用于调试: 手动重现M/T计算,排查RPM=0的原因
 * @param  id  编码器编号
 * @param  out 输出结构体指针
 * @retval BSP_OK             成功
 * @retval BSP_ERR_INVALID_PARAM id超出范围或out为NULL
 */
bsp_status_t bsp_encoder_get_diag(bsp_encoder_id_t id,
                                   bsp_encoder_diag_t *out);

/**
 * @brief  编码器捕获中断处理函数
 * @note   在各TIMG_IRQHandler中调用, 传入编码器编号
 *         ISR中仅更新计数和方向标志, 不做浮点运算
 * @param  id 编码器编号
 */
void bsp_encoder_irq_handler(bsp_encoder_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ENCODER_H */
