/**
 * @file    bsp_encoder.c
 * @brief   缂栫爜鍣ㄩ┍鍔ㄥ疄鐜? * @note    鍩轰簬TIMG7/TIMA1/TIMG6/TIMG0鍥涗釜瀹氭椂鍣ㄧ殑缁勫悎鎹曡幏妯″紡瀹炵幇鍥涜矾缂栫爜鍣? *          A鐩告帴CAPTURE(CC0/CC1), B鐩告帴GPIO杈撳叆
 *
 *          ISR璋冪敤閾?
 *          TIMG7_IRQHandler()  鈫?bsp_encoder_irq_handler(BSP_ENCODER_LF)
 *          TIMA1_IRQHandler()  鈫?bsp_encoder_irq_handler(BSP_ENCODER_LB)
 *          TIMG6_IRQHandler() 鈫?bsp_encoder_irq_handler(BSP_ENCODER_RF)
 *          TIMG0_IRQHandler() 鈫?bsp_encoder_irq_handler(BSP_ENCODER_RB)
 *
 *          缁勫悎鎹曡幏妯″紡(DL_TIMER_CAPTURE_COMBINED_MODE_PULSE_WIDTH_AND_PERIOD_UP):
 *          - CC0: 鑴夊妯″紡, 涓嬮檷娌胯Е鍙? *          - CC1: 鍛ㄦ湡妯″紡, 涓婂崌娌胯Е鍙? *
 *          M娉曟祴閫?
 *          鍦ㄥ浐瀹氭椂闂寸獥鍙ｅ唴璁℃暟缂栫爜鍣ㄨ剦鍐? 鐢卞簲鐢ㄥ眰璋冪敤
 *          bsp_encoder_get_and_clear_all()鑾峰彇鑴夊啿澧為噺,
 *          鍐嶈皟鐢╞sp_encoder_counts_to_rpm()鎹㈢畻RPM.
 */
#include "bsp_encoder.h"
#include "hal_timer.h"
#include "hal_gpio.h"
#include "osal_api.h"
#include "ti_msp_dl_config.h"
#include "project_config.h"
/* ======================== 缂栫爜鍣ㄥ弬鏁?======================== */

/**
 * @brief  缂栫爜鍣ㄥ弬鏁?鏉ヨ嚜纭欢瑙勬牸)
 * @note   姣忚浆鎬昏剦鍐叉暟 = PPR * 鍑忛€熸瘮 * 鍊嶉鏁?2)
 *         缁勫悎鎹曡幏妯″紡涓婥C0+CC1鍧囦负A鐩歌竟娌胯Е鍙?鈫?浜屽€嶉
 */

/* 缂栫爜鍣ㄥ弬鏁伴泦涓湪project_config.h */

/* ======================== 绉佹湁鍙橀噺 ======================== */

/** 缂栫爜鍣ㄩ厤缃〃(鐢卞簲鐢ㄥ眰浼犲叆) */
static const bsp_encoder_config_t *s_encoder_cfg = NULL;

/** 缂栫爜鍣ㄩ厤缃暟閲?*/
static uint32_t s_encoder_cfg_count = 0U;

/** 姣忚浆鎬昏剦鍐叉暟 */
static uint32_t s_encoder_pulses_per_rev = 0U;

/** 鍚勭紪鐮佸櫒绱鑴夊啿璁℃暟(ISR鍐欏叆, 浠诲姟璇诲彇) */
static volatile int32_t s_encoder_count[BSP_ENCODER_COUNT] = {0};

/** 鍚勭紪鐮佸櫒鏂瑰悜鏍囧織(+1=姝ｈ浆, -1=鍙嶈浆) */
static volatile int8_t s_encoder_sign[BSP_ENCODER_COUNT] = {1, 1, 1, 1};

/** 鍚勭紪鐮佸櫒绱鎬昏剦鍐?ISR鍐欏叆, 涓嶈娓呴櫎, 鐢ㄤ簬璋冭瘯) */
static volatile int32_t s_encoder_total[BSP_ENCODER_COUNT] = {0};

/* ======================== M/T娉曟祴閫熷彉閲?======================== */
/** 棣栬竟娌縯imer璁℃暟鍊?*/
static volatile uint16_t s_mt_first_cnt[BSP_ENCODER_COUNT];
/** 鏈竟娌縯imer璁℃暟鍊?*/
static volatile uint16_t s_mt_last_cnt[BSP_ENCODER_COUNT];
/** 鏈€鍚庤竟娌挎柟鍚?+1/-1) */
static volatile int32_t  s_mt_last_dir[BSP_ENCODER_COUNT];
/** 绐楀彛鍐呮槸鍚︽湁杈规部 */
static volatile bool     s_mt_has_edge[BSP_ENCODER_COUNT];
/** CC1纭欢鎹曡幏鍛ㄦ湡鍊?*/
static volatile uint16_t s_mt_last_period[BSP_ENCODER_COUNT];
/** timer婧㈠嚭璁℃暟 */
static volatile uint32_t s_mt_overflow_cnt[BSP_ENCODER_COUNT];
/** 鏈€鍚庤竟娌?2浣嶇粷瀵规椂闂存埑 */
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

/** 鍒濆鍖栨爣蹇?*/
static bool s_encoder_inited = false;

/* ======================== 鍏叡鍑芥暟瀹炵幇 ======================== */

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

    /* 娓呴浂鎵€鏈夎鏁板強M/T鍙橀噺 */
    for (uint32_t i = 0; i < s_encoder_cfg_count; i++) {
        s_encoder_count[i]    = 0;
        s_encoder_total[i]    = 0;
        s_encoder_sign[i]     = 1;
        s_mt_first_cnt[i]     = 0;
        s_mt_last_cnt[i]      = 0;
        s_mt_last_dir[i]      = 0;
        s_mt_has_edge[i]      = false;
        s_mt_last_period[i]   = 0;
        s_mt_overflow_cnt[i]  = 0;
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

    /* 浣胯兘4璺紪鐮佸櫒瀹氭椂鍣ㄧ殑NVIC涓柇骞跺惎鍔ㄨ鏁?*/
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
     * M娉昍PM璁＄畻:
     *   RPM = (delta / PPR) * (60000ms / dt_ms)
     *   delta        : 鏈懆鏈熻剦鍐插閲?     *   PPR          : 姣忚浆鎬昏剦鍐叉暟(PRJ_ENCODER_PULSES_PER_REV, PPR*鍑忛€熸瘮*鍊嶉)
     *   dt_ms        : 閲囨牱鍛ㄦ湡(ms)
     *   60000        : 1鍒嗛挓=60000ms
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

    /* 鍚堝苟涓哄崟娆￠櫎娉? 鍑忓皯绮惧害鎹熷け */
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

/* ======================== 璇婃柇鎺ュ彛 ======================== */

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
    /* 娉ㄦ剰: count 鍜?has_edge 涓嶆竻闆?鈥?鍙璇婃柇 */

    return BSP_OK;
}

/* ======================== M/T娉曟祴閫?======================== */

bsp_status_t bsp_encoder_get_all_rpm_mt(int32_t rpms[], bool had_edge[])
{
    if (rpms == NULL) {
        return BSP_ERR_NULL_PTR;
    }

    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0; i < s_encoder_cfg_count; i++) {
            int32_t M = s_encoder_count[i];
            bool has_edge = s_mt_has_edge[i];
            uint16_t first_cnt = s_mt_first_cnt[i];
            uint16_t last_cnt  = s_mt_last_cnt[i];
            uint16_t period    = s_mt_last_period[i];
            int32_t  last_dir  = s_mt_last_dir[i];
            uint32_t last_abs  = s_mt_last_abs[i];
            uint32_t overflow  = s_mt_overflow_cnt[i];

            s_encoder_count[i] = 0;
            s_mt_has_edge[i]   = false;

            if (had_edge != NULL) {
                had_edge[i] = has_edge;
            }

            int32_t rpm = 0;

            if (M >= 2 || M <= -2) {
                /* M/T娉? T_ref = 鏈竟娌?- 棣栬竟娌?*/
                int16_t diff = (int16_t)(last_cnt - first_cnt);
                uint32_t t_ref = (diff > 0)
                    ? (uint32_t)diff
                    : (uint32_t)(diff + PRJ_UINT16_MOD);
                if (t_ref == 0) t_ref = 1;
                int64_t num = (int64_t)M * PRJ_ENCODER_RPM_CALC_CONST;
                int64_t den = (int64_t)t_ref
                    * (int64_t)s_encoder_pulses_per_rev;
                rpm = (int32_t)(num / den);

            } else if (M == 1 || M == -1) {
                /* T娉? 鐢–C1纭欢鍛ㄦ湡 */
                if (period == 0) {
                    /* CC0棣栨部, period灏氭湭鏇存柊, 鏀圭敤time_since浼扮畻 */
                    uint16_t now_cnt;
                    uint32_t now_abs;
                    OSAL_CRITICAL_SECTION {
                        now_cnt = (uint16_t)hal_timer_get_count(
                            s_encoder_cfg[i].timer);
                        now_abs = (s_mt_overflow_cnt[i] << 16) | now_cnt;
                    }
                    uint32_t time_since = now_abs - last_abs;
                    if (time_since == 0) time_since = 1;
                    int64_t num = (int64_t)M * PRJ_ENCODER_RPM_CALC_CONST;
                    int64_t den = (int64_t)time_since
                        * (int64_t)s_encoder_pulses_per_rev;
                    rpm = (int32_t)(num / den);
                } else {
                    int64_t num = (int64_t)M * PRJ_ENCODER_RPM_CALC_CONST;
                    int64_t den = (int64_t)period
                        * (int64_t)s_encoder_pulses_per_rev;
                    rpm = (int32_t)(num / den);
                }

            } else {
                /* M=0: 鏃犲綋鍓嶇獥鍙ｅ唴杈规部 */
                if (last_dir == 0) {
                    /* 浠庢湭鎹曡幏鍒拌竟娌?鍒濆鍖栧悗棣栨杩愯) */
                    rpm = 0;
                } else {
                    /* 骞虫粦琛板噺: MAX(time_since, last_period) */
                    uint16_t now_cnt;
                    uint32_t now_abs;
                    OSAL_CRITICAL_SECTION {
                        now_cnt = (uint16_t)hal_timer_get_count(
                            s_encoder_cfg[i].timer);
                        now_abs = (s_mt_overflow_cnt[i] << 16) | now_cnt;
                    }
                    uint32_t time_since = now_abs - last_abs;

                    if (period == 0) period = 1;
                    uint32_t effective =
                        (time_since > period) ? time_since : period;

                    int64_t num = (int64_t)last_dir * PRJ_ENCODER_RPM_CALC_CONST;
                    int64_t den = (int64_t)effective
                        * (int64_t)s_encoder_pulses_per_rev;
                    rpm = (int32_t)(num / den);
                }
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

/* ======================== ISR 瀹炵幇 ======================== */

void bsp_encoder_irq_handler(bsp_encoder_id_t id)
{
    hal_timer_irq_flag_t flag = hal_timer_get_irq_flag(s_encoder_cfg[id].timer);
    int8_t dir_sign = s_encoder_cfg[id].dir_sign;
    if (dir_sign == 0) dir_sign = 1;

    /* 璇诲彇B鐩哥數骞冲垽鍒柟鍚?*/
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
         * CC0涓柇(涓嬮檷娌胯Е鍙?
         * 鏂瑰悜鍒ゅ埆: 涓嬮檷娌挎椂B=楂樷啋姝ｈ浆, B=浣庘啋鍙嶈浆
         */
        s_encoder_sign[id] = b_high ? 1 : -1;
        {
            int32_t inc = (int32_t)s_encoder_sign[id] * (int32_t)dir_sign;
            s_encoder_count[id] += inc;
            s_encoder_total[id] += inc;
        }
        {
            /* M/T: 璁板綍杈规部鏃堕棿鎴?*/
            uint16_t cnt = (uint16_t)hal_timer_get_count(
                s_encoder_cfg[id].timer);
            if (!s_mt_has_edge[id]) {
                s_mt_first_cnt[id] = cnt;
                s_mt_has_edge[id] = true;
            }
            s_mt_last_cnt[id] = cnt;
            s_mt_last_abs[id] =
                (s_mt_overflow_cnt[id] << 16) | cnt;
            s_mt_last_dir[id] = s_encoder_sign[id];
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
            }
            s_cap_cc1_value[id] = capture;
            s_cap_cc1_last_abs[id] = current_abs;
            s_cap_cc1_seen[id] = true;
        }
        /*
         * CC1涓柇(涓婂崌娌胯Е鍙?
         * 鏂瑰悜鍒ゅ埆: 涓婂崌娌挎椂B=楂樷啋鍙嶈浆, B=浣庘啋姝ｈ浆
         */
        s_encoder_sign[id] = b_high ? -1 : 1;
        {
            int32_t inc = (int32_t)s_encoder_sign[id] * (int32_t)dir_sign;
            s_encoder_count[id] += inc;
            s_encoder_total[id] += inc;
        }
        {
            /* M/T: 璁板綍杈规部鏃堕棿鎴?*/
            uint16_t cnt = (uint16_t)hal_timer_get_count(
                s_encoder_cfg[id].timer);
            if (!s_mt_has_edge[id]) {
                s_mt_first_cnt[id] = cnt;
                s_mt_has_edge[id] = true;
            }
            s_mt_last_cnt[id] = cnt;
            s_mt_last_abs[id] =
                (s_mt_overflow_cnt[id] << 16) | cnt;
            s_mt_last_dir[id] = s_encoder_sign[id];
        }
        {
            /* CC1纭欢鍛ㄦ湡(涓婂崌娌垮埌涓婂崌娌? */
            s_mt_last_period[id] =
                (uint16_t)hal_timer_get_capture_value(
                    s_encoder_cfg[id].timer, 1);
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
