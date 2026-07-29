#include "task_key.h"
#include "bsp_key.h"
#include "osal_api.h"
#include "key_config.h"

/*
 * 扫描链路：FreeRTOS 任务 -> bsp_key_manager_poll() -> key_poll()
 * -> 消抖/状态机 -> app_key_event_callback()。
 * 任务本身不读取 GPIO，也不直接控制电机。
 */
/** 按 PRJ_KEY_SCAN_PERIOD_MS 周期推进所有按键状态机。 */
void app_key_task(void *param)
{
    /* app_main 创建任务时把板级按键管理器作为参数传入。 */
    bsp_key_manager_t *manager = (bsp_key_manager_t *)param;

    if (manager == NULL) {
        osal_task_delete(NULL);
        return;
    }

    for (;;) {
        /* 一次 poll 会处理消抖、短按、长按、卡键和开关事件。 */
        (void)bsp_key_manager_poll(manager,
            osal_ticks_to_ms(osal_get_tick_count()));
        osal_task_delay_ms(PRJ_KEY_SCAN_PERIOD_MS);
    }
}
