/**
 * @file    app_main.c
 * @brief   Application entry point — initializes hardware and starts FreeRTOS
 */

#include "bsp_board.h"
#include "app_task.h"
#include "osal_task.h"
#include "osal_common.h"

/* ---------- FreeRTOS hook functions ---------- */

void vApplicationIdleHook(void)
{
    /* Optional: enter low-power mode here */
    __asm volatile ("wfi");
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    /* Stack overflow detected — halt for debugging */
    __asm volatile ("bkpt #0");
    for (;;) {}
}

void vApplicationMallocFailedHook(void)
{
    /* malloc failed — halt for debugging */
    __asm volatile ("bkpt #0");
    for (;;) {}
}

/* ========== Main entry ========== */

int main(void)
{
    /* 1. Initialize all board hardware (HAL + BSP) */
    bsp_board_init();

    /* 2. Create application tasks */
    app_task_create_all();

    /* 3. Start OS scheduler (never returns) */
    osal_start_scheduler();

    /* Should never reach here */
    for (;;) {}
}
