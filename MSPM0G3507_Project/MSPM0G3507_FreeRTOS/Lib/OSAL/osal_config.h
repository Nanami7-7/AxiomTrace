/**
 * @file    osal_config.h
 * @brief   OSAL编译配置
 * @note    通过宏开关控制RTOS/裸机模式，在编译时确定，
 *          零运行时开销。修改此文件后需重新编译整个项目
 */
#ifndef OSAL_CONFIG_H
#define OSAL_CONFIG_H

/* ======================== RTOS模式选择 ======================== */

/**
 * @brief  启用FreeRTOS模式
 * @note   设为1: 使用FreeRTOS内核原语(互斥锁/信号量/队列/任务)
 *         设为0: 裸机模式，使用关中断/volatile标志/环形缓冲区替代
 *         默认启用FreeRTOS
 */
#ifndef USE_FREERTOS
#define USE_FREERTOS           1
#endif

/* ======================== 任务默认参数 ======================== */

/** 任务默认优先级(最低) */
#define OSAL_TASK_PRIORITY_LOW     1U
/** 任务默认优先级(中) */
#define OSAL_TASK_PRIORITY_NORMAL  2U
/** 任务默认优先级(高) */
#define OSAL_TASK_PRIORITY_HIGH    3U
/** 任务默认优先级(最高，低于空闲任务) */
#define OSAL_TASK_PRIORITY_HIGHEST 9U

/** 默认任务栈大小(单位:字,即4字节) */
#define OSAL_TASK_STACK_DEFAULT    256U
/** 最小任务栈大小(单位:字) */
#define OSAL_TASK_STACK_MIN        128U

/* ======================== 队列默认参数 ======================== */

/** 默认队列长度 */
#define OSAL_QUEUE_DEFAULT_LEN     16U
/** 默认队列项大小(字节) */
#define OSAL_QUEUE_DEFAULT_SIZE    sizeof(uint32_t)

/* ======================== 超时定义 ======================== */

/** 永久等待(阻塞直到条件满足) */
#define OSAL_WAIT_FOREVER          0xFFFFFFFFUL
/** 不等待(立即返回) */
#define OSAL_NO_WAIT              0U

/* ======================== 裸机模式配置 ======================== */

#if !USE_FREERTOS
/**
 * @brief  裸机模式环形缓冲区大小
 * @note   用于裸机模式下osal_queue的内部缓冲区
 */
#define OSAL_BARE_QUEUE_BUF_SIZE   32U
#endif

#endif /* OSAL_CONFIG_H */
