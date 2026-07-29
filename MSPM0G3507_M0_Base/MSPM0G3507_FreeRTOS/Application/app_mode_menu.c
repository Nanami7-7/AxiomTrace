/**
 * @file    app_mode_menu.c
 * @brief   PA13 单键十模式菜单。
 *
 * 按键行为固定为：
 *   - 菜单列表中短按：切换到下一个 CUST 模式；
 *   - 菜单列表中长按：进入当前选中的 CUST 模式；
 *   - 模式运行中短按：调用当前模式的 app_mode_userXX_short()；
 *   - 模式运行中长按：返回模式列表。
 *
 * 二次开发时只需要修改本文件底部的 30 个公开函数体：
 *   app_mode_user01_enter/run/short() ... app_mode_user10_enter/run/short()
 * 不需要修改按键库、事件分发逻辑或 s_modes[] 注册表。
 */
#include "app_mode_menu.h"
#include "app_line_track.h"
#include "app_custom_modes.h"
#include "oled.h"
#include "osal_api.h"
#include <stddef.h>
#include <stdio.h>

#define APP_MODE_MENU_TASK_PERIOD_MS  (100U)
#define APP_MODE_MENU_VISIBLE_ROWS    (4U)
#define APP_MODE_MENU_LINE_SIZE       (24U)

/* -------------------------------------------------------------------------- */
/* 十个固定模式的内部注册表。用户不需要修改这里。                           */
/* -------------------------------------------------------------------------- */
typedef void (*app_mode_function_t)(void);

typedef struct {
    const char *name;
    app_mode_function_t run;
    app_mode_function_t on_enter;
    app_mode_function_t on_short;
} app_mode_item_t;

static const app_mode_item_t s_modes[] = {
    { "CUST01", app_mode_user01_run, app_mode_user01_enter, app_mode_user01_short },
    { "CUST02", app_mode_user02_run, app_mode_user02_enter, app_mode_user02_short },
    { "CUST03", app_mode_user03_run, app_mode_user03_enter, app_mode_user03_short },
    { "CUST04", app_mode_user04_run, app_mode_user04_enter, app_mode_user04_short },
    { "CUST05", app_mode_user05_run, app_mode_user05_enter, app_mode_user05_short },
    { "CUST06", app_mode_user06_run, app_mode_user06_enter, app_mode_user06_short },
    { "CUST07", app_mode_user07_run, app_mode_user07_enter, app_mode_user07_short },
    { "CUST08", app_mode_user08_run, app_mode_user08_enter, app_mode_user08_short },
    { "CUST09", app_mode_user09_run, app_mode_user09_enter, app_mode_user09_short },
    { "CUST10", app_mode_user10_run, app_mode_user10_enter, app_mode_user10_short }
};

#define APP_MODE_COUNT ((uint8_t)(sizeof(s_modes) / sizeof(s_modes[0])))

typedef enum {
    APP_MODE_MENU_LIST = 0,
    APP_MODE_MENU_RUN  = 1
} app_mode_menu_state_t;

/* 按键回调只投递事件；OLED 和模式业务均在菜单任务中执行。 */
static volatile uint8_t s_pending_event = 0U;
static uint8_t s_current_mode = 0U;
static app_mode_menu_state_t s_state = APP_MODE_MENU_LIST;
static bool s_refresh_needed = true;

static void app_mode_menu_make_line(char *line,
                                    size_t line_size,
                                    const char *name,
                                    bool selected)
{
    size_t i;

    if (line == NULL || name == NULL || line_size < 3U) {
        return;
    }

    line[0] = selected ? '>' : ' ';
    line[1] = ' ';
    i = 0U;
    while ((i + 3U < line_size) && (name[i] != '\0')) {
        line[i + 2U] = name[i];
        ++i;
    }
    line[i + 2U] = '\0';
}

static void app_mode_menu_draw_list(void)
{
    uint8_t first;
    uint8_t row;
    uint8_t index;
    char line[APP_MODE_MENU_LINE_SIZE];

    OLED_NewFrame();

    first = 0U;
    if (s_current_mode >= APP_MODE_MENU_VISIBLE_ROWS) {
        first = (uint8_t)(s_current_mode - APP_MODE_MENU_VISIBLE_ROWS + 1U);
    }
    if ((uint8_t)(first + APP_MODE_MENU_VISIBLE_ROWS) > APP_MODE_COUNT) {
        first = (APP_MODE_COUNT > APP_MODE_MENU_VISIBLE_ROWS)
                    ? (uint8_t)(APP_MODE_COUNT - APP_MODE_MENU_VISIBLE_ROWS)
                    : 0U;
    }

    for (row = 0U; row < APP_MODE_MENU_VISIBLE_ROWS; ++row) {
        index = (uint8_t)(first + row);
        if (index >= APP_MODE_COUNT) {
            break;
        }
        app_mode_menu_make_line(line, sizeof(line), s_modes[index].name,
                                index == s_current_mode);
        OLED_PrintASCIIString(0U, (uint8_t)(row * 16U), line,
                              &afont16x8, OLED_COLOR_NORMAL);
    }
    OLED_ShowFrame();
}

static void app_mode_menu_draw_mode(void)
{
    OLED_NewFrame();
    if (s_current_mode < APP_MODE_COUNT &&
        s_modes[s_current_mode].run != NULL) {
        s_modes[s_current_mode].run();
    }
    OLED_ShowFrame();
}

static void app_mode_menu_consume_event(void)
{
    uint8_t event;

    event = s_pending_event;
    s_pending_event = 0U;

    if (event == (uint8_t)KEY_EVENT_SHORT_PRESS) {
        if (s_state == APP_MODE_MENU_LIST) {
            /* 菜单列表短按：循环选择下一个模式。 */
            s_current_mode = (uint8_t)((s_current_mode + 1U) % APP_MODE_COUNT);
            s_refresh_needed = true;
        } else if (s_current_mode < APP_MODE_COUNT &&
                   s_modes[s_current_mode].on_short != NULL) {
            /* 模式内短按：只调用当前模式自己的短按接口。 */
            s_modes[s_current_mode].on_short();
        }
    } else if (event == (uint8_t)KEY_EVENT_LONG_PRESS) {
        if (s_state == APP_MODE_MENU_LIST) {
            /* 菜单列表长按：进入当前选中的模式。 */
            s_state = APP_MODE_MENU_RUN;
            if (s_current_mode < APP_MODE_COUNT &&
                s_modes[s_current_mode].on_enter != NULL) {
                s_modes[s_current_mode].on_enter();
            }
        } else {
            /* 模式内长按：退出当前模式，返回模式列表。 */
            s_state = APP_MODE_MENU_LIST;
        }
        s_refresh_needed = true;
    }
}

void app_mode_menu_on_key_event(key_event_type_t event)
{
    if (event == KEY_EVENT_SHORT_PRESS || event == KEY_EVENT_LONG_PRESS) {
        /* PA13 只有一个按键；保留最新一次完整手势即可。 */
        s_pending_event = (uint8_t)event;
    }
}

void app_mode_menu_task(void *param)
{
    (void)param;

    for (;;) {
        app_mode_menu_consume_event();
        if (s_state == APP_MODE_MENU_LIST) {
            if (s_refresh_needed) {
                app_mode_menu_draw_list();
                s_refresh_needed = false;
            }
        } else {
            app_mode_menu_draw_mode();
        }
        osal_task_delay_ms(APP_MODE_MENU_TASK_PERIOD_MS);
    }
}

uint8_t app_mode_menu_get_current(void)
{
    return s_current_mode;
}

bool app_mode_menu_is_running(void)
{
    return s_state == APP_MODE_MENU_RUN;
}

/* -------------------------------------------------------------------------- */
/* CUST01~CUST10 开发接口                                                     */
/*                                                                            */
/* 每个模式固定提供 3 个函数：                                                 */
/*   enter(): 长按进入该模式时调用一次；适合初始化变量、打开外设。              */
/*   run():   进入模式后每 100 ms 调用一次；必须保持非阻塞。                    */
/*   short(): 模式内短按 PA13 时调用一次；适合切换子状态或设置请求标志。         */
/*                                                                            */
/* 你只需要在对应函数体内填写代码。不要在这里增加 delay 或长时间等待。          */
/* -------------------------------------------------------------------------- */

/* ========================================================================== *
 * USER01：红外循迹状态机演示（默认关闭）                                    *
 *                                                                          *
 * 启用方法：将下方 #if 0 改为 #if 1                                        *
 *                                                                          *
 * 功能：每 100ms 调用 app_line_track_update() 读取 4 路红外传感器，        *
 *       执行循迹状态机，在 OLED 上显示路况、转向量和左右轮目标速度。       *
 *       短按 PA13 重置状态机。                                             *
 *                                                                          *
 * 注意：使用 snprintf 需将 app_main.c 中 APP_MENU_TASK_STACK_WORDS          *
 *       从 384 改为 512。                                                  *
 * ========================================================================== */
#if 0

void app_mode_user01_enter(void)
{
    app_line_track_init();
    printf("[LINE] mode entered\r\n");
}

void app_mode_user01_run(void)
{
    line_track_output_t out;
    char buf[24];

    app_line_track_update(&out);

    /* 第 1 行：路况状态 + 4bit 传感器原始值 */
    snprintf(buf, sizeof(buf), "%s  %d%d%d%d",
             app_line_track_state_name(out.current_state),
             out.ir_raw[0], out.ir_raw[1], out.ir_raw[2], out.ir_raw[3]);
    OLED_PrintASCIIString(0U, 0U, buf, &afont16x8, OLED_COLOR_NORMAL);

    /* 第 2 行：转向量 + 基础速度 */
    snprintf(buf, sizeof(buf), "TD:%+5.0f BS:%4.0f",
             out.turn_diff, out.base_speed_mm);
    OLED_PrintASCIIString(0U, 16U, buf, &afont16x8, OLED_COLOR_NORMAL);

    /* 第 3 行：左轮目标速度 (mm/s) */
    snprintf(buf, sizeof(buf), "L:%+6d",
             (int)(out.left_target_speed * 1000.0f));
    OLED_PrintASCIIString(0U, 32U, buf, &afont16x8, OLED_COLOR_NORMAL);

    /* 第 4 行：右轮目标速度 (mm/s) + 直角弯标志 */
    snprintf(buf, sizeof(buf), "R:%+6d  %s",
             (int)(out.right_target_speed * 1000.0f),
             out.turn90_active ? "T90" : "");
    OLED_PrintASCIIString(0U, 48U, buf, &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user01_short(void)
{
    app_line_track_reset();
    printf("[LINE] state reset\r\n");
}

#else  /* 默认：进入模式时发送对应自定义协议模板 */

void app_mode_user01_enter(void)
{
    /* 长按进入 CUST01：模拟上位机发送 opcode=0x20。 */
    app_custom_mode_send(0U);
}

void app_mode_user01_run(void)
{
    OLED_PrintASCIIString(0U, 0U, app_custom_mode_name(0U),
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          app_custom_mode_last_send_ok() ? "SEND OK" : "SEND FAIL",
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 32U, "SHORT: RESEND",
                          &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user01_short(void)
{
    /* 模式内短按：再次发送模板01。 */
    app_custom_mode_send(0U);
}

#endif

void app_mode_user02_enter(void)
{
    /* 长按进入 CUST02：模拟上位机发送自定义协议模板。 */
    app_custom_mode_send(1U);
}

void app_mode_user02_run(void)
{
    OLED_PrintASCIIString(0U, 0U, app_custom_mode_name(1U),
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          app_custom_mode_last_send_ok() ? "SEND OK" : "SEND FAIL",
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 32U, "SHORT: RESEND",
                          &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user02_short(void)
{
    /* 模式内短按：再次发送对应自定义协议模板。 */
    app_custom_mode_send(1U);
}

void app_mode_user03_enter(void)
{
    /* 长按进入 CUST03：模拟上位机发送自定义协议模板。 */
    app_custom_mode_send(2U);
}

void app_mode_user03_run(void)
{
    OLED_PrintASCIIString(0U, 0U, app_custom_mode_name(2U),
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          app_custom_mode_last_send_ok() ? "SEND OK" : "SEND FAIL",
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 32U, "SHORT: RESEND",
                          &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user03_short(void)
{
    /* 模式内短按：再次发送对应自定义协议模板。 */
    app_custom_mode_send(2U);
}

void app_mode_user04_enter(void)
{
    /* 长按进入 CUST04：模拟上位机发送自定义协议模板。 */
    app_custom_mode_send(3U);
}

void app_mode_user04_run(void)
{
    OLED_PrintASCIIString(0U, 0U, app_custom_mode_name(3U),
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          app_custom_mode_last_send_ok() ? "SEND OK" : "SEND FAIL",
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 32U, "SHORT: RESEND",
                          &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user04_short(void)
{
    /* 模式内短按：再次发送对应自定义协议模板。 */
    app_custom_mode_send(3U);
}

void app_mode_user05_enter(void)
{
    /* 长按进入 CUST05：模拟上位机发送自定义协议模板。 */
    app_custom_mode_send(4U);
}

void app_mode_user05_run(void)
{
    OLED_PrintASCIIString(0U, 0U, app_custom_mode_name(4U),
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          app_custom_mode_last_send_ok() ? "SEND OK" : "SEND FAIL",
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 32U, "SHORT: RESEND",
                          &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user05_short(void)
{
    /* 模式内短按：再次发送对应自定义协议模板。 */
    app_custom_mode_send(4U);
}

void app_mode_user06_enter(void)
{
    /* 长按进入 CUST06：模拟上位机发送自定义协议模板。 */
    app_custom_mode_send(5U);
}

void app_mode_user06_run(void)
{
    OLED_PrintASCIIString(0U, 0U, app_custom_mode_name(5U),
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          app_custom_mode_last_send_ok() ? "SEND OK" : "SEND FAIL",
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 32U, "SHORT: RESEND",
                          &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user06_short(void)
{
    /* 模式内短按：再次发送对应自定义协议模板。 */
    app_custom_mode_send(5U);
}

void app_mode_user07_enter(void)
{
    /* 长按进入 CUST07：模拟上位机发送自定义协议模板。 */
    app_custom_mode_send(6U);
}

void app_mode_user07_run(void)
{
    OLED_PrintASCIIString(0U, 0U, app_custom_mode_name(6U),
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          app_custom_mode_last_send_ok() ? "SEND OK" : "SEND FAIL",
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 32U, "SHORT: RESEND",
                          &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user07_short(void)
{
    /* 模式内短按：再次发送对应自定义协议模板。 */
    app_custom_mode_send(6U);
}

void app_mode_user08_enter(void)
{
    /* 长按进入 CUST08：模拟上位机发送自定义协议模板。 */
    app_custom_mode_send(7U);
}

void app_mode_user08_run(void)
{
    OLED_PrintASCIIString(0U, 0U, app_custom_mode_name(7U),
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          app_custom_mode_last_send_ok() ? "SEND OK" : "SEND FAIL",
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 32U, "SHORT: RESEND",
                          &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user08_short(void)
{
    /* 模式内短按：再次发送对应自定义协议模板。 */
    app_custom_mode_send(7U);
}

void app_mode_user09_enter(void)
{
    /* 长按进入 CUST09：模拟上位机发送自定义协议模板。 */
    app_custom_mode_send(8U);
}

void app_mode_user09_run(void)
{
    OLED_PrintASCIIString(0U, 0U, app_custom_mode_name(8U),
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          app_custom_mode_last_send_ok() ? "SEND OK" : "SEND FAIL",
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 32U, "SHORT: RESEND",
                          &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user09_short(void)
{
    /* 模式内短按：再次发送对应自定义协议模板。 */
    app_custom_mode_send(8U);
}

void app_mode_user10_enter(void)
{
    /* 长按进入 CUST10：模拟上位机发送自定义协议模板。 */
    app_custom_mode_send(9U);
}

void app_mode_user10_run(void)
{
    OLED_PrintASCIIString(0U, 0U, app_custom_mode_name(9U),
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          app_custom_mode_last_send_ok() ? "SEND OK" : "SEND FAIL",
                          &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 32U, "SHORT: RESEND",
                          &afont16x8, OLED_COLOR_NORMAL);
}

void app_mode_user10_short(void)
{
    /* 模式内短按：再次发送对应自定义协议模板。 */
    app_custom_mode_send(9U);
}
