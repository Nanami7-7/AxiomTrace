/**
 * @file    app_main.c
 * @brief   应用层主入口实现
 * @note    创建FreeRTOS任务, 实现电机PID速度控制
 *          硬件/RTOS无关: 仅通过BSP和OSAL接口操作
 *
 *          控制任务(5ms):  task_control.c
 *          菜单任务:       task_menu.c
 *          IMU任务:        task_imu.c
 */

#include "app_main.h"
#include "project_version.h"
#include "app_vofa.h"
#include "Task/task_control.h"
#include "Task/task_menu.h"
#include "Task/task_imu.h"
#include "osal_api.h"
#include <stdio.h>
#include "bsp_led.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "bsp_uart.h"
#include "app_complementary_filter.h"
#include "app_model_id.h"
#include "app_position_control.h"
#include "hal_gpio.h"
#include "project_config.h"
#include "axiomtrace.h"
#include "app_pid.h"

#if (ID_PWM_MAX != PRJ_MOTOR_COMMAND_MAX) || \
    ((-ID_PWM_MIN) != PRJ_MOTOR_COMMAND_MAX)
#error "Model-ID command range must match the selected motor backend command range"
#endif
#if (ID_DEFAULT_PWM_STEP > ID_PWM_MAX) || \
    (ID_DEFAULT_PWM_STEP < ID_MIN_PWM)
#error "Model-ID default step must lie inside the unified motor command range"
#endif

/* ======================== 私有变量 ======================== */

/** 共享上下文(控制任务和菜单任务之间共享) */
static app_shared_ctx_t s_shared_ctx;

/** 编码器配置(板级) */
static const bsp_encoder_config_t s_encoder_cfg[BSP_ENCODER_COUNT] =
    PRJ_ENCODER_CONFIGS;

/** 控制任务句柄 */
static osal_task_handle_t s_control_task_handle;

/** 菜单任务句柄 */
static osal_task_handle_t s_menu_task_handle;

/** IMU任务句柄 */
static osal_task_handle_t s_imu_task_handle;

/* ======================== 私有函数: BSP初始化 ======================== */

/**
 * @brief  初始化所有BSP模块
 * @retval 0 成功, 非0 失败
 */
static int32_t bsp_modules_init(void)
{
    bsp_status_t ret;

    ret = bsp_led_init();
    if (ret != BSP_OK) { return -1; }

    ret = bsp_uart_init();
    if (ret != BSP_OK) { return -2; }

    ret = bsp_motor_init();
    if (ret != BSP_OK) { return -3; }

    ret = bsp_encoder_init(s_encoder_cfg, BSP_ENCODER_COUNT,
        PRJ_ENCODER_PULSES_PER_REV);
    if (ret != BSP_OK) { return -4; }

    /* LSM6DSR 初始化在 task_imu.c 中完成 (需要 TimerG8 先启动) */
    /* 此处不再需要初始化 IMU */

    return 0;
}

/**
 * @brief  初始化四路PID控制器
 */
static void pid_controllers_init(void)
{
    float duty_max = (float)bsp_motor_get_command_max();

    for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
        app_pid_init(&s_shared_ctx.pid[i],
            APP_PID_DEFAULT_KP,
            APP_PID_DEFAULT_KI,
            APP_PID_DEFAULT_KD,
            APP_PID_MODE_INCREMENT,
            -duty_max,
             duty_max);

        /* FF模式PID默认参数 */
        s_shared_ctx.pid[i].ff_kp = APP_FF_PID_DEFAULT_KP;
        s_shared_ctx.pid[i].ff_ki = APP_FF_PID_DEFAULT_KI;
        s_shared_ctx.pid[i].ff_kd = APP_FF_PID_DEFAULT_KD;
        s_shared_ctx.pid[i].ff_integral_min = -duty_max;
        s_shared_ctx.pid[i].ff_integral_max =  duty_max;
        s_shared_ctx.pid[i].use_ff = false;

        app_ff_init(&s_shared_ctx.ff[i]);

        /*
         * 增量式PID中 integral 变量实际存储"上次输出累加值",
         * 其限幅应与输出限幅一致, 否则会限制最大输出幅度.
         * app_pid_init 已将 integral_min/max 设为 out_min/max,
         * 此处不再额外缩小.
         */
    }
}

/**
 * @brief  初始化位置-速度串级控制器
 * @note   使用 project_config.h 中的 APP_POS / APP_YAW / APP_PLANNER 系列参数
 *         默认 SPEED 模式, 不影响现有速度环行为
 */
static void posctrl_init(void)
{
    app_posctrl_init(&s_shared_ctx.posctrl,
        APP_POS_PID_KP, APP_POS_PID_KI, APP_POS_PID_KD,
        APP_YAW_PID_KP, APP_YAW_PID_KI, APP_YAW_PID_KD,
        APP_PLANNER_ACCEL, APP_PLANNER_MAX_RPM,
        PRJ_ENCODER_PULSES_PER_REV);

    /* 到位判定阈值 */
    s_shared_ctx.posctrl.reached_threshold =
        APP_REACHED_THRESHOLD_POS;  /* 位置模式阈值(脉冲) */
    s_shared_ctx.posctrl.reached_threshold_count =
        APP_REACHED_COUNT;          /* 200ms持续 */
}

/* ======================== 公共函数实现 ======================== */

void app_motor_stop(app_shared_ctx_t *ctx, uint32_t motor_idx)
{
    if (ctx == NULL || motor_idx >= BSP_MOTOR_COUNT) {
        return;
    }
    OSAL_CRITICAL_SECTION {
        ctx->motor_enabled[motor_idx] = false;
        app_pid_reset(&ctx->pid[motor_idx]);
    }
    (void)bsp_motor_stop((bsp_motor_id_t)motor_idx,
        BSP_MOTOR_MODE_BRAKE);
}

void app_motor_stop_all(app_shared_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
            ctx->motor_enabled[i] = false;
            app_pid_reset(&ctx->pid[i]);
        }
    }
    bsp_motor_stop_all();
}

int32_t app_main_init(void)
{
    /* 初始化BSP模块 */
    int32_t err = bsp_modules_init();
    if (err != 0) {
        return err;
    }

    /* 初始化PID控制器 */
    pid_controllers_init();

    /* 初始化互补滤波器 */
    app_cf_init(NULL);  /* 使用project_config.h中的默认参数 */

    /* 初始化模型参数辨识模块 */
    app_id_init();

    /* 初始化位置-速度串级控制器(默认SPEED模式) */
    posctrl_init();

    /* 清零共享上下文 */
    for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
        s_shared_ctx.motor_enabled[i] = false;
    }

    /* 串口输出启动信息 */
    AX_LOG_INFO("=== MSPM0G3507 FreeRTOS ===");
    (void)printf("\r\n");
    (void)printf("============================================================\r\n");
    (void)printf("  MSPM0G3507 4-Motor Controller v%s (FreeRTOS)\r\n",
        PROJECT_VERSION_STRING);
    (void)printf("============================================================\r\n");
    (void)printf("  Motors  : %lu (A/B/C/D)\r\n",
        (unsigned long)BSP_MOTOR_COUNT);
    (void)printf("  Encoder : %lu counts/output-rev (%lu PPR x%lu, gear %lu/%lu)\r\n",
        (unsigned long)PRJ_ENCODER_PULSES_PER_REV,
        (unsigned long)PRJ_MOTOR_ENCODER_PPR,
        (unsigned long)PRJ_ENCODER_DECODE_MULTIPLIER,
        (unsigned long)PRJ_MOTOR_GEAR_RATIO_NUMERATOR,
        (unsigned long)PRJ_MOTOR_GEAR_RATIO_DENOMINATOR);
    (void)printf("  Driver  : %s | command -%lu..+%lu\r\n",
        bsp_motor_get_driver_name(),
        (unsigned long)bsp_motor_get_command_max(),
        (unsigned long)bsp_motor_get_command_max());
#if (PRJ_MOTOR_DRIVER == PRJ_MOTOR_DRIVER_DRV8870)
    (void)printf("  DRV8870 : reverse < %lu%% | deadband %lu%%..%lu%% | forward > %lu%%\r\n",
        (unsigned long)PRJ_DRV8870_DEADBAND_LOW_PERCENT,
        (unsigned long)PRJ_DRV8870_DEADBAND_LOW_PERCENT,
        (unsigned long)PRJ_DRV8870_DEADBAND_HIGH_PERCENT,
        (unsigned long)PRJ_DRV8870_DEADBAND_HIGH_PERCENT);
#else
    (void)printf("  TB6612  : direction GPIO + active-high PWM | true coast/brake\r\n");
#endif
    (void)printf("  PID     : Kp=%.2f Ki=%.2f Kd=%.2f (Increment)\r\n",
        (double)APP_PID_DEFAULT_KP,
        (double)APP_PID_DEFAULT_KI,
        (double)APP_PID_DEFAULT_KD);
    (void)printf("  Control : %lu ms | Menu: %lu ms\r\n",
        (unsigned long)APP_CONTROL_PERIOD_MS,
        (unsigned long)APP_MENU_POLL_PERIOD_MS);
    (void)printf("  Protocol: v%lu, FireWater %lu channels @ %lu ms\r\n",
        (unsigned long)PROJECT_PROTOCOL_VERSION,
        (unsigned long)VOFA_TELEMETRY_CHANNEL_COUNT,
        (unsigned long)APP_RPM_OUTPUT_PERIOD_MS);
    (void)printf("  Debug   : AxiomTrace AX_LOG (DEV profile)\r\n");
    (void)printf("  IMU     : LSM6DSR 6-axis @ 104Hz (Hardware SPI)\r\n");
    (void)printf("  Filter  : Complementary/Madgwick/EKF/Mahony/LKF/LPF\r\n");
    (void)printf("  Fusion  : Complementary filter (alpha=%.2f)\r\n",
        (double)PRJ_CF_ALPHA);
    (void)printf("============================================================\r\n");
    (void)printf("\r\n");

    /* 创建控制任务 */
    s_control_task_handle = osal_task_create(
        app_control_task,
        "ctrl",
        APP_TASK_STACK_CONTROL,
        &s_shared_ctx,
        APP_TASK_PRIORITY_CONTROL);

    if (s_control_task_handle == NULL) {
        return -10;
    }

    /* 创建菜单任务 */
    s_menu_task_handle = osal_task_create(
        app_menu_task,
        "menu",
        APP_TASK_STACK_MENU,
        &s_shared_ctx,
        APP_TASK_PRIORITY_MENU);

    if (s_menu_task_handle == NULL) {
        return -11;
    }

    /* 创建IMU任务 */
    s_imu_task_handle = osal_task_create(
        app_imu_task,
        "imu",
        APP_TASK_STACK_IMU,
        &s_shared_ctx,
        APP_TASK_PRIORITY_IMU);

    if (s_imu_task_handle == NULL) {
        return -12;
    }

    /* 创建 SPI + 陀螺仪测试任务 (临时注释) */
    /*
    {
        osal_task_handle_t test_handle = osal_task_create(
            test_spi_gyro_task,
            "test",
            APP_TASK_STACK_IMU,
            NULL,
            APP_TASK_PRIORITY_IMU);
        if (test_handle == NULL) {
            return -12;
        }
    }
    */

    return 0;
}