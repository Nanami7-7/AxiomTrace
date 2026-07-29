/**
 * @file    task_imu.c
 * @brief   IMU采集任务实现 (LSM6DSR)
 * @note    10ms周期(100Hz):
 *          使用 LSM6DSR BSP 层 API 获取姿态数据
 *          支持多种滤波器: 互补/EKF/Mahony/Madgwick/LKF/LPF
 *          优先级: 4(介于control=5和menu=2之间)
 */
#include "task_imu.h"
#include "app_main.h"
#include "osal_api.h"
#include "project_config.h"
#include "bsp_lsm6dsr.h"
#include "platform.h"
#include "bsp_timer.h"
#include "spi_bridge.h"
#include "app_test_runner.h"
#include <stdio.h>
#include <string.h>
#include <math.h>   /* sqrtf, 用于跳变诊断 acc_norm 计算 */

/* LSM6DSR BSP 上下文 (静态分配) */
static bsp_lsm6dsr_ctx_t g_imu_ctx;

/* 初始化标志 */
static uint8_t g_imu_initialized = 0;

/* 打印计数器 (控制打印频率) */
/** KF 滤波器静态缓冲区大小 (字节)，需容纳 filter_t + kf_priv_t */

static uint32_t g_kf_filter_buf[PRJ_IMU_KF_FILTER_BUF_SIZE / sizeof(uint32_t)];

/**
 * @brief IMU 初始化 (在任务内部调用)
 * @return 0=成功, -1=失败
 */
static int imu_init(void)
{
#if (PRJ_IMU_STARTUP_DIAG_ENABLE != 0U)
    printf("[IMU-DIAG] task entered tick_ms=%lu\r\n",
           (unsigned long)osal_ticks_to_ms(osal_get_tick_count()));
#endif

    /* 1. Initialize SPI bridge. */
    spi_bridge_init();
#if (PRJ_IMU_STARTUP_DIAG_ENABLE != 0U)
    printf("[IMU-DIAG] spi_bridge_init done tick_ms=%lu\r\n",
           (unsigned long)osal_ticks_to_ms(osal_get_tick_count()));
#endif

    /* 2. Start the platform timer used for diagnostics. */
    platform_timer_init();
#if (PRJ_IMU_STARTUP_DIAG_ENABLE != 0U)
    printf("[IMU-DIAG] platform_timer_init done t_us=%lu\r\n",
           (unsigned long)bsp_get_us());
#endif
    
    /* 3. 初始化 LSM6DSR BSP 上下文 (默认使用互补滤波器) */
    if (bsp_lsm6dsr_init_ctx(&g_imu_ctx) != 0) {
        printf("[ERROR] bsp_lsm6dsr_init_ctx failed!\r\n");
        return -1;
    }
    printf("[INFO] bsp_lsm6dsr_init_ctx OK, filter=%s\r\n", 
           filter_type_name(g_imu_ctx.current_filter_type));
    
    /* 4. 销毁默认滤波器，使用静态分配创建 KF */
    if (g_imu_ctx.active_filter != NULL) {
        filter_destroy_safe(g_imu_ctx.active_filter);
        g_imu_ctx.active_filter = NULL;
    }
    
    /* 5. 使用静态分配创建 KF 滤波器 (避免堆分配失败) */
    printf("[INFO] Creating KF filter (static alloc)...\r\n");
    g_imu_ctx.active_filter = filter_create_static(
        FILTER_TYPE_KF, 
        g_kf_filter_buf, 
        sizeof(g_kf_filter_buf)
    );
    
    if (g_imu_ctx.active_filter == NULL) {
        printf("[ERROR] KF filter create failed! buf_size=%u\r\n", (unsigned)PRJ_IMU_KF_FILTER_BUF_SIZE);
        /* 回退到互补滤波器 */
        g_imu_ctx.active_filter = filter_create(FILTER_TYPE_KF);
        if (g_imu_ctx.active_filter == NULL) {
            printf("[ERROR] KF dynamic fallback also failed!\r\n");
            return -1;
        }
        g_imu_ctx.current_filter_type = FILTER_TYPE_KF;
        printf("[INFO] Fallback to KF filter\r\n");
    } else {
        g_imu_ctx.current_filter_type = FILTER_TYPE_KF;
        printf("[INFO] KF filter created OK (static)\r\n");
        
        /* KF 专用参数调优 - 针对 LSM6DSR @ 100Hz
         * 参数来源: kftune 静态扫描 9 组最优 (Set 7)
         *   Q_angle=0.003: 增大角度过程噪声, 加快角度跟踪
         *   Q_bias=0.001:  减小偏置过程噪声, bias 估计更稳定
         *   R_measure=0.03: ACC 测量噪声 (固定)
         *   R_zupt=0.04:   ZUPT 启用 (0.2dps RMS), yaw bias 唯一观测源 */
        g_imu_ctx.active_filter->set_param(g_imu_ctx.active_filter,
            FILTER_PARAM_KF_Q_ANGLE, PRJ_KF_Q_ANGLE_DEFAULT);

        g_imu_ctx.active_filter->set_param(g_imu_ctx.active_filter,
            FILTER_PARAM_KF_Q_BIAS, PRJ_KF_Q_BIAS_DEFAULT);

        g_imu_ctx.active_filter->set_param(g_imu_ctx.active_filter,
            FILTER_PARAM_KF_R_MEASURE, PRJ_KF_R_MEASURE_DEFAULT);

        /* R_zupt: ZUPT 伪测量噪声 (dps²), 启用 ZUPT 以观测 yaw bias
         * 0.04 对应 0.2dps RMS, 是 yaw 轴偏置的唯一观测途径 */
        g_imu_ctx.active_filter->set_param(g_imu_ctx.active_filter,
            FILTER_PARAM_KF_R_ZUPT, PRJ_KF_R_ZUPT_DEFAULT);

        printf("[INFO] KF params configured from project_config.h\r\n");
    }
    
    g_imu_initialized = 1;
    return 0;
}

struct filter* app_imu_get_filter(void)
{
    if (!g_imu_initialized) return NULL;
    return g_imu_ctx.active_filter;
}

void app_imu_reset_filter(void)
{
    if (!g_imu_initialized || g_imu_ctx.active_filter == NULL) return;
    g_imu_ctx.active_filter->reset(g_imu_ctx.active_filter);
}

/**
 * @brief 通过 DMA 发送 IMU 数据 (17通道 VOFA+ FireWater)
 * @param data IMU 数据指针
 * @param ctx  IMU 上下文指针 (含 EKF 诊断)
 * @note  17通道: ax,ay,az,gx,gy,gz, pitch,roll,yaw, kf_p00_x,kf_p00_y,kf_p00_z,gyro_mag,acc_err,kf_p11_x,temp,kf_bias_z
 *        诊断通道在 KF 模式下有效
 */
void app_imu_task(void *param)
{
    app_shared_ctx_t *ctx = (app_shared_ctx_t *)param;
    bsp_lsm6dsr_data_t data;

    if (ctx == NULL) {
        osal_task_delete(NULL);
        return;
    }

    /* 初始化 IMU */
    if (imu_init() != 0) {
        /* 初始化失败，删除任务 */
        osal_task_delete(NULL);
        return;
    }
    printf("[INFO] imu_init done, entering main loop\r\n");

    uint32_t loop_count = 0;
    for (;;) {
        loop_count++;
        
        if (g_imu_initialized && bsp_lsm6dsr_update_ctx(&g_imu_ctx, &data) == 0) {
            OSAL_CRITICAL_SECTION {
                /* 姿态角 */
                ctx->imu.roll       = data.roll;
                ctx->imu.pitch      = data.pitch;
                ctx->imu.yaw        = data.yaw;
                
                /* 加速度 (m/s² → g) */
                ctx->imu.accel_x_g  = data.ax / PRJ_GRAVITY_MS2;
                ctx->imu.accel_y_g  = data.ay / PRJ_GRAVITY_MS2;
                ctx->imu.accel_z_g  = data.az / PRJ_GRAVITY_MS2;
                
                /* 角速度 (dps) */
                ctx->imu.gyro_x_dps = data.gx;
                ctx->imu.gyro_y_dps = data.gy;
                ctx->imu.gyro_z_dps = data.gz;
                
                /* 温度 */
                ctx->imu.temperature = data.temperature;
            }
            
            /* 时间戳 (OSAL tick, 不需要在临界区内) */
            ctx->imu.timestamp_ms = osal_ticks_to_ms(osal_get_tick_count());
            {

            /* Test runner: feed attitude data and poll timeout. */
            if (app_test_runner_is_active()) {
                /* 构造诊断上下文 */
                test_diag_ctx_t diag;
                diag.kf_bias_x      = g_imu_ctx.kf_bias_x;
                diag.kf_bias_y      = g_imu_ctx.kf_bias_y;
                diag.kf_bias_z      = g_imu_ctx.kf_bias_z;
                diag.kf_p00_x       = g_imu_ctx.kf_p00_x;
                diag.kf_p00_y       = g_imu_ctx.kf_p00_y;
                diag.kf_p00_z       = g_imu_ctx.kf_p00_z;
                diag.kf_p11_x       = g_imu_ctx.kf_p11_x;
                diag.kf_p11_y       = g_imu_ctx.kf_p11_y;
                diag.kf_p11_z       = g_imu_ctx.kf_p11_z;
                diag.gyro_mag_dps   = g_imu_ctx.gyro_mag_dps;
                diag.acc_norm_err   = g_imu_ctx.acc_norm_err;
                /* 计算 acc_norm (g), 用于 acc 门限诊断 */
                float ax_g = data.ax / PRJ_GRAVITY_MS2;
                float ay_g = data.ay / PRJ_GRAVITY_MS2;
                float az_g = data.az / PRJ_GRAVITY_MS2;
                diag.acc_norm = sqrtf(ax_g*ax_g + ay_g*ay_g + az_g*az_g);
                app_test_runner_feed_diag(data.pitch, data.roll, data.yaw, &diag);
                app_test_runner_poll();
            }
            }
        } else {
        }

        osal_task_delay_ms(PRJ_IMU_TASK_PERIOD_MS);
    }
}
