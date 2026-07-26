#include "task_key.h"
#include "bsp_key.h"
#include "osal_api.h"
#include "key_config.h"

void app_key_task(void *param)
{
    bsp_key_manager_t *manager = (bsp_key_manager_t *)param;

    if (manager == NULL) {
        osal_task_delete(NULL);
        return;
    }

    for (;;) {
        (void)bsp_key_manager_poll(manager,
            osal_ticks_to_ms(osal_get_tick_count()));
        osal_task_delay_ms(PRJ_KEY_SCAN_PERIOD_MS);
    }
}
