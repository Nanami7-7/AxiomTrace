#ifndef OSAL_CONFIG_H
#define OSAL_CONFIG_H

/* Generic OSAL configuration for the FreeRTOS-based M0 starter project. */

#ifndef USE_FREERTOS
#define USE_FREERTOS 1
#endif

#define OSAL_TASK_PRIORITY_LOW      1U
#define OSAL_TASK_PRIORITY_NORMAL   2U
#define OSAL_TASK_PRIORITY_HIGH     3U
#define OSAL_TASK_PRIORITY_HIGHEST  9U

#define OSAL_TASK_STACK_DEFAULT     256U
#define OSAL_TASK_STACK_MIN         128U

#define OSAL_QUEUE_DEFAULT_LEN      16U
#define OSAL_QUEUE_DEFAULT_SIZE     sizeof(uint32_t)

#define OSAL_WAIT_FOREVER           0xFFFFFFFFUL
#define OSAL_NO_WAIT                0U

#if !USE_FREERTOS
#define OSAL_BARE_QUEUE_BUF_SIZE    32U
#endif

#endif /* OSAL_CONFIG_H */
