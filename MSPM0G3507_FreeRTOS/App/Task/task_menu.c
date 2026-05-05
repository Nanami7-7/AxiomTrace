/**
 * @file    task_menu.c
 * @brief   菜单任务实现
 * @note    状态机驱动串口交互菜单
 *          - MAIN: 显示电机参数+菜单选项
 *          - MOTOR_SELECT: 选择目标电机
 *          - SET_RPM: 设定目标RPM
 *          - TUNING_PID: 设定KP/KI/KD
 *          - RUNNING: 启动电机+10ms输出RPM
 */
#include "task_menu.h"
#include "app_main.h"
#include "app_pid.h"
#include "app_vofa.h"
#include "osal_api.h"
#include "bsp_led.h"
#include "bsp_motor.h"
#include "bsp_uart.h"
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
    app_shared_ctx_t *shared;   /**< 共享上下文指针 */
} menu_ctx_t;

/** 状态处理函数类型 */
typedef void (*menu_handler_t)(menu_ctx_t *ctx);

/* ======================== 私有变量 ======================== */

/** 电机名称查找表 */
static const char *s_motor_names[BSP_MOTOR_COUNT] = {
    "A", "B", "C", "D"
};

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
            (double)ctx->shared->pid[i].kp,
            (double)ctx->shared->pid[i].ki,
            (double)ctx->shared->pid[i].kd);
    }

    (void)printf(
        "a:Motor b:RPM c:PID d:Run q:Back\r\n");
}

/**
 * @brief  停止指定电机(刹车+禁用+PID重置)
 */
static void motor_stop_and_disable(menu_ctx_t *ctx, uint32_t motor_idx)
{
    ctx->shared->motor_enabled[motor_idx] = false;
    (void)bsp_motor_stop((bsp_motor_id_t)motor_idx,
        BSP_MOTOR_MODE_BRAKE);
    app_pid_set_setpoint(&ctx->shared->pid[motor_idx], 0.0f);
    app_pid_reset(&ctx->shared->pid[motor_idx]);
}

/* ======================== 私有函数: 菜单状态处理 ======================== */

/**
 * @brief  主菜单状态处理
 * @note   显示菜单, 等待a/b/c/d切换状态
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
            &ctx->shared->pid[ctx->selected_motor],
            (float)ctx->target_rpm);
        ctx->shared->motor_enabled[ctx->selected_motor] = true;
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
 * @note   输入a-d选择电机, q返回主菜单
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
 * @note   输入整数RPM值, q返回主菜单
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
 * @note   输入"KP KI KD"空格分隔, q返回主菜单
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
                &ctx->shared->pid[ctx->selected_motor],
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
 * @brief  运行状态处理(VOFA+ 协议输出 + 远程调参)
 * @note   10ms间隔输出12通道VOFA+数据, 支持下行命令
 *         通道分配(0~11):
 *         0~3: 4电机实际RPM, 4~7: 4电机目标RPM, 8~11: 4电机PID输出
 *         下行命令: Kp=x, Ki=x, Kd=x, Target=x, Run, Stop, StopAll
 */
static void menu_state_running(menu_ctx_t *ctx)
{
    /* ---- 检查下行命令输入 ---- */
    if (menu_read_line(ctx)) {
        /* 尝试解析为 VOFA+ 命令 */
        vofa_cmd_t cmd;
        if (app_vofa_parse_cmd(ctx->line_buf, &cmd)) {
            app_vofa_apply_cmd(&cmd, ctx->shared,
                &ctx->selected_motor);
        } else {
            /* 非VOFA命令, 检查 'q' 退出 */
            if (ctx->line_buf[0] == 'q') {
                motor_stop_and_disable(ctx,
                    ctx->selected_motor);
                (void)printf("Motor %s stopped\r\n",
                    s_motor_names[ctx->selected_motor]);
                ctx->state = MENU_STATE_MAIN;
                ctx->need_print = true;
                menu_line_reset(ctx);
                osal_task_delay_ms(
                    APP_RPM_OUTPUT_PERIOD_MS);
                menu_led_heartbeat(ctx,
                    APP_RPM_OUTPUT_PERIOD_MS);
                return;
            }
        }
        menu_line_reset(ctx);
    }

    /* ---- 构建12通道数据 ---- */
    float channels[12];
    OSAL_CRITICAL_SECTION {
        for (uint32_t i = 0; i < BSP_MOTOR_COUNT; i++) {
            /* ch0~3: 实际RPM */
            channels[i] = (float)ctx->shared->status.rpm[i];
            /* ch4~7: 目标RPM */
            channels[4 + i] =
                ctx->shared->pid[i].setpoint;
            /* ch8~11: PID输出 */
            channels[8 + i] =
                (float)ctx->shared->status.output[i];
        }
    }

    /* ---- 发送 VOFA+ FireWater 格式 ---- */
    app_vofa_send_firewater(channels, 12U);

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

void app_menu_task(void *param)
{
    app_shared_ctx_t *shared = (app_shared_ctx_t *)param;

    /* 初始化菜单上下文 */
    menu_ctx_t ctx;
    (void)memset(&ctx, 0, sizeof(ctx));
    ctx.state = MENU_STATE_MAIN;
    ctx.selected_motor = 0U;
    ctx.target_rpm = 200;
    ctx.need_print = true;
    ctx.shared = shared;

    for (;;) {
        /* 查表调用当前状态的handler */
        if (ctx.state < MENU_STATE_COUNT) {
            s_menu_handlers[ctx.state](&ctx);
        } else {
            ctx.state = MENU_STATE_MAIN;
        }
    }
}