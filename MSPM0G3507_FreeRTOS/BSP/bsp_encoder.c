/**
 * @file    bsp_encoder.c
 * @brief   编码器驱动实现
 * @note    基于4个TIMG定时器的组合捕获模式实现四路编码器
 *          A相接CAPTURE(CC0/CC1), B相接GPIO输入
 *
 *          ISR调用链:
 *          TIMG8_IRQHandler() → bsp_encoder_irq_handler(BSP_ENCODER_LF)
 *          TIMG7_IRQHandler() → bsp_encoder_irq_handler(BSP_ENCODER_LB)
 *          TIMG6_IRQHandler() → bsp_encoder_irq_handler(BSP_ENCODER_RF)
 *          TIMG0_IRQHandler() → bsp_encoder_irq_handler(BSP_ENCODER_RB)
 *
 *          组合捕获模式(DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP):
 *          - CC0: 脉宽模式, 下降沿触发
 *          - CC1: 周期模式, 上升沿触发
 *
 *          M法测速:
 *          在固定时间窗口内计数编码器脉冲, 由应用层调用
 *          bsp_encoder_get_and_clear_all()获取脉冲增量,
 *          再调用bsp_encoder_counts_to_rpm()换算RPM.
 */
#include "bsp_encoder.h"
#include "hal_timer.h"
#include "hal_gpio.h"
#include "osal_api.h"
#include "ti_msp_dl_config.h"
#include "project_config.h"

/* ======================== 编码器参数 ======================== */

/**
 * @brief  编码器参数(来自硬件规格)
 * @note   每转总脉冲数 = PPR * 减速比 * 倍频数(2)
 *         组合捕获模式下CC0+CC1均为A相边沿触发 → 二倍频
 */

/* 编码器参数集中在project_config.h */

/* ======================== 私有变量 ======================== */

/** 编码器配置表(由应用层传入) */
static const bsp_encoder_config_t *s_encoder_cfg = NULL;

/** 编码器配置数量 */
static uint32_t s_encoder_cfg_count = 0U;

/** 每转总脉冲数 */
static uint32_t s_encoder_pulses_per_rev = 0U;

/** 各编码器累计脉冲计数(ISR写入, 任务读取) */
static volatile int32_t s_encoder_count[BSP_ENCODER_COUNT] = {0};

/** 各编码器方向标志(+1=正转, -1=反转) */
static volatile int8_t s_encoder_sign[BSP_ENCODER_COUNT] = {1, 1, 1, 1};

/** 初始化标志 */
static bool s_encoder_inited = false;

/* ======================== 公共函数实现 ======================== */

bsp_status_t bsp_encoder_init(const bsp_encoder_config_t *cfg,
                               uint32_t count,
                               uint32_t pulses_per_rev)
{
    if (s_encoder_inited) {
        return BSP_OK;
    }

    if ((cfg == NULL) || (count == 0U) ||
        (count > BSP_ENCODER_COUNT) || (pulses_per_rev == 0U)) {
        return BSP_ERR_INVALID_PARAM;
    }

    s_encoder_cfg = cfg;
    s_encoder_cfg_count = count;
    s_encoder_pulses_per_rev = pulses_per_rev;

    /* 清零所有计数 */
    for (uint32_t i = 0; i < s_encoder_cfg_count; i++) {
        s_encoder_count[i]        = 0;
        s_encoder_sign[i]         = 1;
    }

    /* 使能4路编码器定时器的NVIC中断并启动计数 */
    for (uint32_t i = 0; i < s_encoder_cfg_count; i++) {
        hal_status_t ret = hal_timer_enable_irq(
            s_encoder_cfg[i].timer);
        if (ret != HAL_OK) {
            return BSP_ERR_HW_FAULT;
        }

        ret = hal_timer_start(s_encoder_cfg[i].timer);
        if (ret != HAL_OK) {
            return BSP_ERR_HW_FAULT;
        }
    }

    s_encoder_inited = true;
    return BSP_OK;
}

int32_t bsp_encoder_get_count(bsp_encoder_id_t id)
{
    if ((uint32_t)id >= s_encoder_cfg_count) {
        return 0;
    }

    return s_encoder_count[id];
}

bsp_status_t bsp_encoder_get_all_counts(int32_t counts[])
{
    if (counts == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
            counts[i] = (i < s_encoder_cfg_count) ?
                s_encoder_count[i] : 0;
        }
    }

    return BSP_OK;
}

void bsp_encoder_clear_count(bsp_encoder_id_t id)
{
    if ((uint32_t)id >= s_encoder_cfg_count) {
        return;
    }

    OSAL_CRITICAL_SECTION {
        s_encoder_count[id] = 0;
    }
}

void bsp_encoder_clear_all_counts(void)
{
    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0; i < s_encoder_cfg_count; i++) {
            s_encoder_count[i] = 0;
        }
    }
}

int32_t bsp_encoder_get_and_clear_count(bsp_encoder_id_t id)
{
    if ((uint32_t)id >= s_encoder_cfg_count) {
        return 0;
    }

    int32_t delta;

    OSAL_CRITICAL_SECTION {
        delta = s_encoder_count[id];
        s_encoder_count[id] = 0;
    }

    return delta;
}

bsp_status_t bsp_encoder_get_and_clear_all(int32_t deltas[])
{
    if (deltas == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0; i < s_encoder_cfg_count; i++) {
            deltas[i] = s_encoder_count[i];
            s_encoder_count[i] = 0;
        }
        for (uint32_t i = s_encoder_cfg_count;
             i < BSP_ENCODER_COUNT; i++) {
            deltas[i] = 0;
        }
    }

    return BSP_OK;
}

int32_t bsp_encoder_get_rpm(bsp_encoder_id_t id, uint32_t dt_ms)
{
    if ((uint32_t)id >= s_encoder_cfg_count) {
        return 0;
    }

    int32_t delta = bsp_encoder_get_and_clear_count(id);

    /*
     * M法RPM计算:
     *   RPM = (delta / PPR) * (60000ms / dt_ms)
     *   delta        : 本周期脉冲增量
     *   PPR=520      : 每转总脉冲数(PPR*减速比*倍频)
     *   dt_ms        : 采样周期(ms)
     *   60000        : 1分钟=60000ms
     */
    return bsp_encoder_counts_to_rpm(delta, dt_ms);
}

bsp_status_t bsp_encoder_get_all_rpm(int32_t rpms[], uint32_t dt_ms)
{
    if (rpms == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    int32_t deltas[BSP_ENCODER_COUNT];

    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0; i < s_encoder_cfg_count; i++) {
            deltas[i] = s_encoder_count[i];
            s_encoder_count[i] = 0;
        }
        for (uint32_t i = s_encoder_cfg_count;
             i < BSP_ENCODER_COUNT; i++) {
            deltas[i] = 0;
        }
    }

    for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
        rpms[i] = bsp_encoder_counts_to_rpm(deltas[i], dt_ms);
    }

    return BSP_OK;
}

uint32_t bsp_encoder_get_pulses_per_rev(void)
{
    return s_encoder_pulses_per_rev;
}

int32_t bsp_encoder_counts_to_rpm(int32_t delta, uint32_t dt_ms)
{
    if ((dt_ms == 0U) || (s_encoder_pulses_per_rev == 0U)) {
        return 0;
    }

    /* 合并为单次除法, 减少精度损失 */
    int64_t denom = (int64_t)s_encoder_pulses_per_rev
                   * (int64_t)dt_ms;
    if (denom == 0) { return 0; }
    return (int32_t)(((int64_t)delta * 60000) / denom);
}

float bsp_encoder_rpm_to_pulse(float rpm, uint32_t dt_ms)
{
    if ((dt_ms == 0U) || (s_encoder_pulses_per_rev == 0U)) {
        return 0.0f;
    }

    return rpm * (float)s_encoder_pulses_per_rev
         * (float)dt_ms / 60000.0f;
}

void bsp_encoder_irq_handler(bsp_encoder_id_t id)
{
    hal_timer_irq_flag_t flag = hal_timer_get_irq_flag(s_encoder_cfg[id].timer);
    int8_t dir_sign = s_encoder_cfg[id].dir_sign;
    if (dir_sign == 0) dir_sign = 1;

    /* 读取B相电平判别方向 */
    bool b_high = hal_gpio_read_pin(
        s_encoder_cfg[id].b_port,
        s_encoder_cfg[id].b_pin);

    switch (flag) {
    case HAL_TIMER_IRQ_CC0:
        /*
         * CC0中断(下降沿触发)
         * 方向判别: 下降沿时B=高→正转, B=低→反转
         */
        s_encoder_sign[id] = b_high ? 1 : -1;
        s_encoder_count[id] +=
            (int32_t)s_encoder_sign[id]
            * (int32_t)dir_sign;
        break;

    case HAL_TIMER_IRQ_CC1:
        /*
         * CC1中断(上升沿触发)
         * 方向判别: 上升沿时B=高→反转, B=低→正转
         */
        s_encoder_sign[id] = b_high ? -1 : 1;
        s_encoder_count[id] +=
            (int32_t)s_encoder_sign[id]
            * (int32_t)dir_sign;
        break;

    case HAL_TIMER_IRQ_LOAD:
        break;

    default:
        break;
    }
}

void CAPTURE_LEFT_FRONT_INST_IRQHandler(void)
{
    bsp_encoder_irq_handler(BSP_ENCODER_LF);
}

void CAPTURE_LEFT_BACK_INST_IRQHandler(void)
{
    bsp_encoder_irq_handler(BSP_ENCODER_LB);
}

void CAPTURE_RIGHT_FRONT_INST_IRQHandler(void)
{
    bsp_encoder_irq_handler(BSP_ENCODER_RF);
}

void CAPTURE_RIGHT_BACK_INST_IRQHandler(void)
{
    bsp_encoder_irq_handler(BSP_ENCODER_RB);
}