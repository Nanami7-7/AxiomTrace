/**
 * @file    app_test_runner.c
 * @brief   EKF ZUPT 测试运行器 (集成精准计时器)
 *
 * 测试流程:
 *   1. 启动计时器 (默认 60 秒)
 *   2. 在 IMU 任务中采集数据并运行 EKF
 *   3. 超时后自动停止, 输出统计信息
 *   4. 清理资源
 *
 * 使用方法:
 *   在串口发送 "zutptest 60" 启动 60 秒 ZUPT 测试
 *   测试结束后自动输出统计摘要
 */

#include "app_test_runner.h"
#include "app_test_timer.h"
#include "bsp_timer.h"
#include "task_imu.h"
#include "filter.h"
#include "filter_config.h"   /* KF_Q_ANGLE_DEFAULT 等参数宏 */
#include <stdio.h>
#include <string.h>
#include <math.h>   /* fabsf, sqrtf */

/* 宏字符串化 (用于 printf 输出宏的当前值) */
#define TOSTR_HELPER(x)    #x
#define TOSTR(x)           TOSTR_HELPER(x)

/* ============================================================
 * 测试模式与转动状态机
 * ============================================================ */
typedef enum {
    TEST_MODE_ZUPT = 0,   /* 静态 ZUPT 漂移测试 */
    TEST_MODE_TURN,      /* 动态转动精度测试 */
    TEST_MODE_KFTUNE,    /* KF 参数扫描测试 */
} test_mode_t;

typedef enum {
    TURN_STATE_IDLE = 0,
    TURN_STATE_STATIC_REF,   /* 静止参考阶段 */
    TURN_STATE_TURNING,      /* 转动中 (检测开始/结束) */
    TURN_STATE_STABLE,       /* 转动后稳定 */
    TURN_STATE_DONE,         /* 完成 */
} turn_state_t;

/* 转动测试参数 */
#define TURN_STATIC_REF_MS     2000     /* 静止参考时长 */
#define TURN_STABLE_MS         2000     /* 转动后稳定时长 */
#define TURN_START_GYRO_DPS    10.0f    /* 转动开始检测阈值 */
#define TURN_END_GYRO_DPS      2.0f     /* 转动结束检测阈值 */
#define TURN_END_FRAMES        50       /* 转动结束确认帧数 (0.5s @100Hz) */
#define TURN_PASS_THRESHOLD    2.0f     /* PASS 判定阈值 ±2° */

/* ---- 测试统计 ---- */
static struct {
    uint32_t frame_count;
    float    yaw_start;
    float    yaw_end;
    float    yaw_max;
    float    yaw_min;
    float    pitch_start;
    float    pitch_end;
    float    roll_start;
    float    roll_end;
    bool     active;
    bool     yaw_initialized;
    float    yaw_last_raw;
    float    yaw_offset;
    float    yaw_mean;
    float    yaw_M2;
    float    yaw_max_jump;
    uint32_t jump_frame;
    float    jump_prev_yaw;
    float    jump_curr_yaw;
    test_diag_ctx_t jump_diag;

    /* ---- 3-axis bias tracking (KF) ---- */
    float    bias_x_start, bias_y_start, bias_z_start;
    float    bias_x_end,   bias_y_end,   bias_z_end;
    float    bias_x_max,   bias_y_max,   bias_z_max;
    float    bias_x_min,   bias_y_min,   bias_z_min;
    float    bias_x_last,  bias_y_last,  bias_z_last;
    float    bias_max_delta;
    uint32_t bias_sample_count;
    bool     bias_initialized;

    /* ---- 故障检测位 ---- */
    uint32_t fault_nan_inf;
    uint32_t fault_diverge;
    uint32_t fault_acc_anomaly;

    /* ---- TURN 模式专用 ---- */
    test_mode_t  mode;
    turn_state_t turn_state;
    float        target_angle;
    uint32_t     state_start_ms;
    uint32_t     turn_end_counter;
    bool         turn_detected;
    float        yaw_ref;
    float        yaw_ref_sum;
    uint32_t     yaw_ref_count;
    float        yaw_turn_start;
    float        yaw_turn_end;
    float        yaw_stable_end;
    uint32_t     turn_frame_count;
    uint32_t     turn_start_ms;       /* [B12] 转动开始实际时间戳 */
    uint32_t     turn_end_ms;         /* [B12] 转动结束实际时间戳 */
    uint32_t     stable_start_ms;     /* [B12] 稳定阶段开始时间戳 */
    float        turn_yaw_max_jump;
    float        turn_yaw_last;
    /* TURN 分段 3-axis bias tracking */
    float        turn_bias_x_start, turn_bias_y_start, turn_bias_z_start;
    float        turn_bias_x_end,   turn_bias_y_end,   turn_bias_z_end;
    float        stable_bias_x_start, stable_bias_y_start, stable_bias_z_start;
    float        stable_bias_x_end,   stable_bias_y_end,   stable_bias_z_end;
    float        stable_yaw_start;
    float        stable_yaw_end;
    float        stable_yaw_max;
    float        stable_yaw_min;
    uint32_t     stable_frame_count;

    /* ---- KFTUNE 模式专用 ---- */
    uint32_t     kftune_set_duration_ms;  /* 每组测试时长 (ms) */
    uint32_t     kftune_set_index;       /* 当前测试组索引 */
    uint32_t     kftune_set_start_ms;    /* 当前组开始时间 */
    float        kftune_set_yaw_start;   /* 当前组起始 yaw */
    float        kftune_set_yaw_end;     /* 当前组结束 yaw */
    float        kftune_set_yaw_max;     /* 当前组 yaw 最大值 */
    float        kftune_set_yaw_min;     /* 当前组 yaw 最小值 */
    float        kftune_set_yaw_mean;    /* Welford 均值 */
    float        kftune_set_yaw_M2;      /* Welford M2 */
    float        kftune_set_max_jump;    /* 当前组最大跳变 */
    bool         kftune_set_yaw_init;    /* 当前组 yaw 已初始化 */
    bool         kftune_pending_start;    /* 待启动首组 (并发安全: 在IMU任务中执行) */
    uint32_t     kftune_motion_frames;   /* [B8] 当前组检测到运动的帧数 */
} s_stats;

/* ---- 超时回调 ---- */
static void on_test_timeout(void)
{
    /* 在调用 expired() 的上下文中执行 (通常是 task_imu) */
    printf("\r\n[TEST] Timer expired, stopping...\r\n");
}

/* ============================================================
 * 故障检测与 bias_z 跟踪 (通用, 两种模式共享)
 * ============================================================ */

/* NaN/Inf 检测: pitch/roll/yaw 任一无效则记故障 */
static void fault_check_attitude(float pitch, float roll, float yaw)
{
    if (isnan(pitch) || isinf(pitch) ||
        isnan(roll)  || isinf(roll)  ||
        isnan(yaw)   || isinf(yaw)) {
        s_stats.fault_nan_inf++;
    }
}

/* yaw 发散检测: unwrap 后绝对值超过 720° (2圈) 视为发散 */
static void fault_check_diverge(float yaw_unwrapped)
{
    if (fabsf(yaw_unwrapped) > 720.0f) {
        s_stats.fault_diverge++;
    }
}

/* acc 异常检测: acc_norm 超出 [0.5, 2.0]g */
static void fault_check_acc(const test_diag_ctx_t *diag)
{
    if (diag != NULL && diag->acc_norm > 0.0f) {
        if (diag->acc_norm < 0.5f || diag->acc_norm > 2.0f) {
            s_stats.fault_acc_anomaly++;
        }
    }
}

/* 3-axis bias tracking */
static void track_kf_bias(const test_diag_ctx_t *diag)
{
    if (diag == NULL) return;

    float bx = diag->kf_bias_x;
    float by = diag->kf_bias_y;
    float bz = diag->kf_bias_z;
    s_stats.bias_sample_count++;

    if (!s_stats.bias_initialized) {
        s_stats.bias_x_start = bx; s_stats.bias_y_start = by; s_stats.bias_z_start = bz;
        s_stats.bias_x_end   = bx; s_stats.bias_y_end   = by; s_stats.bias_z_end   = bz;
        s_stats.bias_x_max   = bx; s_stats.bias_y_max   = by; s_stats.bias_z_max   = bz;
        s_stats.bias_x_min   = bx; s_stats.bias_y_min   = by; s_stats.bias_z_min   = bz;
        s_stats.bias_x_last  = bx; s_stats.bias_y_last  = by; s_stats.bias_z_last  = bz;
        s_stats.bias_max_delta = 0.0f;
        s_stats.bias_initialized = true;
    } else {
        float dx = fabsf(bx - s_stats.bias_x_last);
        float dy = fabsf(by - s_stats.bias_y_last);
        float dz = fabsf(bz - s_stats.bias_z_last);
        float max_d = dx;
        if (dy > max_d) max_d = dy;
        if (dz > max_d) max_d = dz;
        if (max_d > s_stats.bias_max_delta) s_stats.bias_max_delta = max_d;
        if (bx > s_stats.bias_x_max) s_stats.bias_x_max = bx;
        if (bx < s_stats.bias_x_min) s_stats.bias_x_min = bx;
        if (by > s_stats.bias_y_max) s_stats.bias_y_max = by;
        if (by < s_stats.bias_y_min) s_stats.bias_y_min = by;
        if (bz > s_stats.bias_z_max) s_stats.bias_z_max = bz;
        if (bz < s_stats.bias_z_min) s_stats.bias_z_min = bz;
    }
    s_stats.bias_x_last = bx; s_stats.bias_y_last = by; s_stats.bias_z_last = bz;
    s_stats.bias_x_end  = bx; s_stats.bias_y_end  = by; s_stats.bias_z_end  = bz;
}

/* ---- 前向声明 ---- */
static void turn_feed(float pitch, float roll, float yaw,
                      const test_diag_ctx_t *diag);
static void turn_stop(void);
static void kftune_feed(float pitch, float roll, float yaw,
                        const test_diag_ctx_t *diag);
static void kftune_poll(void);
static void kftune_stop(void);

/* ---- 接口实现 ---- */

bool app_test_runner_is_active(void)
{
    return s_stats.active;
}

void app_test_runner_feed(float pitch, float roll, float yaw)
{
    app_test_runner_feed_diag(pitch, roll, yaw, NULL);
}

void app_test_runner_feed_diag(float pitch, float roll, float yaw,
                                const test_diag_ctx_t *diag)
{
    if (!s_stats.active) return;

    s_stats.frame_count++;

    /* ---- 通用故障检测 (两种模式共享) ---- */
    fault_check_attitude(pitch, roll, yaw);
    fault_check_acc(diag);
    track_kf_bias(diag);

    if (s_stats.mode == TEST_MODE_TURN) {
        turn_feed(pitch, roll, yaw, diag);
        return;
    }

    if (s_stats.mode == TEST_MODE_KFTUNE) {
        kftune_feed(pitch, roll, yaw, diag);
        return;
    }

    /* ---- ZUPT 模式 ---- */
    /* F2: unwrap 跨边界处理
     * 将 [-180,180] 连续角度展开为无界连续值
     * 检测 ±180° 跳变, 累积偏移 */
    float yaw_unwrapped = yaw;
    if (s_stats.yaw_initialized) {
        float dy = yaw - s_stats.yaw_last_raw;
        if (dy > 180.0f) {
            s_stats.yaw_offset -= 360.0f;
        } else if (dy < -180.0f) {
            s_stats.yaw_offset += 360.0f;
        }
        yaw_unwrapped = yaw + s_stats.yaw_offset;

        /* 发散检测: unwrap 后绝对值超过 720° 视为发散 */
        fault_check_diverge(yaw_unwrapped);

        /* F4: 最大帧间跳变 (用 unwrap 后的值, 避免跨边界误判)
         * [跳变诊断] 记录 max_jump 发生时的完整上下文 */
        float jump = fabsf(yaw_unwrapped - s_stats.yaw_end);
        if (jump > s_stats.yaw_max_jump) {
            s_stats.yaw_max_jump = jump;
            s_stats.jump_frame   = s_stats.frame_count;
            s_stats.jump_prev_yaw = s_stats.yaw_end;
            s_stats.jump_curr_yaw = yaw_unwrapped;
            if (diag != NULL) {
                s_stats.jump_diag = *diag;
            } else {
                memset(&s_stats.jump_diag, 0, sizeof(s_stats.jump_diag));
            }
        }

        /* F4: Welford 在线方差算法
         * mean_n = mean_{n-1} + (x_n - mean_{n-1}) / n
         * M2_n = M2_{n-1} + (x_n - mean_{n-1}) * (x_n - mean_n) */
        float delta = yaw_unwrapped - s_stats.yaw_mean;
        s_stats.yaw_mean += delta / (float)s_stats.frame_count;
        float delta2 = yaw_unwrapped - s_stats.yaw_mean;
        s_stats.yaw_M2 += delta * delta2;
    }
    s_stats.yaw_last_raw = yaw;

    /* 记录首帧值 */
    if (!s_stats.yaw_initialized) {
        s_stats.yaw_start = yaw_unwrapped;
        s_stats.yaw_end   = yaw_unwrapped;
        s_stats.yaw_max   = yaw_unwrapped;
        s_stats.yaw_min   = yaw_unwrapped;
        s_stats.yaw_mean  = yaw_unwrapped;
        s_stats.yaw_M2    = 0.0f;
        s_stats.pitch_start = pitch;
        s_stats.roll_start  = roll;
        s_stats.yaw_initialized = true;
    } else {
        s_stats.yaw_end = yaw_unwrapped;
        if (yaw_unwrapped > s_stats.yaw_max) s_stats.yaw_max = yaw_unwrapped;
        if (yaw_unwrapped < s_stats.yaw_min) s_stats.yaw_min = yaw_unwrapped;
    }
    s_stats.pitch_end = pitch;
    s_stats.roll_end  = roll;

    /* 每 100 帧打印进度 */
    if ((s_stats.frame_count % 100) == 0) {
        uint8_t pct = app_test_timer_progress();
        uint32_t remain = app_test_timer_remaining_ms() / 1000;
        printf("[TEST] %3lu%% remain=%lus yaw=%.2f\r\n",
               (unsigned long)pct, (unsigned long)remain, (double)yaw);
    }
}

int app_test_runner_start(uint32_t duration_sec)
{
    if (s_stats.active) {
        printf("[TEST] Test already running\r\n");
        return -1;
    }

    /* [B6修复] 输入参数边界检查 */
    if (duration_sec == 0 || duration_sec > 3600u) {
        printf("[TEST] Invalid duration=%lu (must be 1..3600)\r\n",
               (unsigned long)duration_sec);
        return -1;
    }

    /* 重置统计 */
    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.active = true;
    s_stats.mode = TEST_MODE_ZUPT;  /* [B2修复] 显式设置模式, 不依赖枚举值0 */

    /* 注册超时回调 */
    app_test_timer_set_callback(on_test_timeout);

    /* 启动计时器 */
    if (app_test_timer_start(duration_sec) != 0) {
        s_stats.active = false;
        printf("[TEST] Failed to start timer\r\n");
        return -1;
    }

    printf("\r\n");
    printf("========================================\r\n");
    printf("[TEST] ZUPT Test Started (%lu sec)\r\n", (unsigned long)duration_sec);
    printf("[TEST] Keep device still...\r\n");
    printf("========================================\r\n");

    return 0;
}

void app_test_runner_stop(void)
{
    if (!s_stats.active) return;

    if (s_stats.mode == TEST_MODE_TURN) {
        turn_stop();
        return;
    }

    if (s_stats.mode == TEST_MODE_KFTUNE) {
        kftune_stop();
        return;
    }

    /* ---- ZUPT 模式报告 ---- */
    /* 计算统计 */
    float yaw_drift   = s_stats.yaw_end - s_stats.yaw_start;
    float pitch_drift = s_stats.pitch_end - s_stats.pitch_start;
    float roll_drift  = s_stats.roll_end - s_stats.roll_start;
    float yaw_range   = s_stats.yaw_max - s_stats.yaw_min;

    uint32_t elapsed_ms = app_test_timer_elapsed_ms();
    float elapsed_sec = (float)elapsed_ms / 1000.0f;
    float drift_rate  = (elapsed_sec > 0.1f)
                      ? fabsf(yaw_drift) / elapsed_sec : 0.0f;

    /* 标准差 (Welford 在线算法最终值) */
    float yaw_std = 0.0f;
    if (s_stats.frame_count > 1) {
        float variance = s_stats.yaw_M2 / (float)(s_stats.frame_count - 1);
        yaw_std = sqrtf(variance);
    }

    printf("\r\n");
    printf("========================================\r\n");
    printf("[TEST] ZUPT Test Finished\r\n");
    printf("========================================\r\n");
    printf("  Duration     : %.1f sec\r\n", (double)elapsed_sec);
    printf("  Frames       : %lu\r\n", (unsigned long)s_stats.frame_count);
    printf("  Yaw start    : %.3f deg\r\n", (double)s_stats.yaw_start);
    printf("  Yaw end      : %.3f deg\r\n", (double)s_stats.yaw_end);
    printf("  Yaw drift    : %.3f deg\r\n", (double)yaw_drift);
    printf("  Yaw range    : %.3f deg (max-min)\r\n", (double)yaw_range);
    printf("  Yaw std      : %.4f deg\r\n", (double)yaw_std);
    printf("  Yaw max_jump : %.4f deg\r\n", (double)s_stats.yaw_max_jump);
    printf("  Drift rate   : %.4f deg/s (abs)\r\n", (double)drift_rate);
    printf("  Pitch drift  : %.3f deg\r\n", (double)pitch_drift);
    printf("  Roll drift   : %.3f deg\r\n", (double)roll_drift);
    printf("----------------------------------------\r\n");
    printf("[JUMP DIAG] max_jump context:\r\n");
    printf("  Frame#       : %lu / %lu (%.1f%%)\r\n",
           (unsigned long)s_stats.jump_frame,
           (unsigned long)s_stats.frame_count,
           s_stats.frame_count > 0
             ? 100.0f * (float)s_stats.jump_frame / (float)s_stats.frame_count
             : 0.0f);
    printf("  Prev yaw     : %.4f deg\r\n", (double)s_stats.jump_prev_yaw);
    printf("  Curr yaw     : %.4f deg\r\n", (double)s_stats.jump_curr_yaw);
    printf("  gyro_mag     : %.4f dps\r\n", (double)s_stats.jump_diag.gyro_mag_dps);
    printf("  acc_norm     : %.4f g (err=%.4f)\r\n",
           (double)s_stats.jump_diag.acc_norm,
           (double)s_stats.jump_diag.acc_norm_err);
    printf("  kf_bias      : %.4f, %.4f, %.4f dps (x,y,z)\r\n",
           (double)s_stats.jump_diag.kf_bias_x,
           (double)s_stats.jump_diag.kf_bias_y,
           (double)s_stats.jump_diag.kf_bias_z);
    printf("  kf_p00       : %.6f, %.6f, %.6f\r\n",
           (double)s_stats.jump_diag.kf_p00_x,
           (double)s_stats.jump_diag.kf_p00_y,
           (double)s_stats.jump_diag.kf_p00_z);
    printf("  kf_p11       : %.6f, %.6f, %.6f\r\n",
           (double)s_stats.jump_diag.kf_p11_x,
           (double)s_stats.jump_diag.kf_p11_y,
           (double)s_stats.jump_diag.kf_p11_z);

    /* ---- 3-axis bias tracking ---- */
    if (s_stats.bias_initialized) {
        printf("[BIAS TRACK] samples=%lu\r\n",
               (unsigned long)s_stats.bias_sample_count);
        printf("  bias_x start: %.4f end: %.4f diff: %.4f\r\n",
               (double)s_stats.bias_x_start, (double)s_stats.bias_x_end,
               (double)(s_stats.bias_x_end - s_stats.bias_x_start));
        printf("  bias_y start: %.4f end: %.4f diff: %.4f\r\n",
               (double)s_stats.bias_y_start, (double)s_stats.bias_y_end,
               (double)(s_stats.bias_y_end - s_stats.bias_y_start));
        printf("  bias_z start: %.4f end: %.4f diff: %.4f\r\n",
               (double)s_stats.bias_z_start, (double)s_stats.bias_z_end,
               (double)(s_stats.bias_z_end - s_stats.bias_z_start));
        printf("  max_delta    : %.4f dps (single frame)\r\n",
               (double)s_stats.bias_max_delta);
        printf("  x_range=[%.4f,%.4f] y_range=[%.4f,%.4f] z_range=[%.4f,%.4f]\r\n",
               (double)s_stats.bias_x_min, (double)s_stats.bias_x_max,
               (double)s_stats.bias_y_min, (double)s_stats.bias_y_max,
               (double)s_stats.bias_z_min, (double)s_stats.bias_z_max);
    } else {
        printf("[BIAS TRACK] (no diag data)\r\n");
    }

    /* ---- 故障检测摘要 ---- */
    printf("[FAULT DETECT]\r\n");
    printf("  nan_inf     : %lu\r\n", (unsigned long)s_stats.fault_nan_inf);
    printf("  diverge     : %lu (yaw > 720 deg)\r\n",
           (unsigned long)s_stats.fault_diverge);
    printf("  acc_anomaly : %lu (acc outside [0.5,2.0]g)\r\n",
           (unsigned long)s_stats.fault_acc_anomaly);

    /* ---- 综合判定 ---- */
    /* PASS 条件: 无故障 + drift_rate < 0.1°/s + max_jump < 1°
     * [B7修复] acc_anomaly 纳入判定 (超过总帧数 5% 视为异常) */
    const char *verdict = "PASS";
    uint32_t acc_anomaly_limit = (s_stats.frame_count + 19u) / 20u;  /* ~5% */
    if (s_stats.fault_nan_inf > 0 || s_stats.fault_diverge > 0) {
        verdict = "FAIL (critical fault)";
    } else if (s_stats.fault_acc_anomaly > acc_anomaly_limit) {
        verdict = "FAIL (acc anomaly > 5%)";
    } else if (drift_rate > 0.1f) {
        verdict = "FAIL (drift > 0.1 deg/s)";
    } else if (s_stats.yaw_max_jump > 1.0f) {
        verdict = "WARN (max_jump > 1 deg)";
    }
    printf("[VERDICT] %s\r\n", verdict);
    printf("========================================\r\n\r\n");

    /* 清理 */
    app_test_timer_cleanup();
    s_stats.active = false;
}

void app_test_runner_poll(void)
{
    if (!s_stats.active) return;

    if (s_stats.mode == TEST_MODE_KFTUNE) {
        kftune_poll();
        return;
    }

    if (app_test_timer_expired()) {
        app_test_runner_stop();
    }
}

/* ============================================================
 * 动态转动精度测试 (TURN 模式)
 * ============================================================ */

/**
 * @brief 转动测试喂入函数 (状态机)
 *
 * 状态: STATIC_REF → TURNING → STABLE → DONE
 * 分段统计:
 *   - TURNING: bias_z 起止 + maneuver_level 分布 (验证 ZUPT 门控)
 *   - STABLE: yaw 漂移 + bias_z 漂移 (验证"停下后yaw变动"修复)
 */
static void turn_feed(float pitch, float roll, float yaw,
                      const test_diag_ctx_t *diag)
{
    /* unwrap 处理 (复用 s_stats.yaw_last_raw / yaw_offset) */
    float yaw_unwrapped = yaw;
    if (s_stats.yaw_initialized) {
        float dy = yaw - s_stats.yaw_last_raw;
        if (dy > 180.0f) {
            s_stats.yaw_offset -= 360.0f;
        } else if (dy < -180.0f) {
            s_stats.yaw_offset += 360.0f;
        }
        yaw_unwrapped = yaw + s_stats.yaw_offset;

        /* 发散检测 */
        fault_check_diverge(yaw_unwrapped);
    }
    s_stats.yaw_last_raw = yaw;

    if (!s_stats.yaw_initialized) {
        s_stats.yaw_initialized = true;
        /* 首帧: yaw_unwrapped = yaw, yaw_ref 累加在 STATIC_REF case 中处理 */
    }

    uint32_t now_ms = app_test_timer_elapsed_ms();
    float gyro_mag = (diag != NULL) ? diag->gyro_mag_dps : 0.0f;

    switch (s_stats.turn_state) {
    case TURN_STATE_STATIC_REF:
        /* 累加 yaw 用于计算参考均值 */
        if (s_stats.yaw_ref_count == 0) {
            s_stats.yaw_ref_sum = yaw_unwrapped;
            s_stats.yaw_ref_count = 1;
        } else {
            s_stats.yaw_ref_sum += yaw_unwrapped;
            s_stats.yaw_ref_count++;
        }

        if ((now_ms - s_stats.state_start_ms) >= TURN_STATIC_REF_MS) {
            /* [B9修复] 除零保护 (正常情况 yaw_ref_count >= 1) */
            if (s_stats.yaw_ref_count > 0) {
                s_stats.yaw_ref = s_stats.yaw_ref_sum / (float)s_stats.yaw_ref_count;
            } else {
                s_stats.yaw_ref = yaw_unwrapped;  /* fallback */
            }
            s_stats.pitch_start = pitch;
            s_stats.roll_start  = roll;
            s_stats.turn_state = TURN_STATE_TURNING;
            s_stats.state_start_ms = now_ms;
            s_stats.turn_end_counter = 0;
            s_stats.turn_detected = false;
            printf("[TEST] STATIC_REF done, yaw_ref=%.3f\r\n"
                   "[TEST] Please turn device %.1f deg...\r\n",
                   (double)s_stats.yaw_ref, (double)s_stats.target_angle);
        }
        break;

    case TURN_STATE_TURNING:
        if (!s_stats.turn_detected) {
            /* 检测转动开始 */
            if (gyro_mag > TURN_START_GYRO_DPS) {
                s_stats.turn_detected = true;
                s_stats.yaw_turn_start = yaw_unwrapped;
                s_stats.turn_yaw_last = yaw_unwrapped;
                s_stats.turn_yaw_max_jump = 0.0f;
                s_stats.turn_frame_count = 1;
                s_stats.turn_start_ms = now_ms;  /* [B12] 记录实际时间 */
                /* 记录转动开始时 3-axis bias */
                s_stats.turn_bias_x_start = s_stats.bias_x_last;
                s_stats.turn_bias_y_start = s_stats.bias_y_last;
                s_stats.turn_bias_z_start = s_stats.bias_z_last;
                printf("[TEST] Turn START detected, gyro=%.1f dps, yaw=%.3f\r\n",
                       (double)gyro_mag, (double)yaw_unwrapped);
                printf("[TEST] Turn to target, then send 'turnend'\r\n");
            }
        } else {
            /* 转动中: 仅统计, 不自动结束 (等用户发送 turnend) */
            s_stats.turn_frame_count++;
            float jump = fabsf(yaw_unwrapped - s_stats.turn_yaw_last);
            if (jump > s_stats.turn_yaw_max_jump) {
                s_stats.turn_yaw_max_jump = jump;
            }
            s_stats.turn_yaw_last = yaw_unwrapped;
            s_stats.yaw_turn_end = yaw_unwrapped;  /* 持续更新, 供 turnend 使用 */
        }
        break;

    case TURN_STATE_STABLE:
        /* 稳定阶段: 统计 yaw 漂移 (验证"停下后yaw变动"修复) */
        if (s_stats.stable_frame_count == 0) {
            s_stats.stable_yaw_start = yaw_unwrapped;
            s_stats.stable_yaw_max   = yaw_unwrapped;
            s_stats.stable_yaw_min   = yaw_unwrapped;
            s_stats.stable_bias_x_start = s_stats.bias_x_last;
            s_stats.stable_bias_y_start = s_stats.bias_y_last;
            s_stats.stable_bias_z_start = s_stats.bias_z_last;
        } else {
            if (yaw_unwrapped > s_stats.stable_yaw_max) s_stats.stable_yaw_max = yaw_unwrapped;
            if (yaw_unwrapped < s_stats.stable_yaw_min) s_stats.stable_yaw_min = yaw_unwrapped;
        }
        s_stats.stable_frame_count++;
        s_stats.stable_yaw_end = yaw_unwrapped;
        s_stats.stable_bias_x_end = s_stats.bias_x_last;
        s_stats.stable_bias_y_end = s_stats.bias_y_last;
        s_stats.stable_bias_z_end = s_stats.bias_z_last;

        /* yaw_stable_end 每帧同步更新, 保证超时中断时也有有效值
         * (原实现仅在 STABLE->DONE 切换时赋值, 超时路径下为 0 导致角度计算错误) */
        s_stats.yaw_stable_end = yaw_unwrapped;

        if ((now_ms - s_stats.state_start_ms) >= TURN_STABLE_MS) {
            s_stats.turn_state = TURN_STATE_DONE;
            /* 自动输出报告并停止 */
            app_test_runner_stop();
            return;  /* stop 已输出报告, 直接返回 */
        }
        break;

    case TURN_STATE_DONE:
    case TURN_STATE_IDLE:
    default:
        break;
    }

    /* pitch/roll 记录 */
    s_stats.pitch_end = pitch;
    s_stats.roll_end = roll;

    /* 每 100 帧打印进度 */
    if ((s_stats.frame_count % 100) == 0) {
        uint8_t pct = app_test_timer_progress();
        uint32_t remain = app_test_timer_remaining_ms() / 1000;
        printf("[TEST] %3lu%% remain=%lus yaw=%.2f gyro=%.1f\r\n",
               (unsigned long)pct, (unsigned long)remain,
               (double)yaw, (double)gyro_mag);
    }
}

/**
 * @brief 启动动态转动精度测试
 */
int app_test_runner_start_turn(float target_angle, uint32_t timeout_sec)
{
    if (s_stats.active) {
        printf("[TEST] Test already running\r\n");
        return -1;
    }

    /* [B6修复] 输入参数边界检查 */
    if (fabsf(target_angle) < 1.0f) {
        printf("[TEST] Invalid target_angle=%.1f (must be |angle|>=1)\r\n",
               (double)target_angle);
        return -1;
    }
    if (timeout_sec == 0 || timeout_sec > 3600u) {
        printf("[TEST] Invalid timeout=%lu (must be 1..3600)\r\n",
               (unsigned long)timeout_sec);
        return -1;
    }

    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.active = true;
    s_stats.mode = TEST_MODE_TURN;
    s_stats.turn_state = TURN_STATE_STATIC_REF;
    s_stats.target_angle = target_angle;

    app_test_timer_set_callback(on_test_timeout);
    if (app_test_timer_start(timeout_sec) != 0) {
        s_stats.active = false;
        printf("[TEST] Failed to start timer\r\n");
        return -1;
    }

    printf("\r\n");
    printf("========================================\r\n");
    printf("[TEST] Turn Test Started\r\n");
    printf("[TEST] Target: %.1f deg, Timeout: %lu sec\r\n",
           (double)target_angle, (unsigned long)timeout_sec);
    printf("[TEST] Phase 1: Static reference (2s)...\r\n");
    printf("========================================\r\n");

    return 0;
}

/**
 * @brief 手动确认转动结束
 */
void app_test_runner_end_turn(void)
{
    if (!s_stats.active || s_stats.mode != TEST_MODE_TURN) {
        printf("[TEST] turnend: No turn test running\r\n");
        return;
    }
    if (s_stats.turn_state != TURN_STATE_TURNING) {
        printf("[TEST] turnend: Not in TURNING state (current=%d)\r\n",
               (int)s_stats.turn_state);
        return;
    }
    if (!s_stats.turn_detected) {
        printf("[TEST] turnend: Turn not yet detected (gyro < %.1f dps)\r\n",
               (double)TURN_START_GYRO_DPS);
        return;
    }

    uint32_t now_ms = app_test_timer_elapsed_ms();
    s_stats.turn_state = TURN_STATE_STABLE;
    s_stats.state_start_ms = now_ms;
    s_stats.turn_end_ms = now_ms;      /* [B12] 记录转动结束时间 */
    s_stats.stable_start_ms = now_ms;  /* [B12] 记录稳定阶段开始时间 */
    /* 记录转动结束时 3-axis bias */
    s_stats.turn_bias_x_end = s_stats.bias_x_last;
    s_stats.turn_bias_y_end = s_stats.bias_y_last;
    s_stats.turn_bias_z_end = s_stats.bias_z_last;
    s_stats.stable_frame_count = 0;  /* 重置, stable 阶段首帧重新初始化 */
    printf("[TEST] Turn END (manual), yaw=%.3f, turned=%.3f deg, now stable 2s...\r\n",
           (double)s_stats.yaw_turn_end,
           (double)(s_stats.yaw_turn_end - s_stats.yaw_turn_start));
}

/**
 * @brief 转动测试报告输出 (扩展版: 分段统计 + bias_z 门控验证)
 */
static void turn_stop(void)
{
    uint32_t elapsed_ms = app_test_timer_elapsed_ms();
    float elapsed_sec = (float)elapsed_ms / 1000.0f;

    /* 计算实际转动角度 */
    float actual_turn = 0.0f;
    float error = 0.0f;
    float error_pct = 0.0f;
    const char *verdict = "N/A";
    bool wrong_direction = false;  /* [B11] 方向相反检测, 需在 turn_detected 外使用 */

    if (s_stats.turn_detected) {
        /* 实际转动 = 稳定后 yaw - 参考 yaw
         * [B1修复] 若超时在 TURNING 阶段触发, yaw_stable_end 未被赋值(=0),
         * 此时用 yaw_turn_end 作为 fallback, 避免实际转动计算为 -yaw_ref 的错误 */
        float yaw_final = s_stats.yaw_stable_end;
        if (s_stats.stable_frame_count == 0) {
            yaw_final = s_stats.yaw_turn_end;
        }
        actual_turn = yaw_final - s_stats.yaw_ref;
        error = actual_turn - s_stats.target_angle;
        error_pct = (s_stats.target_angle != 0.0f)
                  ? 100.0f * error / s_stats.target_angle : 0.0f;

        /* [B11修复] 检测方向相反: 实际转动方向与目标方向不一致
         * 当 |actual_turn + target_angle| < |actual_turn - target_angle| 时,
         * 说明用户转反了方向, 此时按绝对值计算误差更合理 */
        if (s_stats.target_angle != 0.0f) {
            float same_dir_error = fabsf(actual_turn - s_stats.target_angle);
            float opp_dir_error  = fabsf(actual_turn + s_stats.target_angle);
            if (opp_dir_error < same_dir_error) {
                wrong_direction = true;
                /* 重新按绝对值计算误差 */
                error = fabsf(actual_turn) - fabsf(s_stats.target_angle);
                error_pct = 100.0f * error / fabsf(s_stats.target_angle);
            }
        }

        if (wrong_direction) {
            verdict = "FAIL (wrong direction)";
        } else if (fabsf(error) <= TURN_PASS_THRESHOLD) {
            verdict = "PASS";
        } else {
            verdict = "FAIL";
        }
    }

    printf("\r\n");
    printf("========================================\r\n");
    printf("[TEST] Turn Test Finished\r\n");
    printf("========================================\r\n");
    printf("  Duration       : %.1f sec\r\n", (double)elapsed_sec);
    printf("  Frames         : %lu\r\n", (unsigned long)s_stats.frame_count);
    if (s_stats.turn_detected) {
        printf("  ---- Angle ----\r\n");
        printf("  Yaw reference  : %.3f deg\r\n", (double)s_stats.yaw_ref);
        printf("  Yaw turn start : %.3f deg\r\n", (double)s_stats.yaw_turn_start);
        printf("  Yaw turn end   : %.3f deg\r\n", (double)s_stats.yaw_turn_end);
        printf("  Yaw stable end : %.3f deg\r\n", (double)s_stats.yaw_stable_end);
        printf("  Target angle   : %.3f deg\r\n", (double)s_stats.target_angle);
        printf("  Actual turn    : %.3f deg\r\n", (double)actual_turn);
        if (wrong_direction) {
            printf("  [WARNING] Wrong direction! Target=%+.1f, Actual=%+.1f\r\n",
                   (double)s_stats.target_angle, (double)actual_turn);
        }
        printf("  Error          : %.3f deg (%.2f%%)\r\n",
               (double)error, (double)error_pct);
        /* [B12修复] 使用实际时间戳而非帧数/100, 避免采样率假设错误 */
        float turn_duration_sec = (float)(s_stats.turn_end_ms - s_stats.turn_start_ms) / 1000.0f;
        if (turn_duration_sec < 0.0f) turn_duration_sec = 0.0f;
        printf("  Turn frames    : %lu (%.1f sec)\r\n",
               (unsigned long)s_stats.turn_frame_count,
               (double)turn_duration_sec);
        printf("  Max jump       : %.4f deg\r\n",
               (double)s_stats.turn_yaw_max_jump);

        /* [B10修复] 增加 pitch/roll 漂移报告 (验证 ACC 修正质量) */
        printf("  Pitch start    : %.3f deg\r\n", (double)s_stats.pitch_start);
        printf("  Pitch end      : %.3f deg\r\n", (double)s_stats.pitch_end);
        printf("  Pitch drift    : %.3f deg\r\n",
               (double)(s_stats.pitch_end - s_stats.pitch_start));
        printf("  Roll start     : %.3f deg\r\n", (double)s_stats.roll_start);
        printf("  Roll end       : %.3f deg\r\n", (double)s_stats.roll_end);
        printf("  Roll drift     : %.3f deg\r\n",
               (double)(s_stats.roll_end - s_stats.roll_start));

        /* ---- 分段: 转动过程统计 (验证慢转 yaw 增长率) ---- */
        float turn_yaw_inc = s_stats.yaw_turn_end - s_stats.yaw_turn_start;
        /* [B12修复] 复用上方基于实际时间戳的 turn_duration_sec */
        float turn_yaw_rate = (turn_duration_sec > 0.1f)
                            ? turn_yaw_inc / turn_duration_sec : 0.0f;
        printf("  ---- Turn Phase ----\r\n");
        printf("  yaw_increase   : %.3f deg (turn_start->turn_end)\r\n",
               (double)turn_yaw_inc);
        printf("  yaw_rate       : %.3f deg/s (avg)\r\n",
               (double)turn_yaw_rate);

        /* ---- 分段: 稳定阶段统计 (验证"停下后yaw变动"修复) ---- */
        if (s_stats.stable_frame_count > 0) {
            float stable_yaw_drift = s_stats.stable_yaw_end - s_stats.stable_yaw_start;
            float stable_yaw_range = s_stats.stable_yaw_max - s_stats.stable_yaw_min;
            /* [B12修复] 使用实际时间戳而非帧数/100 */
            uint32_t now = app_test_timer_elapsed_ms();
            float stable_duration_sec = (float)(now - s_stats.stable_start_ms) / 1000.0f;
            if (stable_duration_sec < 0.0f) stable_duration_sec = 0.0f;
            float stable_drift_rate = (stable_duration_sec > 0.1f)
                                     ? fabsf(stable_yaw_drift) / stable_duration_sec : 0.0f;
            printf("  ---- Stable Phase ----\r\n");
            printf("  yaw_start      : %.3f deg\r\n", (double)s_stats.stable_yaw_start);
            printf("  yaw_end        : %.3f deg\r\n", (double)s_stats.stable_yaw_end);
            printf("  yaw_drift      : %.3f deg\r\n", (double)stable_yaw_drift);
            printf("  yaw_range      : %.3f deg\r\n", (double)stable_yaw_range);
            printf("  drift_rate     : %.4f deg/s\r\n", (double)stable_drift_rate);
            printf("  frames         : %lu (%.1f sec)\r\n",
                   (unsigned long)s_stats.stable_frame_count,
                   (double)stable_duration_sec);
        }

        /* ---- 3-axis bias tracking (turn phase) ---- */
        if (s_stats.bias_initialized) {
            printf("  ---- KF Bias Tracking ----\r\n");
            printf("  turn x: start=%.4f end=%.4f diff=%.4f\r\n",
                   (double)s_stats.turn_bias_x_start, (double)s_stats.turn_bias_x_end,
                   (double)(s_stats.turn_bias_x_end - s_stats.turn_bias_x_start));
            printf("  turn y: start=%.4f end=%.4f diff=%.4f\r\n",
                   (double)s_stats.turn_bias_y_start, (double)s_stats.turn_bias_y_end,
                   (double)(s_stats.turn_bias_y_end - s_stats.turn_bias_y_start));
            printf("  turn z: start=%.4f end=%.4f diff=%.4f\r\n",
                   (double)s_stats.turn_bias_z_start, (double)s_stats.turn_bias_z_end,
                   (double)(s_stats.turn_bias_z_end - s_stats.turn_bias_z_start));
            printf("  stable x: start=%.4f end=%.4f diff=%.4f\r\n",
                   (double)s_stats.stable_bias_x_start, (double)s_stats.stable_bias_x_end,
                   (double)(s_stats.stable_bias_x_end - s_stats.stable_bias_x_start));
            printf("  stable y: start=%.4f end=%.4f diff=%.4f\r\n",
                   (double)s_stats.stable_bias_y_start, (double)s_stats.stable_bias_y_end,
                   (double)(s_stats.stable_bias_y_end - s_stats.stable_bias_y_start));
            printf("  stable z: start=%.4f end=%.4f diff=%.4f\r\n",
                   (double)s_stats.stable_bias_z_start, (double)s_stats.stable_bias_z_end,
                   (double)(s_stats.stable_bias_z_end - s_stats.stable_bias_z_start));
        } else {
            printf("  ---- KF Bias Tracking ---- (no diag data)\r\n");
        }

        printf("  ---- Verdict ----\r\n");
        printf("  Angle: %s (threshold: +/-%.1f deg)\r\n",
               verdict, (double)TURN_PASS_THRESHOLD);
    } else {
        printf("  [WARNING] No turn detected (gyro < %.1f dps)\r\n",
               (double)TURN_START_GYRO_DPS);
        printf("  Yaw ref        : %.3f deg\r\n", (double)s_stats.yaw_ref);
        printf("  Yaw final      : %.3f deg\r\n", (double)s_stats.yaw_stable_end);
    }

    /* ---- 3-axis bias overall ---- */
    if (s_stats.bias_initialized) {
        printf("  ---- KF Bias Overall ----\r\n");
        printf("  x: start=%.4f end=%.4f total=%.4f range=[%.4f,%.4f]\r\n",
               (double)s_stats.bias_x_start, (double)s_stats.bias_x_end,
               (double)(s_stats.bias_x_end - s_stats.bias_x_start),
               (double)s_stats.bias_x_min, (double)s_stats.bias_x_max);
        printf("  y: start=%.4f end=%.4f total=%.4f range=[%.4f,%.4f]\r\n",
               (double)s_stats.bias_y_start, (double)s_stats.bias_y_end,
               (double)(s_stats.bias_y_end - s_stats.bias_y_start),
               (double)s_stats.bias_y_min, (double)s_stats.bias_y_max);
        printf("  z: start=%.4f end=%.4f total=%.4f range=[%.4f,%.4f]\r\n",
               (double)s_stats.bias_z_start, (double)s_stats.bias_z_end,
               (double)(s_stats.bias_z_end - s_stats.bias_z_start),
               (double)s_stats.bias_z_min, (double)s_stats.bias_z_max);
        printf("  max_delta     : %.4f dps (single frame)\r\n",
               (double)s_stats.bias_max_delta);
    }

    /* ---- 故障检测摘要 ---- */
    printf("  ---- Fault Detect ----\r\n");
    printf("  nan_inf       : %lu\r\n", (unsigned long)s_stats.fault_nan_inf);
    printf("  diverge       : %lu (yaw > 720 deg)\r\n",
           (unsigned long)s_stats.fault_diverge);
    printf("  acc_anomaly   : %lu (acc outside [0.5,2.0]g)\r\n",
           (unsigned long)s_stats.fault_acc_anomaly);

    /* ---- 综合判定 ---- */
    /* [B3修复] 未转动时输出 FAIL 而非 N/A, 并始终打印 [OVERALL]
     * [B11修复] 方向相反时明确提示 */
    const char *overall;
    if (!s_stats.turn_detected) {
        overall = "FAIL (no turn detected)";
    } else if (s_stats.fault_nan_inf > 0 || s_stats.fault_diverge > 0) {
        overall = "FAIL (critical fault)";
    } else if (wrong_direction) {
        overall = "FAIL (wrong direction)";
    } else if (fabsf(error) > TURN_PASS_THRESHOLD) {
        overall = "FAIL (angle error)";
    } else {
        overall = "PASS";
    }
    printf("  [OVERALL] %s\r\n", overall);
    printf("========================================\r\n\r\n");

    app_test_timer_cleanup();
    s_stats.active = false;
}

/* ============================================================
 * KF 参数扫描测试 (KFTUNE 模式)
 *
 * 扫描 Q_angle × Q_bias 组合, 固定 R_measure=0.03, R_zupt=0.04
 * 每组测试 N 秒静止漂移, 输出 yaw_drift/drift_rate/std/max_jump
 * 最终输出汇总表和最优参数推荐
 *
 * 使用方式: 串口 "kftune 10" (每组 10 秒, 共 9 组 ≈ 90 秒)
 * 测试期间保持设备静止!
 * ============================================================ */

/* 参数扫描矩阵 (3×3 = 9 组) */
#define KFTUNE_Q_ANGLE_COUNT  3
#define KFTUNE_Q_BIAS_COUNT   3
#define KFTUNE_SET_COUNT      (KFTUNE_Q_ANGLE_COUNT * KFTUNE_Q_BIAS_COUNT)

/* 固定参数 */
#define KFTUNE_R_MEASURE_FIXED  0.03f
#define KFTUNE_R_ZUPT_FIXED     0.04f   /* 启用 ZUPT (0.2dps RMS 对应) */

/* Q_angle 扫描值 (deg²/s)
 * 0.0005: 低噪声, 信任陀螺
 * 0.001:  中等
 * 0.003:  当前默认 (KFTUNE 最优 Set 7) */
static const float kftune_q_angle_list[KFTUNE_Q_ANGLE_COUNT] = {
    0.0005f, 0.001f, 0.003f
};

/* Q_bias 扫描值 (dps²/s)
 * 0.001:  当前默认 (KFTUNE 最优 Set 7, 稳定)
 * 0.003:  中等
 * 0.005:  快跟踪, 噪声大 */
static const float kftune_q_bias_list[KFTUNE_Q_BIAS_COUNT] = {
    0.001f, 0.003f, 0.005f
};

/* 每组测试结果 */
typedef struct {
    float q_angle;
    float q_bias;
    float yaw_drift;        /* 度 */
    float drift_rate;       /* 度/秒 */
    float yaw_std;          /* 度 */
    float max_jump;         /* 度 */
    float bias_z_end;       /* dps */
    uint32_t frame_count;
    bool    completed;      /* 该组是否完整完成 (提前中止时为 false) */
    bool    valid;          /* [B8] 该组数据是否有效 (运动帧数 < 5%) */
    uint32_t motion_frames; /* [B8] 检测到运动的帧数 */
} kftune_result_t;

static kftune_result_t s_kftune_results[KFTUNE_SET_COUNT];

/* 前向声明 */
static void kftune_start_set(uint32_t index);
static void kftune_print_summary(void);
static void kftune_stop(void);

/**
 * @brief KF 参数扫描启动
 */
int app_test_runner_start_kftune(uint32_t duration_per_set)
{
    if (s_stats.active) {
        printf("[TEST] Test already running\r\n");
        return -1;
    }

    /* [B6修复] 输入参数边界检查 (避免 duration=0 导致死循环) */
    if (duration_per_set == 0 || duration_per_set > 600u) {
        printf("[TEST] Invalid duration_per_set=%lu (must be 1..600)\r\n",
               (unsigned long)duration_per_set);
        return -1;
    }

    /* 检查 filter 可用性 */
    filter_t *f = app_imu_get_filter();
    if (f == NULL) {
        printf("[TEST] kftune: filter not initialized\r\n");
        return -1;
    }
    if (f->type != FILTER_TYPE_KF) {
        printf("[TEST] kftune: filter type=%d, need KF\r\n", (int)f->type);
        return -1;
    }

    memset(&s_stats, 0, sizeof(s_stats));
    memset(s_kftune_results, 0, sizeof(s_kftune_results));
    s_stats.active = true;
    s_stats.mode = TEST_MODE_KFTUNE;
    s_stats.kftune_set_index = 0;
    s_stats.kftune_set_duration_ms = duration_per_set * 1000u;

    /* 启动计时器 (kftune 自己管理多组定时, 用 elapsed_ms) */
    app_test_timer_set_callback(on_test_timeout);
    if (app_test_timer_start(duration_per_set * KFTUNE_SET_COUNT + 10) != 0) {
        s_stats.active = false;
        printf("[TEST] Failed to start timer\r\n");
        return -1;
    }

    printf("\r\n");
    printf("========================================\r\n");
    printf("[KFTUNE] KF Parameter Sweep Started\r\n");
    printf("[KFTUNE] %u sets x %u sec = %u sec total\r\n",
           (unsigned)KFTUNE_SET_COUNT,
           (unsigned)duration_per_set,
           (unsigned)(KFTUNE_SET_COUNT * duration_per_set));
    printf("[KFTUNE] Fixed: R_measure=%.4f, R_zupt=%.4f\r\n",
           (double)KFTUNE_R_MEASURE_FIXED, (double)KFTUNE_R_ZUPT_FIXED);
    printf("[KFTUNE] Sweep: Q_angle x Q_bias\r\n");
    printf("[KFTUNE] KEEP DEVICE STILL!\r\n");
    printf("========================================\r\n");

    /* 设置 pending 标志, 首组在 IMU 任务中启动 (避免与 update() 并发) */
    s_stats.kftune_pending_start = true;

    return 0;
}

/**
 * @brief 启动指定组的测试
 */
static void kftune_start_set(uint32_t index)
{
    if (index >= KFTUNE_SET_COUNT) {
        /* 所有组完成, 输出汇总 */
        kftune_print_summary();
        app_test_timer_cleanup();
        s_stats.active = false;
        return;
    }

    uint32_t qa_idx = index / KFTUNE_Q_BIAS_COUNT;
    uint32_t qb_idx = index % KFTUNE_Q_BIAS_COUNT;
    float q_angle = kftune_q_angle_list[qa_idx];
    float q_bias  = kftune_q_bias_list[qb_idx];

    /* 设置 KF 参数 */
    filter_t *f = app_imu_get_filter();
    if (f == NULL) {
        printf("[KFTUNE] filter NULL, abort\r\n");
        app_test_timer_cleanup();  /* 清理计时器, 避免后续测试无法启动 */
        s_stats.active = false;
        return;
    }

    /* 固定 R_measure 和 R_zupt, 设置当前组的 Q_angle 和 Q_bias */
    f->set_param(f, FILTER_PARAM_KF_Q_ANGLE, q_angle);
    f->set_param(f, FILTER_PARAM_KF_Q_BIAS, q_bias);
    f->set_param(f, FILTER_PARAM_KF_R_MEASURE, KFTUNE_R_MEASURE_FIXED);
    f->set_param(f, FILTER_PARAM_KF_R_ZUPT, KFTUNE_R_ZUPT_FIXED);

    /* 重置滤波器状态, 让新参数从零开始收敛 */
    app_imu_reset_filter();

    /* 重置当前组统计 */
    s_stats.kftune_set_index = index;
    s_stats.kftune_set_start_ms = app_test_timer_elapsed_ms();
    s_stats.kftune_set_yaw_start = 0.0f;
    s_stats.kftune_set_yaw_end = 0.0f;
    s_stats.kftune_set_yaw_max = -1e30f;
    s_stats.kftune_set_yaw_min = 1e30f;
    s_stats.kftune_set_yaw_mean = 0.0f;
    s_stats.kftune_set_yaw_M2 = 0.0f;
    s_stats.kftune_set_max_jump = 0.0f;
    s_stats.kftune_set_yaw_init = false;
    s_stats.frame_count = 0;
    s_stats.yaw_offset = 0.0f;
    s_stats.yaw_last_raw = 0.0f;
    /* [B4修复] 每组重置故障计数器, 保证统计与帧数一致 */
    s_stats.fault_nan_inf = 0;
    s_stats.fault_diverge = 0;
    s_stats.fault_acc_anomaly = 0;
    /* [B8] 每组重置运动检测计数器 */
    s_stats.kftune_motion_frames = 0;

    printf("\r\n[KFTUNE] Set %u/%u: Q_angle=%.4f, Q_bias=%.4f\r\n",
           (unsigned)(index + 1), (unsigned)KFTUNE_SET_COUNT,
           (double)q_angle, (double)q_bias);
    printf("[KFTUNE]   (reset filter, testing %u sec...)\r\n",
           (unsigned)(s_stats.kftune_set_duration_ms / 1000u));
}

/**
 * @brief KF 参数扫描每帧喂入
 * [B8修复] 增加 gyro_mag 运动检测, 标记组有效性
 */
static void kftune_feed(float pitch, float roll, float yaw,
                        const test_diag_ctx_t *diag)
{
    /* [B8] 运动检测: gyro_mag > 5dps 视为运动 (静态测试应 <2dps) */
    if (diag != NULL) {
        if (diag->gyro_mag_dps > 5.0f) {
            s_stats.kftune_motion_frames++;
        }
    }

    /* unwrap yaw (跨边界处理) */
    float yaw_unwrapped = yaw;
    if (s_stats.kftune_set_yaw_init) {
        float dy = yaw - s_stats.yaw_last_raw;
        if (dy > 180.0f) {
            s_stats.yaw_offset -= 360.0f;
        } else if (dy < -180.0f) {
            s_stats.yaw_offset += 360.0f;
        }
        yaw_unwrapped = yaw + s_stats.yaw_offset;
    }
    s_stats.yaw_last_raw = yaw;

    if (!s_stats.kftune_set_yaw_init) {
        s_stats.kftune_set_yaw_start = yaw_unwrapped;
        s_stats.kftune_set_yaw_end = yaw_unwrapped;
        s_stats.kftune_set_yaw_max = yaw_unwrapped;
        s_stats.kftune_set_yaw_min = yaw_unwrapped;
        s_stats.kftune_set_yaw_mean = yaw_unwrapped;
        s_stats.kftune_set_yaw_init = true;
    } else {
        /* 最大跳变 */
        float jump = fabsf(yaw_unwrapped - s_stats.kftune_set_yaw_end);
        if (jump > s_stats.kftune_set_max_jump) {
            s_stats.kftune_set_max_jump = jump;
        }
        /* Welford 在线方差 (frame_count 由 feed_diag 统一递增, 此处不再重复) */
        float delta = yaw_unwrapped - s_stats.kftune_set_yaw_mean;
        s_stats.kftune_set_yaw_mean += delta / (float)s_stats.frame_count;
        float delta2 = yaw_unwrapped - s_stats.kftune_set_yaw_mean;
        s_stats.kftune_set_yaw_M2 += delta * delta2;
    }
    s_stats.kftune_set_yaw_end = yaw_unwrapped;
    if (yaw_unwrapped > s_stats.kftune_set_yaw_max) s_stats.kftune_set_yaw_max = yaw_unwrapped;
    if (yaw_unwrapped < s_stats.kftune_set_yaw_min) s_stats.kftune_set_yaw_min = yaw_unwrapped;
}

/**
 * @brief KF 参数扫描轮询 (检查当前组是否超时)
 */
static void kftune_poll(void)
{
    /* 首组延迟启动: 在 IMU 任务上下文中执行, 避免与 update() 并发 */
    if (s_stats.kftune_pending_start) {
        s_stats.kftune_pending_start = false;
        kftune_start_set(0);
        return;
    }

    uint32_t now_ms = app_test_timer_elapsed_ms();
    uint32_t elapsed_ms = now_ms - s_stats.kftune_set_start_ms;

    if (elapsed_ms >= s_stats.kftune_set_duration_ms) {
        /* 当前组结束, 记录结果 */
        uint32_t idx = s_stats.kftune_set_index;
        uint32_t qa_idx = idx / KFTUNE_Q_BIAS_COUNT;
        uint32_t qb_idx = idx % KFTUNE_Q_BIAS_COUNT;

        float duration_sec = (float)s_stats.kftune_set_duration_ms / 1000.0f;
        float yaw_drift = s_stats.kftune_set_yaw_end - s_stats.kftune_set_yaw_start;
        float drift_rate = (duration_sec > 0.1f)
                         ? fabsf(yaw_drift) / duration_sec : 0.0f;
        float yaw_std = 0.0f;
        if (s_stats.frame_count > 1) {
            float variance = s_stats.kftune_set_yaw_M2 / (float)(s_stats.frame_count - 1);
            yaw_std = sqrtf(variance);
        }

        s_kftune_results[idx].q_angle = kftune_q_angle_list[qa_idx];
        s_kftune_results[idx].q_bias  = kftune_q_bias_list[qb_idx];
        s_kftune_results[idx].yaw_drift = yaw_drift;
        s_kftune_results[idx].drift_rate = drift_rate;
        s_kftune_results[idx].yaw_std = yaw_std;
        s_kftune_results[idx].max_jump = s_stats.kftune_set_max_jump;
        s_kftune_results[idx].frame_count = s_stats.frame_count;
        s_kftune_results[idx].completed = true;  /* 标记本组已完成 */
        /* [B8] 记录运动帧数并判定有效性 (运动帧 < 5% 总帧数) */
        s_kftune_results[idx].motion_frames = s_stats.kftune_motion_frames;
        uint32_t motion_limit = (s_stats.frame_count + 19u) / 20u;  /* ~5% */
        s_kftune_results[idx].valid = (s_stats.kftune_motion_frames <= motion_limit);

        /* 读取 bias_z */
        filter_t *f = app_imu_get_filter();
        if (f != NULL) {
            filter_kf_diag_t kf_diag;
            if (filter_kf_get_diag(f, &kf_diag) == 0) {
                s_kftune_results[idx].bias_z_end = kf_diag.bias_z;
            }
        }

        printf("[KFTUNE]   drift=%+.3f deg, rate=%.4f deg/s, std=%.4f, jump=%.4f, bias_z=%.4f\r\n",
               (double)yaw_drift, (double)drift_rate,
               (double)yaw_std, (double)s_stats.kftune_set_max_jump,
               (double)s_kftune_results[idx].bias_z_end);

        /* 启动下一组 */
        kftune_start_set(idx + 1);
    }
}

/**
 * @brief KF 参数扫描汇总报告
 */
static void kftune_print_summary(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("[KFTUNE] Summary Report\r\n");
    printf("========================================\r\n");
    printf("Fixed: R_measure=%.4f, R_zupt=%.4f\r\n",
           (double)KFTUNE_R_MEASURE_FIXED, (double)KFTUNE_R_ZUPT_FIXED);
    printf("----------------------------------------\r\n");
    printf("%-4s %-8s %-8s %-10s %-10s %-8s %-8s %-8s %-7s\r\n",
           "Set", "Q_angle", "Q_bias", "drift(deg)", "rate(d/s)",
           "std(deg)", "jump(deg)", "bias_z", "valid");
    printf("----------------------------------------\r\n");

    /* [B5修复] 最优选择: 综合评分 = drift_rate + std + max_jump (加权求和)
     * 静态测试中 drift_rate 都接近 0, 单一指标被噪声主导
     * 权重: drift_rate(10) + std(100) + max_jump(50), 数值越小越优
     * 仅在已完成且有效的组中选择 */
    uint32_t best_idx = 0;
    float best_score = 1e30f;
    bool found_best = false;
    for (uint32_t i = 0; i < KFTUNE_SET_COUNT; i++) {
        printf("%-4u %-8.4f %-8.4f %-10.3f %-10.4f %-8.4f %-8.4f %-8.4f %s\r\n",
               (unsigned)(i + 1),
               (double)s_kftune_results[i].q_angle,
               (double)s_kftune_results[i].q_bias,
               (double)s_kftune_results[i].yaw_drift,
               (double)s_kftune_results[i].drift_rate,
               (double)s_kftune_results[i].yaw_std,
               (double)s_kftune_results[i].max_jump,
               (double)s_kftune_results[i].bias_z_end,
               s_kftune_results[i].completed
                 ? (s_kftune_results[i].valid ? "OK" : "MOVED")
                 : "INC");
        if (s_kftune_results[i].completed && s_kftune_results[i].valid) {
            /* 综合评分: 越小越优 */
            float score = s_kftune_results[i].drift_rate * 10.0f
                        + s_kftune_results[i].yaw_std * 100.0f
                        + s_kftune_results[i].max_jump * 50.0f;
            if (score < best_score) {
                best_score = score;
                best_idx = i;
                found_best = true;
            }
        }
    }
    printf("----------------------------------------\r\n");
    if (!found_best) {
        printf("[BEST] No valid set found (aborted or device moved)\r\n");
        printf("========================================\r\n\r\n");
        /* 仍恢复默认参数 */
    } else {
        printf("[BEST] Set %u: Q_angle=%.4f, Q_bias=%.4f\r\n",
           (unsigned)(best_idx + 1),
           (double)s_kftune_results[best_idx].q_angle,
           (double)s_kftune_results[best_idx].q_bias);
    printf("       drift_rate=%.4f deg/s, std=%.4f deg, jump=%.4f deg\r\n",
           (double)s_kftune_results[best_idx].drift_rate,
           (double)s_kftune_results[best_idx].yaw_std,
           (double)s_kftune_results[best_idx].max_jump);
    printf("       bias_z=%.4f dps\r\n",
           (double)s_kftune_results[best_idx].bias_z_end);
    printf("========================================\r\n\r\n");
    }

    /* 恢复默认参数, 但保持 ZUPT 启用 */
    filter_t *f = app_imu_get_filter();
    if (f != NULL) {
        f->set_param(f, FILTER_PARAM_KF_Q_ANGLE, KF_Q_ANGLE_DEFAULT);
        f->set_param(f, FILTER_PARAM_KF_Q_BIAS, KF_Q_BIAS_DEFAULT);
        f->set_param(f, FILTER_PARAM_KF_R_MEASURE, KF_R_MEASURE_DEFAULT);
        f->set_param(f, FILTER_PARAM_KF_R_ZUPT, KFTUNE_R_ZUPT_FIXED);
        app_imu_reset_filter();
        printf("[KFTUNE] Restored default params (Q_a=" TOSTR(KF_Q_ANGLE_DEFAULT) ", Q_b=" TOSTR(KF_Q_BIAS_DEFAULT) ", R_m=" TOSTR(KF_R_MEASURE_DEFAULT) ", R_z=0.04)\r\n");
        printf("[KFTUNE] NOTE: R_zupt kept at 0.04 (ZUPT enabled)\r\n\r\n");
    }
}

/**
 * @brief KF 参数扫描停止 (提前中止)
 */
static void kftune_stop(void)
{
    printf("\r\n[KFTUNE] Test stopped (set %u/%u)\r\n",
           (unsigned)(s_stats.kftune_set_index + 1),
           (unsigned)KFTUNE_SET_COUNT);
    kftune_print_summary();
    app_test_timer_cleanup();
    s_stats.active = false;
}
