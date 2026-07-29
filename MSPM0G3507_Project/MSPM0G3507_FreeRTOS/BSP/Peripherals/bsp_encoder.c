/**
 * @file    bsp_encoder.c
 * 说明：编码器测速相关处理。
 *
 * 说明：编码器测速相关处理。
 * 说明：编码器测速相关处理。
 * 说明：编码器测速相关处理。
 * 说明：编码器测速相关处理。
 * 说明：编码器测速相关处理。
 *
 *          组合捕获模式(DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP):
 * 说明：编码器测速相关处理。
 * 说明：编码器测速相关处理。
 * 说明：编码器测速相关处理。
 *          bsp_encoder_get_and_clear_all()获取脉冲增量,
 *          再调用bsp_encoder_counts_to_rpm()换算RPM.
 */
#include "bsp_encoder.h"
#include "hal_timer.h"
#include "hal_gpio.h"
#include "osal_api.h"
#include "ti_msp_dl_config.h"
#include "project_config.h"
/* 说明：编码器测速相关处理。 */

/**
 * 说明：编码器测速相关处理。
 * 说明：编码器测速相关处理。
 * 说明：编码器测速相关处理。
 */

/* 编码器参数集中在project_config.h */

/* ======================== 私有变量 ======================== */

/** 编码器配置表(由应用层传入) */
static const bsp_encoder_config_t *s_encoder_cfg = NULL;

/* 说明：编码器测速相关处理。 */
static uint32_t s_encoder_cfg_count = 0U;

/** 每转总脉冲数 */
static uint32_t s_encoder_pulses_per_rev = 0U;

/** 各编码器累计脉冲计数(ISR写入, 任务读取) */
static volatile int32_t s_encoder_count[BSP_ENCODER_COUNT] = {0};

/** 各编码器方向标志(+1=正转, -1=反转) */
static volatile int8_t s_encoder_sign[BSP_ENCODER_COUNT] = {1, 1, 1, 1};

/* 说明：编码器测速相关处理。 */
static volatile int32_t s_encoder_total[BSP_ENCODER_COUNT] = {0};

/* 说明：编码器测速相关处理。 */
/* 说明：编码器测速相关处理。 */
static volatile uint16_t s_mt_first_cnt[BSP_ENCODER_COUNT];
/* 说明：编码器测速相关处理。 */
static volatile uint16_t s_mt_last_cnt[BSP_ENCODER_COUNT];
/* 说明：编码器测速相关处理。 */
static volatile int32_t  s_mt_last_dir[BSP_ENCODER_COUNT];
/** 窗口内是否有边沿 */
static volatile bool     s_mt_has_edge[BSP_ENCODER_COUNT];
/* 说明：编码器测速相关处理。 */
static volatile uint32_t s_mt_last_period[BSP_ENCODER_COUNT];
/** timer溢出计数 */
static volatile uint32_t s_mt_overflow_cnt[BSP_ENCODER_COUNT];
/* 说明：编码器测速相关处理。 */
static volatile uint32_t s_mt_last_abs[BSP_ENCODER_COUNT];

/* Factory capture diagnostics. These are observation-only counters. */
static volatile uint32_t s_cap_cc0_count[BSP_ENCODER_COUNT];
static volatile uint32_t s_cap_cc1_count[BSP_ENCODER_COUNT];
static volatile uint32_t s_cap_load_count[BSP_ENCODER_COUNT];
static volatile uint16_t s_cap_cc0_value[BSP_ENCODER_COUNT];
static volatile uint16_t s_cap_cc1_value[BSP_ENCODER_COUNT];
static volatile uint16_t s_cap_cc1_previous[BSP_ENCODER_COUNT];
static volatile uint16_t s_cap_cc0_timer[BSP_ENCODER_COUNT];
static volatile uint16_t s_cap_cc1_timer[BSP_ENCODER_COUNT];
static volatile uint16_t s_cap_load_timer[BSP_ENCODER_COUNT];
static volatile uint32_t s_cap_cc1_delta[BSP_ENCODER_COUNT];
static volatile uint32_t s_cap_cc1_last_abs[BSP_ENCODER_COUNT];
static volatile bool s_cap_cc1_seen[BSP_ENCODER_COUNT];
static volatile bool s_cap_cc1_valid[BSP_ENCODER_COUNT];
/* M/T测速模式：M=固定窗口计数，T=边沿周期，STOP=超时停止。 */
typedef enum {
    BSP_ENCODER_SPEED_MODE_STOP = 0,
    BSP_ENCODER_SPEED_MODE_M,
    BSP_ENCODER_SPEED_MODE_T
} bsp_encoder_speed_mode_t;

static volatile bsp_encoder_speed_mode_t s_mt_speed_mode[BSP_ENCODER_COUNT];
static volatile uint8_t s_mt_m_enter_cycles[BSP_ENCODER_COUNT];
static volatile uint8_t s_mt_t_enter_cycles[BSP_ENCODER_COUNT];

/* 说明：编码器测速相关处理。 */
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

    /* 清零扢有计数及M/T变量 */
    for (uint32_t i = 0; i < s_encoder_cfg_count; i++) {
        s_encoder_count[i]    = 0;
        s_encoder_total[i]    = 0;
        s_encoder_sign[i]     = 1;
        s_mt_first_cnt[i]     = 0;
        s_mt_last_cnt[i]      = 0;
        s_mt_last_dir[i]      = 0;
        s_mt_has_edge[i]      = false;
        s_mt_last_period[i]   = 0U;
        s_mt_speed_mode[i]     = BSP_ENCODER_SPEED_MODE_STOP;
        s_mt_m_enter_cycles[i] = 0U;
        s_mt_t_enter_cycles[i] = 0U;
        s_mt_overflow_cnt[i]  = 0U;
        s_mt_last_abs[i]      = 0;
        s_cap_cc0_count[i]    = 0;
        s_cap_cc1_count[i]    = 0;
        s_cap_load_count[i]   = 0;
        s_cap_cc0_value[i]    = 0;
        s_cap_cc1_value[i]    = 0;
        s_cap_cc1_previous[i] = 0;
        s_cap_cc0_timer[i]    = 0;
        s_cap_cc1_timer[i]    = 0;
        s_cap_load_timer[i]   = 0;
        s_cap_cc1_delta[i]    = 0;
        s_cap_cc1_last_abs[i] = 0;
        s_cap_cc1_seen[i]     = false;
        s_cap_cc1_valid[i]    = false;
    }

    /* 说明：编码器测速相关处理。 */
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

bsp_status_t bsp_encoder_get_all_totals(int32_t totals[])
{
    if (totals == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
            totals[i] = (i < s_encoder_cfg_count) ?
                s_encoder_total[i] : 0;
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
     * M 法 RPM 计算:
     *   RPM = (delta / PPR) * (60000ms / dt_ms)
     * 说明：编码器测速相关处理。
     *   dt_ms        : 采样周期(ms)
     *   60000        : 1 分钟 = 60000 ms
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

    /* 说明：编码器测速相关处理。 */
    int64_t denom = (int64_t)s_encoder_pulses_per_rev
                   * (int64_t)dt_ms;
    if (denom == 0) { return 0; }
    return (int32_t)(((int64_t)delta * (int64_t)PRJ_MS_PER_MIN) / denom);
}

float bsp_encoder_rpm_to_pulse(float rpm, uint32_t dt_ms)
{
    if ((dt_ms == 0U) || (s_encoder_pulses_per_rev == 0U)) {
        return 0.0f;
    }

    return rpm * (float)s_encoder_pulses_per_rev
         * (float)dt_ms / (float)PRJ_MS_PER_MIN;
}

/* ======================== 诊断接口 ======================== */

bsp_status_t bsp_encoder_get_diag(bsp_encoder_id_t id,
                                   bsp_encoder_diag_t *out)
{
    if ((uint32_t)id >= s_encoder_cfg_count) {
        return BSP_ERR_INVALID_PARAM;
    }
    if (out == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    OSAL_CRITICAL_SECTION {
        out->count     = s_encoder_count[id];
        out->total     = s_encoder_total[id];
        out->sign      = s_encoder_sign[id];
        out->has_edge  = s_mt_has_edge[id];
        out->first_cnt = s_mt_first_cnt[id];
        out->last_cnt  = s_mt_last_cnt[id];
        out->last_dir  = s_mt_last_dir[id];
        out->period    = s_mt_last_period[id];
        out->overflow  = s_mt_overflow_cnt[id];
        out->last_abs  = s_mt_last_abs[id];
        out->cc0_event_count      = s_cap_cc0_count[id];
        out->cc1_event_count      = s_cap_cc1_count[id];
        out->load_event_count     = s_cap_load_count[id];
        out->cc0_capture          = s_cap_cc0_value[id];
        out->cc1_capture          = s_cap_cc1_value[id];
        out->cc1_previous_capture = s_cap_cc1_previous[id];
        out->cc0_timer_count      = s_cap_cc0_timer[id];
        out->cc1_timer_count      = s_cap_cc1_timer[id];
        out->load_timer_count     = s_cap_load_timer[id];
        out->cc1_delta            = s_cap_cc1_delta[id];
        out->cc1_last_abs         = s_cap_cc1_last_abs[id];
        out->cc1_period_valid     = s_cap_cc1_valid[id];
        /* Read both encoder pads without changing mux, pull, timer, or counters. */
        out->a_level = hal_gpio_read_pin(s_encoder_cfg[id].a_port,
                                         s_encoder_cfg[id].a_pin);
        out->b_level = hal_gpio_read_pin(s_encoder_cfg[id].b_port,
                                         s_encoder_cfg[id].b_pin);
        {
            uint16_t now_cnt = (uint16_t)hal_timer_get_count(
                s_encoder_cfg[id].timer);
            uint32_t now_abs = (s_mt_overflow_cnt[id] << 16) | now_cnt;
            out->time_since = now_abs - out->last_abs;
        }
    }
    /* 说明：编码器测速相关处理。 */

    return BSP_OK;
}

/* 说明：编码器测速相关处理。 */

bsp_status_t bsp_encoder_get_all_rpm_mt(int32_t rpms[], bool had_edge[])
{
    if (rpms == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0U; i < s_encoder_cfg_count; i++) {
            const int32_t m_count = s_encoder_count[i];
            const uint32_t abs_m = (m_count < 0) ?
                (uint32_t)(-m_count) : (uint32_t)m_count;
            const bool has_edge = s_mt_has_edge[i];
            const uint16_t first_cnt = s_mt_first_cnt[i];
            const uint16_t last_cnt = s_mt_last_cnt[i];
            uint32_t period = s_mt_last_period[i];
            const int32_t last_dir = s_mt_last_dir[i];
            const uint32_t last_abs = s_mt_last_abs[i];
            const uint16_t now_cnt = (uint16_t)hal_timer_get_count(
                s_encoder_cfg[i].timer);
            const uint32_t now_abs = (s_mt_overflow_cnt[i] << 16) |
                                     (uint32_t)now_cnt;
            const uint32_t time_since = now_abs - last_abs;
            const uint32_t stop_ticks = (uint32_t)(
                ((uint64_t)PRJ_CAPTURE_TIMER_FREQ_HZ *
                 (uint64_t)PRJ_ENCODER_STOP_TIMEOUT_MS) /
                (uint64_t)PRJ_MS_PER_S);

            s_encoder_count[i] = 0;
            s_mt_has_edge[i] = false;

            if (had_edge != NULL) {
                had_edge[i] = has_edge;
            }

            /* 100ms 无边沿直接判停，避免旧周期值长时间拖尾。 */
            if ((last_dir == 0) ||
                ((stop_ticks > 0U) && (time_since >= stop_ticks))) {
                s_mt_speed_mode[i] = BSP_ENCODER_SPEED_MODE_STOP;
                s_mt_m_enter_cycles[i] = 0U;
                s_mt_t_enter_cycles[i] = 0U;
                rpms[i] = 0;
                continue;
            }

            if (has_edge &&
                (s_mt_speed_mode[i] == BSP_ENCODER_SPEED_MODE_STOP)) {
                s_mt_speed_mode[i] = BSP_ENCODER_SPEED_MODE_T;
            }

            /* 连续多个窗口满足条件后再切换，防止阈值附近 T/M 来回跳。 */
            if (has_edge &&
                (abs_m >= PRJ_ENCODER_SPEED_MODE_ENTER_M_COUNT)) {
                if (s_mt_m_enter_cycles[i] < 0xFFU) {
                    s_mt_m_enter_cycles[i]++;
                }
                s_mt_t_enter_cycles[i] = 0U;
                if (s_mt_m_enter_cycles[i] >=
                    PRJ_ENCODER_SPEED_MODE_CONFIRM_CYCLES) {
                    s_mt_speed_mode[i] = BSP_ENCODER_SPEED_MODE_M;
                }
            } else if (abs_m <= PRJ_ENCODER_SPEED_MODE_EXIT_M_COUNT) {
                if (s_mt_t_enter_cycles[i] < 0xFFU) {
                    s_mt_t_enter_cycles[i]++;
                }
                s_mt_m_enter_cycles[i] = 0U;
                if (s_mt_t_enter_cycles[i] >=
                    PRJ_ENCODER_SPEED_MODE_CONFIRM_CYCLES) {
                    s_mt_speed_mode[i] = BSP_ENCODER_SPEED_MODE_T;
                }
            } else {
                s_mt_m_enter_cycles[i] = 0U;
                s_mt_t_enter_cycles[i] = 0U;
            }

            int32_t rpm = 0;

            if ((s_mt_speed_mode[i] == BSP_ENCODER_SPEED_MODE_M) &&
                (abs_m >= 2U)) {
                /* M 个边沿之间只有 M-1 个时间间隔。 */
                const uint32_t interval_count = abs_m - 1U;
                const int32_t direction = (m_count < 0) ? -1 : 1;
                const int16_t diff = (int16_t)(last_cnt - first_cnt);
                uint32_t t_ref = (diff > 0) ?
                    (uint32_t)diff : (uint32_t)(diff + PRJ_UINT16_MOD);
                if (t_ref == 0U) {
                    t_ref = 1U;
                }

                const int64_t num = (int64_t)direction *
                    (int64_t)interval_count * PRJ_ENCODER_RPM_CALC_CONST;
                const int64_t den = (int64_t)t_ref *
                    (int64_t)s_encoder_pulses_per_rev;
                rpm = (int32_t)(num / den);
            } else if (period != 0U) {
                /* T 法周期是 A 相上升沿到上升沿，每转周期数是双边沿计数的一半。 */
                if (!has_edge && (time_since > period)) {
                    period = time_since;
                }
                const int64_t num = (int64_t)last_dir *
                    PRJ_ENCODER_RPM_CALC_CONST;
                const int64_t den = (int64_t)period *
                    (int64_t)PRJ_ENCODER_PERIODS_PER_OUTPUT_REV;
                rpm = (int32_t)(num / den);
            }

            rpms[i] = rpm;
        }

        for (uint32_t i = s_encoder_cfg_count;
             i < BSP_ENCODER_COUNT; i++) {
            rpms[i] = 0;
            if (had_edge != NULL) {
                had_edge[i] = false;
            }
        }
    }

    return BSP_OK;
}
/* ======================== ISR 实现 ======================== */

void bsp_encoder_irq_handler(bsp_encoder_id_t id)
{
    hal_timer_irq_flag_t flag = hal_timer_get_irq_flag(s_encoder_cfg[id].timer);
    int8_t dir_sign = s_encoder_cfg[id].dir_sign;
    if (dir_sign == 0) dir_sign = 1;

    /* 说明：编码器测速相关处理。 */
    bool b_high = hal_gpio_read_pin(
        s_encoder_cfg[id].b_port,
        s_encoder_cfg[id].b_pin);

    switch (flag) {
    case HAL_TIMER_IRQ_CC0:
        /* Observation only: capture register plus timer count at ISR entry. */
        s_cap_cc0_count[id]++;
        s_cap_cc0_value[id] = (uint16_t)hal_timer_get_capture_value(
            s_encoder_cfg[id].timer, 0U);
        s_cap_cc0_timer[id] = (uint16_t)hal_timer_get_count(
            s_encoder_cfg[id].timer);
        /*
         * 说明：编码器测速相关处理。
         * 方向判别: 下降沿时B=高→正转, B=低→反转
         */
        s_encoder_sign[id] = b_high ? 1 : -1;
        {
            int32_t inc = (int32_t)s_encoder_sign[id] * (int32_t)dir_sign;
            s_encoder_count[id] += inc;
            s_encoder_total[id] += inc;
        }
        {
            /* 说明：编码器测速相关处理。 */
            uint16_t cnt = (uint16_t)hal_timer_get_count(
                s_encoder_cfg[id].timer);
            if (!s_mt_has_edge[id]) {
                s_mt_first_cnt[id] = cnt;
                s_mt_has_edge[id] = true;
            }
            s_mt_last_cnt[id] = cnt;
            s_mt_last_abs[id] =
                (s_mt_overflow_cnt[id] << 16) | cnt;
            s_mt_last_dir[id] = (int32_t)s_encoder_sign[id] * (int32_t)dir_sign;
        }
        break;

    case HAL_TIMER_IRQ_CC1:
        /* Observation only: CC1 is a hardware timestamp, not a period. */
        s_cap_cc1_count[id]++;
        {
            uint16_t capture = (uint16_t)hal_timer_get_capture_value(
                s_encoder_cfg[id].timer, 1U);
            uint32_t current_abs = (s_mt_overflow_cnt[id] << 16) |
                                   (uint32_t)capture;
            s_cap_cc1_timer[id] = (uint16_t)hal_timer_get_count(
                s_encoder_cfg[id].timer);
            /* The first CC1 edge establishes a timestamp; the second
             * edge is the first valid period measurement. */
            if (s_cap_cc1_seen[id]) {
                s_cap_cc1_previous[id] = s_cap_cc1_value[id];
                s_cap_cc1_delta[id] = current_abs - s_cap_cc1_last_abs[id];
                s_cap_cc1_valid[id] = true;
                /* CC1 保存的是上升沿时间戳，连续两次时间戳之差才是周期。 */
                s_mt_last_period[id] = s_cap_cc1_delta[id];
            }
            s_cap_cc1_value[id] = capture;
            s_cap_cc1_last_abs[id] = current_abs;
            s_cap_cc1_seen[id] = true;
        }
        /*
         * 说明：编码器测速相关处理。
         * 方向判别: 上升沿时B=高→反转, B=低→正转
         */
        s_encoder_sign[id] = b_high ? -1 : 1;
        {
            int32_t inc = (int32_t)s_encoder_sign[id] * (int32_t)dir_sign;
            s_encoder_count[id] += inc;
            s_encoder_total[id] += inc;
        }
        {
            /* 说明：编码器测速相关处理。 */
            uint16_t cnt = (uint16_t)hal_timer_get_count(
                s_encoder_cfg[id].timer);
            if (!s_mt_has_edge[id]) {
                s_mt_first_cnt[id] = cnt;
                s_mt_has_edge[id] = true;
            }
            s_mt_last_cnt[id] = cnt;
            s_mt_last_abs[id] =
                (s_mt_overflow_cnt[id] << 16) | cnt;
            s_mt_last_dir[id] = (int32_t)s_encoder_sign[id] * (int32_t)dir_sign;
        }
        break;

    case HAL_TIMER_IRQ_LOAD:
        s_mt_overflow_cnt[id]++;
        s_cap_load_count[id]++;
        s_cap_load_timer[id] = (uint16_t)hal_timer_get_count(
            s_encoder_cfg[id].timer);
        break;

    default:
        break;
    }
}

void PRJ_ENCODER_LF_IRQ_HANDLER(void)
{
    bsp_encoder_irq_handler(BSP_ENCODER_LF);
}

void PRJ_ENCODER_LB_IRQ_HANDLER(void)
{
    bsp_encoder_irq_handler(BSP_ENCODER_LB);
}

void PRJ_ENCODER_RF_IRQ_HANDLER(void)
{
    bsp_encoder_irq_handler(BSP_ENCODER_RF);
}

void PRJ_ENCODER_RB_IRQ_HANDLER(void)
{
    bsp_encoder_irq_handler(BSP_ENCODER_RB);
}
