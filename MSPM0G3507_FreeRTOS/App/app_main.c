/**
* @file    app_main.c
* @brief   应用层主入口实现
* @note    创建FreeRTOS任务, 实现电机PID速度控制
*          硬件/RTOS无关: 仅通过BSP和OSAL接口操作
*
*          控制任务(5ms):
*          1. 读取4路编码器脉冲增量(M法测速)
*          2. 脉冲数换算RPM
*          3. PID计算电机速度
*          4. 设置电机PWM输出
*
*          菜单任务:
*          状态机驱动串口交互菜单
*          - MAIN: 显示电机参数+菜单选项
*          - MOTOR_SELECT: 选择目标电机
*          - SET_RPM: 设定目标RPM
*          - TUNING_PID: 设定KP/KI/KD
*          - RUNNING: 启动电机+10ms输出RPM
*/
#include "app_main.h"
#include "app_pid.h"
#include "osal_api.h"
#include "bsp_led.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "bsp_uart.h"
#include "hal_gpio.h"
#include "project_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================== 私有类型 ======================== */

/** 菜单状态枚举 */
typedef enum {
    MENU_STATE_MAIN = 0,     /**< 主菜单 */
    MENU_STATE_MOTOR_SELECT, /**< 选择电机 */
    MENU_STATE_SET_RPM,      /**< 设定RPM */
    MENU_STATE_TUNING_PID,   /**< 调PID参数 */
    MENU_STATE_RUNNING,      /**< 运行中(输出RPM) */
    MENU_STATE_COUNT         /**< 状态总数 */
} menu_state_t;

/** 菜单上下文结构体 */
typedef struct {
    menu_state_t state;         /**< 当前状态 */
    uint32_t     selected_motor;/**< 选中电机索引(0~3) */
    int32_t      target_rpm;    /**< 目标RPM(用户输入) */
    char         line_buf[MENU_LINE_BUF_SIZE]; /**< 行缓冲 */
    uint32_t     line_pos;      /**< 行缓冲写入位置 */
    uint32_t     led_counter;   /**< LED分频计数器 */
    bool         need_print;    /**< 主菜单需重印标志 */
} menu_ctx_t;

/** 状态处理函数类型 */
typedef void (*menu_handler_t)(menu_ctx_t *ctx);

/* ======================== 私有变量 ======================== */

/** 四路电机PID控制器实例 */
static app_pid_t s_pid[BSP_MOTOR_COUNT];

/** 电机配置(板级) */
static const bsp_motor_config_t s_motor_cfg[BSP_MOTOR_COUNT] =
    PRJ_MOTOR_CONFIGS;

/** 编码器配置(板级) */
static const bsp_encoder_config_t s_encoder_cfg[BSP_ENCODER_COUNT] =
    PRJ_ENCODER_CONFIGS;

/** 控制任务句柄 */
static osal_task_handle_t s_control_task_handle;

/** 菜单任务句柄 */
static osal_task_handle_t s_menu_task_handle;

/** 电机使能标志(菜单任务写, 控制任务读) */
static bool s_motor_enabled[BSP_MOTOR_COUNT];

/** 控制任务共享状态(控制任务写, 菜单任务读) */
typedef struct {
    int32_t rpm[BSP_MOTOR_COUNT];
    int32_t output[BSP_MOTOR_COUNT];
} app_control_status_t;

static app_control_status_t s_control_status;

/** 电机名称查找表 */
static const char *s_motor_names[BSP_MOTOR_COUNT] = {
    "A", "B", "C", "D"
};

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

    ret = bsp_motor_init(s_motor_cfg, BSP_MOTOR_COUNT,
        PRJ_MOTOR_PWM_TIMER, PRJ_MOTOR_PWM_PERIOD);
    if (ret != BSP_OK) { return -3; }

    ret = bsp_encoder_init(s_encoder_cfg, BSP_ENCODER_COUNT,
        PRJ_ENCODER_PULSES_PER_REV);
    if (ret != BSP_OK) { return -4; }

    return 0;
}

/**
* @brief  初始化四路PID控制器
*/
static void pid_controllers_init(void)
{
    float duty_max = (float)bsp_motor_get_duty_max();

    for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
        app_pid_init(&s_pid[i],
            APP_PID_DEFAULT_KP,
            APP_PID_DEFAULT_KI,
            APP_PID_DEFAULT_KD,
            APP_PID_MODE_INCREMENT,
            -duty_max,
             duty_max);

        /* 速度环: 积分限幅为输出限幅的50% */
        app_pid_set_integral_limit(&s_pid[i],
            -duty_max * 0.5f,
             duty_max * 0.5f);
    }
}

/* ======================== 私有函数: 菜单辅助 ======================== */

/**
* @brief  非阻塞行输入
* @note   从UART逐字节读取, 回显, 支持退格
*         遇到\r或\n视为一行完成
* @param  ctx 菜单上下文
* @retval true  一行输入完成(ctx->line_buf含完整行, 以\0结尾)
* @retval false 尚未完成, 继续轮询
*/
static bool menu_read_line(menu_ctx_t *ctx)
{
    uint8_t ch;

    while (bsp_uart_getc(&ch) == BSP_OK) {
        if (ch == '\r' || ch == '\n') {
            /* 行结束: 终止字符串, 回显换行 */
            ctx->line_buf[ctx->line_pos] = '\0';
            (void)printf("\r\n");
            ctx->line_pos = 0U;
            return true;
        } else if (ch == 0x7FU || ch == 0x08U) {
            /* 退格: 删除末字符 */
            if (ctx->line_pos > 0U) {
                ctx->line_pos--;
                (void)printf("\b \b");
            }
        } else if (ch >= 0x20U && ch < 0x7FU) {
            /* 可打印字符: 存入+回显 */
            if (ctx->line_pos < (MENU_LINE_BUF_SIZE - 1U)) {
                ctx->line_buf[ctx->line_pos] = (char)ch;
                ctx->line_pos++;
                (void)bsp_uart_putc(ch);
            }
        }
        /* 其他字符(如0x1B ESC)忽略 */
    }

    return false;
}

/**
* @brief  重置行缓冲区
*/
static void menu_line_reset(menu_ctx_t *ctx)
{
    ctx->line_pos = 0U;
    ctx->line_buf[0] = '\0';
    bsp_uart_rx_flush();
}

/**
* @brief  LED心跳(分频实现)
* @param  ctx     菜单上下文
* @param  period  当前循环延时(ms)
*/
static void menu_led_heartbeat(menu_ctx_t *ctx, uint32_t period)
{
    ctx->led_counter += period;
    if (ctx->led_counter >= 500U) {
        ctx->led_counter = 0U;
        bsp_led_toggle();
    }
}

/**
* @brief  打印主菜单信息(选中电机+所有PID参数+选项)
*/
static void menu_print_main(const menu_ctx_t *ctx)
{
    (void)printf("\r\n=== Motor: %s ===\r\n",
        s_motor_names[ctx->selected_motor]);

    for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
        (void)printf("%c: KP=%.2f KI=%.2f KD=%.2f\r\n",
            'A' + (int)i,
            (double)s_pid[i].kp,
            (double)s_pid[i].ki,
            (double)s_pid[i].kd);
    }

    (void)printf(
        "a:Motor b:RPM c:PID d:Run q:Back\r\n");
}

/**
* @brief  停止指定电机(刹车+禁用+PID重置)
*/
static void motor_stop_and_disable(uint32_t motor_idx)
{
    s_motor_enabled[motor_idx] = false;
    (void)bsp_motor_stop((bsp_motor_id_t)motor_idx,
        BSP_MOTOR_MODE_BRAKE);
    app_pid_set_setpoint(&s_pid[motor_idx], 0.0f);
    app_pid_reset(&s_pid[motor_idx]);
}

/* ======================== 私有函数: 菜单状态处理 ======================== */

/**
* @brief  主菜单状态处理
* @note   显示菜单, 等待1/2/3/4切换状态
*/
static void menu_state_main(menu_ctx_t *ctx)
{
    if (ctx->need_print) {
        menu_print_main(ctx);
        ctx->need_print = false;
    }

    if (!menu_read_line(ctx)) {
        osal_task_delay_ms(APP_MENU_POLL_PERIOD_MS);
        menu_led_heartbeat(ctx, APP_MENU_POLL_PERIOD_MS);
        return;
    }

    char ch = ctx->line_buf[0];

    switch (ch) {
    case 'a':
        ctx->state = MENU_STATE_MOTOR_SELECT;
        (void)printf("Select motor (a-d, q=back):\r\n");
        break;
    case 'b':
        ctx->state = MENU_STATE_SET_RPM;
        (void)printf("Set RPM for Motor %s (q=back):\r\n",
            s_motor_names[ctx->selected_motor]);
        break;
    case 'c':
        ctx->state = MENU_STATE_TUNING_PID;
        (void)printf(
            "Set KP KI KD for Motor %s (e.g. 1.5 0.3 0.1, q=back):\r\n",
            s_motor_names[ctx->selected_motor]);
        break;
    case 'd':
        ctx->state = MENU_STATE_RUNNING;
        /* 设PID目标值(RPM)并使能电机 */
        app_pid_set_setpoint(
            &s_pid[ctx->selected_motor],
            (float)ctx->target_rpm);
        s_motor_enabled[ctx->selected_motor] = true;
        (void)printf("Motor %s running (q=stop):\r\n",
            s_motor_names[ctx->selected_motor]);
        break;
    default:
        break;
    }

    menu_line_reset(ctx);
    osal_task_delay_ms(APP_MENU_POLL_PERIOD_MS);
    menu_led_heartbeat(ctx, APP_MENU_POLL_PERIOD_MS);
}

/**
* @brief  电机选择状态处理
* @note   输入1-4选择电机, 0返回主菜单
*/
static void menu_state_motor_select(menu_ctx_t *ctx)
{
    if (!menu_read_line(ctx)) {
        osal_task_delay_ms(APP_MENU_POLL_PERIOD_MS);
        menu_led_heartbeat(ctx, APP_MENU_POLL_PERIOD_MS);
        return;
    }

    char ch = ctx->line_buf[0];

    if (ch == 'q') {
        ctx->state = MENU_STATE_MAIN;
        ctx->need_print = true;
    } else if (ch >= 'a' && ch <= 'd') {
        ctx->selected_motor = (uint32_t)(ch - 'a');
        (void)printf("Motor %s selected\r\n",
            s_motor_names[ctx->selected_motor]);
        ctx->state = MENU_STATE_MAIN;
        ctx->need_print = true;
    } else {
        (void)printf("Invalid! Enter a-d or q\r\n");
    }

    menu_line_reset(ctx);
    osal_task_delay_ms(APP_MENU_POLL_PERIOD_MS);
    menu_led_heartbeat(ctx, APP_MENU_POLL_PERIOD_MS);
}

/**
* @brief  设定RPM状态处理
* @note   输入整数RPM值, 0返回主菜单
*/
static void menu_state_set_rpm(menu_ctx_t *ctx)
{
    if (!menu_read_line(ctx)) {
        osal_task_delay_ms(APP_MENU_POLL_PERIOD_MS);
        menu_led_heartbeat(ctx, APP_MENU_POLL_PERIOD_MS);
        return;
    }

    char ch = ctx->line_buf[0];

    if (ch == 'q') {
        ctx->state = MENU_STATE_MAIN;
        ctx->need_print = true;
    } else {
        int32_t rpm = atoi(ctx->line_buf);
        if (rpm == 0) {
            (void)printf("Invalid RPM!\r\n");
        } else {
            ctx->target_rpm = rpm;
            (void)printf("Motor %s target: %ld RPM\r\n",
                s_motor_names[ctx->selected_motor],
                (long)rpm);
            ctx->state = MENU_STATE_MAIN;
            ctx->need_print = true;
        }
    }

    menu_line_reset(ctx);
    osal_task_delay_ms(APP_MENU_POLL_PERIOD_MS);
    menu_led_heartbeat(ctx, APP_MENU_POLL_PERIOD_MS);
}

/**
* @brief  调PID参数状态处理
* @note   输入"KP KI KD"空格分隔, 0返回主菜单
*/
static void menu_state_tuning_pid(menu_ctx_t *ctx)
{
    if (!menu_read_line(ctx)) {
        osal_task_delay_ms(APP_MENU_POLL_PERIOD_MS);
        menu_led_heartbeat(ctx, APP_MENU_POLL_PERIOD_MS);
        return;
    }

    char ch = ctx->line_buf[0];

    if (ch == 'q') {
        ctx->state = MENU_STATE_MAIN;
        ctx->need_print = true;
    } else {
        float kp = 0.0f, ki = 0.0f, kd = 0.0f;
        int n = sscanf(ctx->line_buf, "%f %f %f",
            &kp, &ki, &kd);
        if (n == 3) {
            app_pid_set_params(
                &s_pid[ctx->selected_motor],
                kp, ki, kd);
            (void)printf(
                "Motor %s: KP=%.2f KI=%.2f KD=%.2f\r\n",
                s_motor_names[ctx->selected_motor],
                (double)kp, (double)ki, (double)kd);
            ctx->state = MENU_STATE_MAIN;
            ctx->need_print = true;
        } else {
            (void)printf(
                "Invalid! e.g. 1.5 0.3 0.1\r\n");
        }
    }

    menu_line_reset(ctx);
    osal_task_delay_ms(APP_MENU_POLL_PERIOD_MS);
    menu_led_heartbeat(ctx, APP_MENU_POLL_PERIOD_MS);
}

/**
* @brief  运行状态处理
* @note   10ms间隔输出RPM, 输入0停电机返回
*/
static void menu_state_running(menu_ctx_t *ctx)
{
    /* 检查是否有输入 */
    uint8_t ch;
    if (bsp_uart_getc(&ch) == BSP_OK) {
        if (ch == 'q') {
            motor_stop_and_disable(ctx->selected_motor);
            (void)printf("Motor %s stopped\r\n",
                s_motor_names[ctx->selected_motor]);
            ctx->state = MENU_STATE_MAIN;
            ctx->need_print = true;
            menu_line_reset(ctx);
            osal_task_delay_ms(APP_RPM_OUTPUT_PERIOD_MS);
            menu_led_heartbeat(ctx,
                APP_RPM_OUTPUT_PERIOD_MS);
            return;
        }
    }

    /* 读取当前电机RPM并输出 */
    int32_t rpm = 0;
    OSAL_CRITICAL_SECTION {
        rpm = s_control_status.rpm[ctx->selected_motor];
    }
    (void)printf("%ld\r\n", (long)rpm);

    osal_task_delay_ms(APP_RPM_OUTPUT_PERIOD_MS);
    menu_led_heartbeat(ctx, APP_RPM_OUTPUT_PERIOD_MS);
}

/** 状态处理函数表 */
static const menu_handler_t
    s_menu_handlers[MENU_STATE_COUNT] = {
    [MENU_STATE_MAIN]         = menu_state_main,
    [MENU_STATE_MOTOR_SELECT] = menu_state_motor_select,
    [MENU_STATE_SET_RPM]      = menu_state_set_rpm,
    [MENU_STATE_TUNING_PID]   = menu_state_tuning_pid,
    [MENU_STATE_RUNNING]      = menu_state_running,
};

/* ======================== 公共函数实现 ======================== */

int32_t app_main_init(void)
{
    /* 初始化BSP模块 */
    int32_t err = bsp_modules_init();
    if (err != 0) {
        return err;
    }

    /* 初始化PID控制器 */
    pid_controllers_init();

    /* 串口输出启动信息 */
    (void)printf("\r\n=== MSPM0G3507 FreeRTOS ===\r\n");

    /* 创建控制任务 */
    s_control_task_handle = osal_task_create(
        app_control_task,
        "ctrl",
        APP_TASK_STACK_CONTROL,
        NULL,
        APP_TASK_PRIORITY_CONTROL);

    if (s_control_task_handle == NULL) {
        return -10;
    }

    /* 创建菜单任务 */
    s_menu_task_handle = osal_task_create(
        app_menu_task,
        "menu",
        APP_TASK_STACK_MENU,
        NULL,
        APP_TASK_PRIORITY_MENU);

    if (s_menu_task_handle == NULL) {
        return -11;
    }

    return 0;
}

void app_control_task(void *param)
{
    (void)param;

    const float dt_s = (float)APP_CONTROL_PERIOD_MS / 1000.0f;

    for (;;) {
        /*
         * M法测速: 读取并清零脉冲计数, 换算RPM
         * 在APP_CONTROL_PERIOD_MS窗口内计数编码器脉冲
         */
        int32_t deltas[BSP_ENCODER_COUNT];
        (void)bsp_encoder_get_and_clear_all(deltas);

        int32_t rpm_local[BSP_ENCODER_COUNT];
        for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
            rpm_local[i] = bsp_encoder_counts_to_rpm(
                deltas[i], APP_CONTROL_PERIOD_MS);
        }

        OSAL_CRITICAL_SECTION {
            for (uint32_t i = 0; i < BSP_ENCODER_COUNT; i++) {
                s_control_status.rpm[i] = rpm_local[i];
            }
        }

        /* PID计算 + 电机输出(仅使能电机) */
        for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
            if (!s_motor_enabled[i]) {
                continue;
            }

            float feedback = (float)rpm_local[i];
            float output = app_pid_compute(
                &s_pid[i], feedback, dt_s);
            (void)bsp_motor_set_speed(
                (bsp_motor_id_t)i, (int32_t)output);
        }
        /* 周期延时 */
        osal_task_delay_ms(APP_CONTROL_PERIOD_MS);
    }
}

void app_menu_task(void *param)
{
    (void)param;

    /* 初始化菜单上下文 */
    menu_ctx_t ctx;
    (void)memset(&ctx, 0, sizeof(ctx));
    ctx.state = MENU_STATE_MAIN;
    ctx.selected_motor = 0U;
    ctx.target_rpm = 200;
    ctx.need_print = true;

    for (;;) {
        /* 查表调用当前状态的handler */
        if (ctx.state < MENU_STATE_COUNT) {
            s_menu_handlers[ctx.state](&ctx);
        } else {
            ctx.state = MENU_STATE_MAIN;
        }
    }
}
