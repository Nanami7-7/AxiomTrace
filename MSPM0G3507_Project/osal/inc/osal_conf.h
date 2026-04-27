/**
 * @file    osal_conf.h
 * @brief   OSAL layer configuration — maps to FreeRTOS parameters
 */

#ifndef OSAL_CONF_H
#define OSAL_CONF_H

/* ========== OSAL Feature Enables ========== */
/* Set to 1 to include each OSAL module; 0 to exclude */
#define OSAL_TASK_ENABLED          1
#define OSAL_DELAY_ENABLED         1
#define OSAL_SEMAPHORE_ENABLED     1
#define OSAL_MUTEX_ENABLED         1
#define OSAL_QUEUE_ENABLED         1
#define OSAL_TIMER_ENABLED         1
#define OSAL_CRITICAL_ENABLED      1

/* ========== OSAL Default Task Parameters ========== */
#define OSAL_DEFAULT_STACK_SIZE    256     /* Words (1024 bytes) */
#define OSAL_MAX_NAME_LEN          16

/* ========== OSAL Timer Defaults ========== */
#define OSAL_TIMER_DEFAULT_PERIOD_MS  1000

/* ========== OSAL Queue Defaults ========== */
#define OSAL_QUEUE_DEFAULT_LENGTH     8
#define OSAL_QUEUE_DEFAULT_ITEM_SIZE  sizeof(uint32_t)

#endif /* OSAL_CONF_H */
