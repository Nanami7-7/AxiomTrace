/**
 * @file    app_main.c
 * @brief   应用层主入口与共享状态管理
 * @note    负责 FreeRTOS 任务创建、BSP 初始化和控制器初始化。
 *          硬件与 RTOS 相关操作分别封装在 BSP 和 OSAL 中。
 *
 *          控制任务：task_control.c
 *          菜单任务：task_menu.c
 *          IMU 任务：task_imu.c
 */

#include "app_main.h"
#include "app_vofa.h"
#include "app_protocol_a.h"
#if (PRJ_UART1_BLE_DEBUG_ENABLE != 0U)
#include "app_uart1_ble_debug.h"
#endif
#include "Task/task_control.h"
#include "Task/task_menu.h"
#include "Task/task_imu.h"
#include "Task/task_key.h"
#include "bsp_key.h"
#include "app_key_events.h"
#include "key_config.h"
#include "osal_api.h"
#include "portable.h"
#include <stdio.h>
#include <string.h>
#include "bsp_led.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "bsp_adc.h"

/* 工厂测试目标不使用红外循迹，避免把循迹模块和浮点依赖带入测试固件。 */
#ifndef PRJ_LINE_TRACK_ENABLE
#define PRJ_LINE_TRACK_ENABLE 1
#endif
#if PRJ_LINE_TRACK_ENABLE
#include "bsp_ir.h"
#include "app_line_track.h"
#endif

#include "bsp_uart.h"
#include "project_config.h"
#include "app_complementary_filter.h"
#include "app_model_id.h"
#include "app_position_control.h"
#include "hal_gpio.h"
#include "ti_msp_dl_config.h"
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

/* ======================== 全局对象 ======================== */

/** 应用层共享上下文，由控制、菜单、IMU 和协议任务共同访问。 */
static app_shared_ctx_t s_shared_ctx;

/** 返回 Board A 协议处理使用的应用共享上下文。 */
app_shared_ctx_t *app_protocol_get_context(void)
{
    return &s_shared_ctx;
}

bool app_state_snapshot_read(const app_shared_ctx_t *ctx,
                             app_state_snapshot_t *snapshot)
{
    if (ctx == NULL || snapshot == NULL) {
        return false;
    }

    OSAL_CRITICAL_SECTION {
        (void)memcpy(&snapshot->control, &ctx->status,
                     sizeof(snapshot->control));
        (void)memcpy(&snapshot->imu, &ctx->imu,
                     sizeof(snapshot->imu));
        for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
            snapshot->motor[i].enabled = ctx->motor_enabled[i];
            snapshot->motor[i].rpm = ctx->status.rpm[i];
            snapshot->motor[i].output = ctx->status.output[i];
            snapshot->motor[i].target = ctx->pid[i].setpoint;
            snapshot->motor[i].kp = ctx->pid[i].kp;
            snapshot->motor[i].ki = ctx->pid[i].ki;
            snapshot->motor[i].kd = ctx->pid[i].kd;
            snapshot->motor[i].ff_enabled = ctx->ff[i].enabled;
            snapshot->motor[i].ff_k = ctx->ff[i].k;
            snapshot->motor[i].ff_b = ctx->ff[i].b;
        }
        snapshot->mode = ctx->posctrl.mode;
    }

    return true;
}


/** 编码器硬件配置表。 */
static const bsp_encoder_config_t s_encoder_cfg[BSP_ENCODER_COUNT] =
    PRJ_ENCODER_CONFIGS;

/** 控制任务句柄。 */
static osal_task_handle_t s_control_task_handle;

/** 菜单任务句柄。 */
static osal_task_handle_t s_menu_task_handle;

/** IMU 任务句柄。 */
static osal_task_handle_t s_imu_task_handle;

#if (PRJ_KEY_ENABLE != 0U)
static osal_task_handle_t s_key_task_handle;
static bsp_key_manager_t s_key_manager;
static bsp_key_instance_t s_key_instances[PRJ_KEY_COUNT];
static const bsp_key_config_t s_key_configs[PRJ_KEY_COUNT] = {
    PRJ_KEY_CONFIGS
};
static bool s_key_motion_ready;
#endif
/** 首个 FreeRTOS 运行时故障码，后续故障不会覆盖。 */
static volatile uint32_t s_runtime_fault_code = APP_RUNTIME_FAULT_NONE;

/* ======================== 内部初始化与诊断 ======================== */

/**
 * @brief 读取 FreeRTOS 运行时资源与故障诊断信息。
 */
bool app_runtime_diag_read(app_runtime_diag_t *out)
{
    if (out == NULL) {
        return false;
    }

    out->control_stack_high_watermark_words = 0U;
    out->menu_stack_high_watermark_words = 0U;
    out->imu_stack_high_watermark_words = 0U;

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
    if (s_control_task_handle != NULL) {
        out->control_stack_high_watermark_words =
            (uint32_t)uxTaskGetStackHighWaterMark(s_control_task_handle);
    }
    if (s_menu_task_handle != NULL) {
        out->menu_stack_high_watermark_words =
            (uint32_t)uxTaskGetStackHighWaterMark(s_menu_task_handle);
    }
    if (s_imu_task_handle != NULL) {
        out->imu_stack_high_watermark_words =
            (uint32_t)uxTaskGetStackHighWaterMark(s_imu_task_handle);
    }
#endif

#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
    out->free_heap_bytes = (uint32_t)xPortGetFreeHeapSize();
    out->minimum_ever_free_heap_bytes =
        (uint32_t)xPortGetMinimumEverFreeHeapSize();
#else
    out->free_heap_bytes = 0U;
    out->minimum_ever_free_heap_bytes = 0U;
#endif

    out->fault_code = s_runtime_fault_code;
    return true;
}

/**
 * @brief 记录首个 FreeRTOS 运行时故障。
 */
void app_runtime_diag_record_fault(uint32_t fault_code)
{
    if ((fault_code != APP_RUNTIME_FAULT_NONE) &&
        (s_runtime_fault_code == APP_RUNTIME_FAULT_NONE)) {
        s_runtime_fault_code = fault_code;
    }
}

/**
 * @brief  初始化各 BSP 模块。
 * @retval 0 成功，负值表示对应模块初始化失败。
 */
static int32_t bsp_modules_init(void)
{
    bsp_status_t ret;

    ret = bsp_led_init();
    if (ret != BSP_OK) { return -1; }

    ret = bsp_uart_init();
    if (ret != BSP_OK) { return -2; }

#if PRJ_LINE_TRACK_ENABLE
    /* 初始化四路红外输入；当前只读取和计算，不连接电机输出。 */
    BSP_IR_Init();
#endif

    ret = bsp_motor_init();
    if (ret != BSP_OK) { return -3; }

    ret = bsp_encoder_init(s_encoder_cfg, BSP_ENCODER_COUNT,
        PRJ_ENCODER_PULSES_PER_REV);
    if (ret != BSP_OK) { return -4; }

    ret = bsp_adc_init();
    if (ret != BSP_OK) { return -5; }

    /* Enable the end-of-sequence interrupt for the fifth ADC MEM. */
    DL_ADC12_enableInterrupt(ADC_VOLTAGE_INST,
        DL_ADC12_INTERRUPT_MEM4_RESULT_LOADED);
    NVIC_EnableIRQ(ADC_VOLTAGE_INST_INT_IRQN);

    /* LSM6DSR 由 task_imu.c 初始化，避免在此重复配置传感器和定时资源。 */
    /* IMU 任务启动后自行完成传感器初始化。 */

    return 0;
}

/**
 * @brief  初始化各电机的速度环 PID 和前馈参数。
 */
static void pid_controllers_init(void)
{
    float duty_max = (float)bsp_motor_get_command_max();

    for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
        app_pid_init(&s_shared_ctx.pid[i],
            PRJ_PID_DEFAULT_KP,
            PRJ_PID_DEFAULT_KI,
            PRJ_PID_DEFAULT_KD,
            APP_PID_MODE_INCREMENT,
            -duty_max,
             duty_max);

        /* 前馈模式下使用的 PID 修正参数。 */
        s_shared_ctx.pid[i].ff_kp = PRJ_FF_PID_DEFAULT_KP;
        s_shared_ctx.pid[i].ff_ki = PRJ_FF_PID_DEFAULT_KI;
        s_shared_ctx.pid[i].ff_kd = PRJ_FF_PID_DEFAULT_KD;
        s_shared_ctx.pid[i].ff_integral_min = -duty_max;
        s_shared_ctx.pid[i].ff_integral_max =  duty_max;
        s_shared_ctx.pid[i].use_ff = false;

        app_ff_init(&s_shared_ctx.ff[i]);

        /*
         * 增量式 PID 的 integral 字段保存上一周期输出。
         * app_pid_init() 已统一设置积分状态和输出上下限，
         * 此处不再重复覆盖，避免初始化规则分散。
         * 后续修改限幅时只需维护 app_pid_init()。
         */
    }
}

/**
 * @brief  初始化位置与航向串级控制器。
 * @note   参数来自 project_config.h 中的 PRJ_POS、PRJ_YAW 和 PRJ_PLANNER 宏。
 *         默认保持 SPEED 模式，等待业务层显式设置目标。
 */
static void posctrl_init(void)
{
    app_posctrl_init(&s_shared_ctx.posctrl,
        PRJ_POS_PID_KP, PRJ_POS_PID_KI, PRJ_POS_PID_KD,
        PRJ_YAW_PID_KP, PRJ_YAW_PID_KI, PRJ_YAW_PID_KD,
        PRJ_PLANNER_ACCEL, PRJ_PLANNER_MAX_RPM,
        PRJ_ENCODER_PULSES_PER_REV);

    /* 配置到位判定条件。 */
    s_shared_ctx.posctrl.reached_threshold =
        PRJ_REACHED_THRESHOLD_POS;  /* 允许的位置误差阈值。 */
    s_shared_ctx.posctrl.reached_threshold_count =
        PRJ_REACHED_COUNT;          /* 连续满足阈值的控制周期数。 */
}

/* ======================== 电机停止接口 ======================== */

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
    /* 初始化 BSP 模块。 */
    int32_t err = bsp_modules_init();
    if (err != 0) {
        return err;
    }

#if PRJ_LINE_TRACK_ENABLE
    /* 初始化循迹算法状态；业务任务需要时调用 app_line_track_update()。 */
    app_line_track_init();
#endif

    /* 初始化四路电机速度环 PID 与前馈参数。 */
    /* IMU 数据由独立任务更新，控制任务只读取共享快照。 */
    /* PID 初始化完成前保持所有电机关闭。 */
    pid_controllers_init();

    /* 初始化互补滤波器。 */
    app_cf_init(NULL);  /* 使用 project_config.h 中的默认参数。 */

    /* 初始化电机模型辨识模块。 */
    app_id_init();

    /* 初始化位置/航向串级控制器，默认保持 SPEED 模式。 */
    posctrl_init();

    /* 上电默认关闭全部电机输出。 */
    for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
        s_shared_ctx.motor_enabled[i] = false;
        s_shared_ctx.overload_cnt[i] = 0U;
    }

#if (PRJ_KEY_ENABLE != 0U)
    s_key_motion_ready = app_key_motion_init(&s_shared_ctx);
    if (!s_key_motion_ready) {
        (void)printf("[KEY] motion init failed; key actions disabled\r\n");
    }

    if (bsp_key_manager_init(
            &s_key_manager,
            s_key_instances,
            s_key_configs,
            PRJ_KEY_COUNT,
            osal_ticks_to_ms(osal_get_tick_count())) != KEY_STATUS_OK) {
        s_key_manager.initialized = false;
        (void)printf("[KEY] manager init failed; key task disabled\r\n");
    } else {
        (void)printf("[KEY] initialized: KEY_key=PA7 active-low\r\n");
        (void)printf("[KEY] initialized: KEY_switch=PB3 active-low\r\n");
        (void)printf("[MOTION] state=IDLE\r\n");
    }
#endif

    /* 打印启动信息。 */
    AX_LOG_INFO("=== MSPM0G3507 FreeRTOS ===");
    (void)printf("\r\n");
    (void)printf("============================================================\r\n");
    (void)printf("  MSPM0G3507 4-Motor Controller v%s (FreeRTOS)\r\n",
        PRJ_VERSION_STRING);
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
        (double)PRJ_PID_DEFAULT_KP,
        (double)PRJ_PID_DEFAULT_KI,
        (double)PRJ_PID_DEFAULT_KD);
    (void)printf("  Control : %lu ms | Menu: %lu ms\r\n",
        (unsigned long)PRJ_CONTROL_PERIOD_MS,
        (unsigned long)PRJ_MENU_POLL_PERIOD_MS);
    (void)printf("  Protocol: v%lu, FireWater %lu channels @ %lu ms\r\n",
        (unsigned long)PRJ_PROTOCOL_VERSION,
        (unsigned long)VOFA_TELEMETRY_CHANNEL_COUNT,
        (unsigned long)PRJ_RPM_OUTPUT_PERIOD_MS);
    (void)printf("  Debug   : AxiomTrace AX_LOG (DEV profile)\r\n");
    (void)printf("  IMU     : LSM6DSR 6-axis @ 104Hz (Hardware SPI)\r\n");
    (void)printf("  Filter  : Complementary/Madgwick/EKF/Mahony/LKF/LPF\r\n");
    (void)printf("  Fusion  : Complementary filter (alpha=%.2f)\r\n",
        (double)PRJ_CF_ALPHA);
    (void)printf("============================================================\r\n");
    (void)printf("\r\n");

    /* 创建控制任务。 */
    s_control_task_handle = osal_task_create(
        app_control_task,
        "ctrl",
        PRJ_TASK_STACK_CONTROL,
        &s_shared_ctx,
        PRJ_TASK_PRIORITY_CONTROL);

    if (s_control_task_handle == NULL) {
        return -10;
    }

    /* 创建菜单任务。 */
    s_menu_task_handle = osal_task_create(
        app_menu_task,
        "menu",
        PRJ_TASK_STACK_MENU,
        &s_shared_ctx,
        PRJ_TASK_PRIORITY_MENU);

    if (s_menu_task_handle == NULL) {
        return -11;
    }

    /* 创建 IMU 任务。 */
    s_imu_task_handle = osal_task_create(
        app_imu_task,
        "imu",
        PRJ_TASK_STACK_IMU,
        &s_shared_ctx,
        PRJ_TASK_PRIORITY_IMU);

    if (s_imu_task_handle == NULL) {
        return -12;
    }

#if (PRJ_KEY_ENABLE != 0U)
    if (s_key_motion_ready && s_key_manager.initialized) {
        s_key_task_handle = osal_task_create(
            app_key_task,
            "key",
            TASK_STACK_KEY,
            &s_key_manager,
            TASK_PRIO_KEY);

        if (s_key_task_handle == NULL) {
            (void)printf("[KEY] task create failed; key actions disabled\r\n");
        }
    }
#endif

    /* 启动 UART1 调试或板间协议任务，二者由编译宏互斥选择。 */
    /*
     * UART1 发送由低优先级任务完成，不在控制任务中格式化或阻塞发送。
     */
    {
#if (PRJ_UART1_BLE_DEBUG_ENABLE != 0U)
        /*
         * 先打印链路诊断，便于确认烧录的确实是当前 BLE 调试固件。
         * 注意：UART1 目前只是透明串口，MSPM0 无法凭空知道外部 BLE 模块的 MAC。
         * 真实 MAC 需要根据模块型号进入 AT 模式查询，后续再接入具体驱动。
         */
        (void)printf("[BLE-DIAG] UART1 BLE 调试已编译启用，波特率=%lu 8N1\r\n",
                     (unsigned long)PRJ_UART1_BLE_DEBUG_UART_BAUDRATE);
        (void)printf("[BLE-DIAG] 工作模式=透明传输，当前 MAC=<未查询>\r\n");
        (void)printf("[BLE-DIAG] 原因=未配置 BLE 模块型号和 AT 查询协议\r\n");

        int32_t debug_ret = app_uart1_ble_debug_init();
        if (debug_ret != 0) {
            (void)printf("[UART1-BLE] 调试模式初始化失败: %ld\r\n",
                         (long)debug_ret);
        } else {
            (void)printf("[UART1-BLE] 调试模式已启用，UART1=%lu 8N1\r\n",
                         (unsigned long)PRJ_UART1_BLE_DEBUG_UART_BAUDRATE);
            (void)printf("[BLE-DIAG] UART1 透明调试任务已创建；手机连接后可发送 HELP\r\n");
        }
#else
        int32_t protocol_ret = app_protocol_a_init();
        if (protocol_ret != 0) {
            (void)printf("[PROTO-A] UART1板间协议初始化失败: %ld\r\n",
                         (long)protocol_ret);
        } else {
            (void)printf("[PROTO-A] UART1板间通信任务已就绪\r\n");
        }
#endif
    }

    /*
    {
        osal_task_handle_t test_handle = osal_task_create(
            test_spi_gyro_task,
            "test",
            PRJ_TASK_STACK_IMU,
            NULL,
            PRJ_TASK_PRIORITY_IMU);
        if (test_handle == NULL) {
            return -12;
        }
    }
    */

    return 0;
}
