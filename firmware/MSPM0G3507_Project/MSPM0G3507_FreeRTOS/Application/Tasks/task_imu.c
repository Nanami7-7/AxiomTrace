/**
 * @file    task_imu.c
 * @brief   IMU采集任务实现 (LSM6DSR)
 * @note    10ms周期(100Hz):
 *          使用 LSM6DSR BSP 层 API 获取姿态数据
 *          支持多种滤波器: 互补/ESKF/Mahony/Madgwick/KF/LKF/LPF
 *          优先级: 4(介于control=5和menu=2之间)
 */
#include "task_imu.h"
#include "app_main.h"
#include "osal_api.h"
#include "project_config.h"
#include "bsp_lsm6dsr.h"
#include "filter_config.h"
#include "bsp_uart.h"
#include "platform.h"
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
#if (PRJ_IMU_UART_TELEMETRY_ENABLE != 0U)
/* IMU telemetry cadence and DMA buffer are compiled only when explicitly enabled. */
static uint32_t g_print_count = 0U;
#define IMU_PRINT_INTERVAL  (20U)
#define IMU_DMA_BUF_SIZE    (512U)
static char g_dma_buf[IMU_DMA_BUF_SIZE];
#endif
/** Caller-owned, correctly aligned storage; no heap allocation in production. */
static filter_static_storage_t g_imu_filter_storage;

/**
 * @brief IMU 初始化 (在任务内部调用)
 * @return 0=成功, -1=失败
 */
static int imu_init(void)
{
    /* 1. 初始化硬件 SPI */
    spi_bridge_init();
    
    /* 2. 初始化平台计时器 (TimerG8) */
    platform_timer_init();
    
    /* 3. Construct the production KF before BSP initialization. */
    filter_t *initial_filter = filter_create_static(
        FILTER_TYPE_KF,
        &g_imu_filter_storage,
        sizeof(g_imu_filter_storage));
    if (initial_filter == NULL) {
        printf("[ERROR] Static KF construction failed!\r\n");
        return -1;
    }

    /* 4. Initialize the sensor context with the caller-owned filter. */
    if (bsp_lsm6dsr_init_ctx_with_filter(&g_imu_ctx, initial_filter) != 0) {
        printf("[ERROR] bsp_lsm6dsr_init_ctx failed!\r\n");
        return -1;
    }
    printf("[INFO] bsp_lsm6dsr_init_ctx OK, filter=%s\r\n", 
           filter_type_name(g_imu_ctx.current_filter_type));
    
    printf("[INFO] KF filter created OK (static, %u bytes)\r\n",
           (unsigned)filter_get_static_size(FILTER_TYPE_KF));

    /* KF 专用参数调优 - 针对 LSM6DSR @ 100Hz
     * 参数来源: kftune 静态扫描 9 组最优 (Set 7)
     *   Q_angle=0.003: 增大角度过程噪声, 加快角度跟踪
     *   Q_bias=0.001:  减小偏置过程噪声, bias 估计更稳定
     *   R_measure=0.03: ACC 测量噪声 (固定)
     *   R_zupt=0.04:   ZUPT 启用 (0.2dps RMS), yaw bias 唯一观测源 */
    if (bsp_lsm6dsr_set_filter_param_ctx(&g_imu_ctx,
            FILTER_PARAM_KF_Q_ANGLE, 0.003f) != 0 ||
        bsp_lsm6dsr_set_filter_param_ctx(&g_imu_ctx,
            FILTER_PARAM_KF_Q_BIAS, 0.001f) != 0 ||
        bsp_lsm6dsr_set_filter_param_ctx(&g_imu_ctx,
            FILTER_PARAM_KF_R_MEASURE, 0.03f) != 0 ||
        bsp_lsm6dsr_set_filter_param_ctx(&g_imu_ctx,
            FILTER_PARAM_KF_R_ZUPT, 0.04f) != 0) {
        printf("[ERROR] KF parameter configuration failed!\r\n");
        bsp_lsm6dsr_destroy_ctx(&g_imu_ctx);
        return -1;
    }

    printf("[INFO] KF params: Q_angle=0.003, Q_bias=0.001, R_measure=0.03, R_zupt=0.04\r\n");
    
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
#if (PRJ_IMU_UART_TELEMETRY_ENABLE != 0U)
static void imu_send_dma(const bsp_lsm6dsr_data_t *data, const bsp_lsm6dsr_ctx_t *ctx)
{
    if (data == NULL || ctx == NULL) return;

    /* 检查 DMA 是否空闲 */
    if (!bsp_uart_tx_idle()) {
        return;  /* 上一次发送未完成，跳过本次 */
    }

    /* 17通道 VOFA+ FireWater 格式:
     * ch0-2:  ax,ay,az (g)
     * ch3-5:  gx,gy,gz (dps, BSP偏置补偿后)
     * ch6-8:  pitch,roll,yaw (deg)
     * ch9:    kf_p00_x (KF X轴角度协方差)
     * ch10:   kf_p00_y (KF Y轴角度协方差)
     * ch11:   kf_p00_z (KF Z轴角度协方差)
     * ch12:   gyro_mag (dps)
     * ch13:   acc_norm_err (g)
     * ch14:   kf_p11_x (KF X轴偏置协方差)
     * ch15:   temp (°C)
     * ch16:   kf_bias_z (KF Z轴偏置估计, dps)
     */
    int len = snprintf(g_dma_buf, IMU_DMA_BUF_SIZE,
        "%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.6f,%.6f,%.6f,%.3f,%.4f,%.6f,%.1f,%.4f\r\n",
        data->ax / PRJ_GRAVITY_MS2,
        data->ay / PRJ_GRAVITY_MS2,
        data->az / PRJ_GRAVITY_MS2,
        data->gx, data->gy, data->gz,
        data->pitch, data->roll, data->yaw,
        (double)ctx->kf_p00_x,
        (double)ctx->kf_p00_y,
        (double)ctx->kf_p00_z,
        (double)ctx->gyro_mag_dps,
        (double)ctx->acc_norm_err,
        (double)ctx->kf_p11_x,
        data->temperature,
        (double)ctx->kf_bias_z
    );

    if (len > 0 && len < IMU_DMA_BUF_SIZE) {
        bsp_uart_send_dma((uint8_t *)g_dma_buf, (uint16_t)len);
    }
}

#endif
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
            
#if (PRJ_IMU_UART_TELEMETRY_ENABLE != 0U)
            /* Optional periodic IMU telemetry; disabled by default on shared UART0. */
            g_print_count++;
            if (g_print_count >= IMU_PRINT_INTERVAL) {
                g_print_count = 0U;
                imu_send_dma(&data, &g_imu_ctx);
            }
#endif

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
        } else {
        }

        osal_task_delay_ms(PRJ_IMU_TASK_PERIOD_MS);
    }
}
