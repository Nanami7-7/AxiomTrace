#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Generic FreeRTOS configuration for MSPM0G3507 at 32 MHz. */

#include <stdint.h>
#include <stddef.h>

#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configIDLE_SHOULD_YIELD                1
#define configCPU_CLOCK_HZ                     ((unsigned long)32000000UL)
#define configTICK_RATE_HZ                     ((TickType_t)1000U)
#define configMAX_PRIORITIES                    6U
#define configMINIMAL_STACK_SIZE                128U
#define configMAX_TASK_NAME_LEN                 16U
#define configTOTAL_HEAP_SIZE                  ((size_t)(14U * 1024U))
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         1
#define configUSE_16_BIT_TICKS                  0
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               3U
#define configTIMER_QUEUE_LENGTH                10U
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE
#define configUSE_CO_ROUTINES                   0
#define configUSE_TRACE_FACILITY               0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_TICKLESS_IDLE                0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configQUEUE_REGISTRY_SIZE               0
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP   2
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0
#define configUSE_NEWLIB_REENTRANT              0
#define configAPPLICATION_ALLOCATED_HEAP        0

#define configIDLE_TASK_STACK_DEPTH             configMINIMAL_STACK_SIZE

#define configASSERT(x) do { if (!(x)) { taskDISABLE_INTERRUPTS(); for (;;) { } } } while (0)

#define INCLUDE_vTaskPrioritySet               1
#define INCLUDE_uxTaskPriorityGet              1
#define INCLUDE_vTaskDelete                    1
#define INCLUDE_vTaskCleanUpResources          0
#define INCLUDE_vTaskSuspend                   1
#define INCLUDE_vTaskDelayUntil                1
#define INCLUDE_vTaskDelay                     1
#define INCLUDE_uxTaskGetStackHighWaterMark    1
#define INCLUDE_xTaskGetIdleTaskHandle         0
#define INCLUDE_eTaskGetState                  1
#define INCLUDE_xTaskResumeFromISR             1
#define INCLUDE_xTaskGetCurrentTaskHandle      1
#define INCLUDE_xTaskGetSchedulerState         1
#define INCLUDE_xSemaphoreGetMutexHolder       0
#define INCLUDE_xTimerPendFunctionCall         0

/* MSPM0 Cortex-M0+ implements four priority levels. */
#ifdef __NVIC_PRIO_BITS
#define configPRIO_BITS                        __NVIC_PRIO_BITS
#else
#define configPRIO_BITS                        2U
#endif
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY       3U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  1U
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

#define configENABLE_ISR_STACK_INIT             0

/* Map the FreeRTOS port handlers to the CMSIS startup names. */
#ifndef __TI_COMPILER_VERSION__
#define xPortPendSVHandler                     PendSV_Handler
#define vPortSVCHandler                        SVC_Handler
#define xPortSysTickHandler                    SysTick_Handler
#endif

#endif /* FREERTOS_CONFIG_H */
