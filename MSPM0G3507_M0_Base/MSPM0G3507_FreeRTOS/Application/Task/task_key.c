/**
 * @file    task_key.c
 * @brief   按键驱动和扫描任务。
 */
#include "task_key.h"
#include "bsp_key.h"
#include "key_config.h"
#include "osal_api.h"

void app_key_task(void *param)
{
    bsp_key_manager_t *manager = (bsp_key_manager_t *)param;

    if (manager == NULL) {
        /* 按键驱动和扫描任务。 */
        osal_task_delete(NULL);
        return;
    }

    for (;;) {
        /* 按键驱动和扫描任务。 */
        (void)bsp_key_manager_poll(manager,
                                   osal_ticks_to_ms(osal_get_tick_count()));
        osal_task_delay_ms(PRJ_KEY_SCAN_PERIOD_MS);
    }
}
