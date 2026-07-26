/**
 * @file    app_main.c
 * @brief   FreeRTOS application: OLED display + 4-ch IR line follower.
 *
 * Peripherals:
 *   - OLED  (software I2C): PA0=SCL, PA1=SDA
 *   - IR    (GPIO input)  : PA12=CH1, PA13=CH2, PA14=CH3, PA15=CH4
 *   - UART0 (debug print) : PA10=TX, PA11=RX, 115200-8-N-1
 */
#include "app_main.h"
#include "osal_api.h"
#include "oled.h"
#include "oled_port.h"
#include "bsp_ir.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <string.h>
////////////////////////////////////////////
#define APP_BASE_TASK_STACK_WORDS   (512U)
#define APP_BASE_TASK_PRIORITY      (1U)
#define APP_BASE_TASK_PERIOD_MS     (200U)

static osal_task_handle_t s_base_task;

/*
 * IR display helper: render a compact 4-channel status bar on OLED.
 *   y    : starting Y pixel
 *   state: 4-byte array, 1=black line, 0=white
 */
static void ir_oled_display(uint8_t y, const uint8_t state[4])
{
    char line[22];
    /*
     * Show: "IR [1 0 1 0]"  — 1=black, 0=white
     */
    snprintf(line, sizeof(line), "IR[%d %d %d %d]",
             state[0], state[1], state[2], state[3]);
    OLED_PrintASCIIString(0, y, line, &afont16x8, OLED_COLOR_NORMAL);
}

static void base_task(void *param)
{
    uint8_t  ir_state[4];
    uint8_t  toggle = 0;
    uint32_t tick   = 0;
    (void)param;

    for (;;) {
        /* ---- 1. Read IR sensors ---- */
        BSP_IR_Read(ir_state);      /* logical: 1=black line, 0=white */

        /* ---- 2. UART debug print ---- */
        printf("[%lu] IR: %d %d %d %d\r\n",
               (unsigned long)tick,
               ir_state[0], ir_state[1], ir_state[2], ir_state[3]);

        /* ---- 3. OLED display ---- */
        OLED_NewFrame();
        OLED_PrintASCIIString(0, 0,  "MSPM0 IR+OLED",   &afont16x8, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(0, 16, "PA12-15=IR CH1-4", &afont16x8, OLED_COLOR_NORMAL);
        ir_oled_display(32, ir_state);
        OLED_PrintASCIIString(0, 48, "1=Black 0=White", &afont16x8,
                              toggle ? OLED_COLOR_REVERSED : OLED_COLOR_NORMAL);
        OLED_ShowFrame();
        toggle = !toggle;
        tick++;

        osal_task_delay_ms(APP_BASE_TASK_PERIOD_MS);
    }
}

int32_t app_main_init(void)
{
    /* 初始化 OLED 软件 I2C 引脚, 等待上电稳定后初始化 OLED 屏 */
    OLED_Port_Init();
    DL_Common_delayCycles(CPUCLK_FREQ / 10U); /* 约 100ms */
    OLED_Init();

    /* 初始化4路红外巡线 GPIO (PA12-PA15, 上拉输入) */
    BSP_IR_Init();

    printf("=== MSPM0G3507 IR + OLED Demo ===\r\n");
    printf("IR pins: CH1=PA12 CH2=PA13 CH3=PA14 CH4=PA15\r\n");
    printf("Level: 1=Black line, 0=White surface\r\n");

    s_base_task = osal_task_create(base_task,
        "base",
        APP_BASE_TASK_STACK_WORDS,
        NULL,
        APP_BASE_TASK_PRIORITY);

    return (s_base_task != NULL) ? 0 : -1;
}
////////////////////////////////////////////
