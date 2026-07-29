/**
 * @file    app_main.c
 * @brief   ????????????
 * @note    ???FreeRTOS???, ??????PID??????
 *          ???/RTOS???: ?????BSP??SAL??????
 *
 *          ??????(5ms):  task_control.c
 *          ??????:       task_menu.c
 *          IMU???:        task_imu.c
 */

#include "app_main.h"
#include "app_vofa.h"
#include "app_protocol_a.h"
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

/* ======================== ?????? ======================== */

/** ???????????????????????????? */
static app_shared_ctx_t s_shared_ctx;

/** ???????????????????????????????????????????*/
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


/** ???????????) */
static const bsp_encoder_config_t s_encoder_cfg[BSP_ENCODER_COUNT] =
    PRJ_ENCODER_CONFIGS;

/** ????????? */
static osal_task_handle_t s_control_task_handle;

/** ????????? */
static osal_task_handle_t s_menu_task_handle;

/** IMU?????? */
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
/** ??????????????????????????????????????*/
static volatile uint32_t s_runtime_fault_code = APP_RUNTIME_FAULT_NONE;

/* ======================== ??????: BSP?????======================== */

/**
 * @brief ??? FreeRTOS ?????????????
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
 * @brief ??????????????FreeRTOS ??????????
 */
void app_runtime_diag_record_fault(uint32_t fault_code)
{
    if ((fault_code != APP_RUNTIME_FAULT_NONE) &&
        (s_runtime_fault_code == APP_RUNTIME_FAULT_NONE)) {
        s_runtime_fault_code = fault_code;
    }
}

/**
 * @brief  ????????SP???
 * @retval 0 ???, ?? ???
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

    /* LSM6DSR ?????? task_imu.c ?????(????TimerG8 ????? */
    /* ?????????????? IMU */

    return 0;
}

/**
 * @brief  ????????ID?????
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

        /* FF???PID?????? */
        s_shared_ctx.pid[i].ff_kp = PRJ_FF_PID_DEFAULT_KP;
        s_shared_ctx.pid[i].ff_ki = PRJ_FF_PID_DEFAULT_KI;
        s_shared_ctx.pid[i].ff_kd = PRJ_FF_PID_DEFAULT_KD;
        s_shared_ctx.pid[i].ff_integral_min = -duty_max;
        s_shared_ctx.pid[i].ff_integral_max =  duty_max;
        s_shared_ctx.pid[i].use_ff = false;

        app_ff_init(&s_shared_ctx.ff[i]);

        /*
         * ?????ID??integral ?????????"???????????,
         * ????????????????? ?????????????????
         * app_pid_init ??? integral_min/max ??? out_min/max,
         * ????????????.
         */
    }
}

/**
 * @brief  ???????????????????
 * @note   ??? project_config.h ??? PRJ_POS / PRJ_YAW / PRJ_PLANNER ??????
 *         ??? SPEED ???, ????????????????
 */
static void posctrl_init(void)
{
    app_posctrl_init(&s_shared_ctx.posctrl,
        PRJ_POS_PID_KP, PRJ_POS_PID_KI, PRJ_POS_PID_KD,
        PRJ_YAW_PID_KP, PRJ_YAW_PID_KI, PRJ_YAW_PID_KD,
        PRJ_PLANNER_ACCEL, PRJ_PLANNER_MAX_RPM,
        PRJ_ENCODER_PULSES_PER_REV);

    /* ??????????*/
    s_shared_ctx.posctrl.reached_threshold =
        PRJ_REACHED_THRESHOLD_POS;  /* ?????????????) */
    s_shared_ctx.posctrl.reached_threshold_count =
        PRJ_REACHED_COUNT;          /* 200ms??? */
}

/* ======================== ????????? ======================== */

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
    /* ?????SP??? */
    int32_t err = bsp_modules_init();
    if (err != 0) {
        return err;
    }

#if PRJ_LINE_TRACK_ENABLE
    /* 初始化循迹算法状态；业务任务需要时调用 app_line_track_update()。 */
    app_line_track_init();
#endif

    /* ?????ID?????*/
    /* ?????IMU ???????????????????????????????*/
    /* ?????ID?????*/
    pid_controllers_init();

    /* ???????????? */
    app_cf_init(NULL);  /* ???project_config.h????????? */

    /* ?????????????????*/
    app_id_init();

    /* ??????????????????????SPEED???) */
    posctrl_init();

    /* ???????????*/
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

    /* ???????????? */
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

    /* ????????? */
    s_control_task_handle = osal_task_create(
        app_control_task,
        "ctrl",
        PRJ_TASK_STACK_CONTROL,
        &s_shared_ctx,
        PRJ_TASK_PRIORITY_CONTROL);

    if (s_control_task_handle == NULL) {
        return -10;
    }

    /* ????????? */
    s_menu_task_handle = osal_task_create(
        app_menu_task,
        "menu",
        PRJ_TASK_STACK_MENU,
        &s_shared_ctx,
        PRJ_TASK_PRIORITY_MENU);

    if (s_menu_task_handle == NULL) {
        return -11;
    }

    /* ???IMU??? */
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

    /* ??? SPI + ??????????? (??????) */
    /*
     * UART1 board-link protocol is optional. Core tasks must be created first;
     * a protocol-task failure must not suppress UART0 diagnostics or control.
     */
    {
        int32_t protocol_ret = app_protocol_a_init();
        if (protocol_ret != 0) {
            (void)printf("[PROTO-A] optional UART1 init failed: %ld\r\n",
                         (long)protocol_ret);
        } else {
            (void)printf("[PROTO-A] UART1 link task ready\r\n");
        }
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
