/**
 * @file    osal_api.h
 * @brief   OSAL统一操作系统接口
 * @note    提供任务/互斥锁/信号量/队列/延时/临界区的统一API，
 *          根据osal_config.h中USE_FREERTOS宏自动选择实现:
 *          - FreeRTOS模式: 封装FreeRTOS内核原语
 *          - 裸机模式: 关中断/volatile标志/环形缓冲区替代
 *          APP层和BSP层仅include此头文件，不直接引用RTOS头文件
 */
#ifndef OSAL_API_H
#define OSAL_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "osal_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ======================== OSAL错误码 ======================== */

/** OSAL操作返回状态码 */
typedef enum {
    OSAL_OK              = 0,   /**< 操作成功 */
    OSAL_ERR_TIMEOUT     = -1,  /**< 等待超时 */
    OSAL_ERR_INVALID_PARAM = -2,/**< 无效参数 */
    OSAL_ERR_NO_MEM      = -3,  /**< 内存不足 */
    OSAL_ERR_BUSY        = -4,  /**< 资源忙 */
    OSAL_ERR_FAIL        = -5,  /**< 操作失败 */
} osal_status_t;

/* ======================== 条件类型定义 ======================== */

#if USE_FREERTOS
/* ---- FreeRTOS模式: 使用FreeRTOS句柄类型 ---- */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/** 任务句柄类型 */
typedef TaskHandle_t        osal_task_handle_t;
/** 互斥锁句柄类型 */
typedef SemaphoreHandle_t   osal_mutex_t;
/** 信号量句柄类型 */
typedef SemaphoreHandle_t   osal_sem_t;
/** 队列句柄类型 */
typedef QueueHandle_t       osal_queue_t;

#else
/* ---- 裸机模式: 使用简化类型 ---- */

/** 任务函数类型 */
typedef void (*osal_task_func_t)(void *param);

/** 任务句柄类型(裸机模式为函数指针+参数) */
typedef struct {
    osal_task_func_t func;   /**< 任务函数指针 */
    void            *param;  /**< 任务参数 */
} osal_task_handle_t;

/** 互斥锁类型(裸机模式为嵌套计数+中断状态) */
typedef struct {
    volatile int32_t count;     /**< 嵌套计数(0=未锁定) */
    volatile uint32_t int_mask; /**< 保存的中断状态 */
} osal_mutex_t;

/** 信号量类型(裸机模式为volatile计数) */
typedef struct {
    volatile int32_t count;   /**< 信号量计数 */
    int32_t          max_count; /**< 最大计数 */
} osal_sem_t;

/** 队列类型前向声明(裸机模式在osal_queue.c中定义) */
typedef struct osal_queue osal_queue_t;

#endif /* USE_FREERTOS */

/* ======================== 任务管理接口 ======================== */

/**
 * @brief  任务函数类型
 * @param  param 任务入口参数
 */
typedef void (*osal_task_func_t)(void *param);

/**
 * @brief  创建任务
 * @param  func       任务函数
 * @param  name       任务名称(调试用,最长configMAX_TASK_NAME_LEN)
 * @param  stack_size 任务栈大小(单位:字,即4字节)
 * @param  param      任务入口参数,可为NULL
 * @param  priority   任务优先级(1=最低, configMAX_PRIORITIES-1=最高)
 * @retval 非NULL 任务句柄
 * @retval NULL   创建失败(内存不足)
 */
osal_task_handle_t osal_task_create(osal_task_func_t func,
    const char *name, uint16_t stack_size, void *param,
    uint32_t priority);

/**
 * @brief  删除任务
 * @param  handle 任务句柄,NULL则删除当前任务
 */
void osal_task_delete(osal_task_handle_t handle);

/**
 * @brief  获取当前任务句柄
 * @retval 当前任务句柄
 */
osal_task_handle_t osal_task_get_current(void);

/**
 * @brief  任务延时(毫秒)
 * @note   FreeRTOS: vTaskDelay(pdMS_TO_TICKS(ms))
 *         裸机: 忙等延时(调用delay_cycle)
 * @param  ms 延时毫秒数
 */
void osal_task_delay_ms(uint32_t ms);

/**
 * @brief  获取当前系统滴答计数
 * @note   FreeRTOS: xTaskGetTickCount()
 *         裸机: 自维护的全局滴答计数器
 * @retval 当前滴答计数值
 */
uint32_t osal_get_tick_count(void);

/**
 * @brief  毫秒转换为滴答数
 * @note   FreeRTOS: pdMS_TO_TICKS(ms)
 *         裸机: 原值返回(1 tick = 1 ms)
 * @param  ms 毫秒数
 * @retval 对应的滴答数
 */
uint32_t osal_ms_to_ticks(uint32_t ms);

/**
 * @brief  滴答数转换为毫秒
 * @note   FreeRTOS: ticks * portTICK_PERIOD_MS
 *         裸机: 原值返回(1 tick = 1 ms)
 * @param  ticks 滴答数
 * @retval 对应的毫秒数
 */
uint32_t osal_ticks_to_ms(uint32_t ticks);

/* ======================== 互斥锁接口 ======================== */

/**
 * @brief  创建互斥锁
 * @retval 非NULL 互斥锁句柄
 * @retval NULL   创建失败
 */
osal_mutex_t osal_mutex_create(void);

/**
 * @brief  删除互斥锁
 * @param  mutex 互斥锁句柄
 */
void osal_mutex_delete(osal_mutex_t mutex);

/**
 * @brief  获取互斥锁(阻塞)
 * @note   FreeRTOS: xSemaphoreTake(mutex, portMAX_DELAY)
 *         裸机: 关中断
 * @param  mutex 互斥锁句柄
 */
void osal_mutex_lock(osal_mutex_t mutex);

/**
 * @brief  释放互斥锁
 * @param  mutex 互斥锁句柄
 */
void osal_mutex_unlock(osal_mutex_t mutex);

/* ======================== 信号量接口 ======================== */

/**
 * @brief  创建计数信号量
 * @param  init_count 初始计数值
 * @param  max_count  最大计数值
 * @retval 非NULL 信号量句柄
 * @retval NULL   创建失败
 */
osal_sem_t osal_sem_create(uint32_t init_count, uint32_t max_count);

/**
 * @brief  删除信号量
 * @param  sem 信号量句柄
 */
void osal_sem_delete(osal_sem_t sem);

/**
 * @brief  等待信号量(可超时)
 * @param  sem        信号量句柄
 * @param  timeout_ms 超时时间(ms), OSAL_WAIT_FOREVER=永久等待
 * @retval OSAL_OK    成功获取
 * @retval OSAL_ERR_TIMEOUT 等待超时
 */
osal_status_t osal_sem_wait(osal_sem_t sem, uint32_t timeout_ms);

/**
 * @brief  释放信号量(任务上下文)
 * @param  sem 信号量句柄
 */
void osal_sem_release(osal_sem_t sem);

/**
 * @brief  释放信号量(ISR上下文)
 * @note   ISR中必须使用此函数，不可使用osal_sem_release
 * @param  sem 信号量句柄
 */
void osal_sem_release_from_isr(osal_sem_t sem);

/* ======================== 消息队列接口 ======================== */

/**
 * @brief  创建消息队列
 * @param  queue_len  队列长度(消息条数)
 * @param  item_size  每条消息大小(字节)
 * @retval 非NULL 队列句柄
 * @retval NULL   创建失败
 */
osal_queue_t osal_queue_create(uint32_t queue_len, uint32_t item_size);

/**
 * @brief  删除消息队列
 * @param  queue 队列句柄
 */
void osal_queue_delete(osal_queue_t queue);

/**
 * @brief  发送消息到队列(任务上下文)
 * @param  queue     队列句柄
 * @param  item      消息内容指针
 * @param  timeout_ms 超时时间(ms), 0=不等待
 * @retval OSAL_OK   发送成功
 * @retval OSAL_ERR_TIMEOUT 队列满,等待超时
 */
osal_status_t osal_queue_send(osal_queue_t queue, const void *item,
    uint32_t timeout_ms);

/**
 * @brief  从队列接收消息(任务上下文)
 * @param  queue     队列句柄
 * @param  item      接收缓冲区指针
 * @param  timeout_ms 超时时间(ms), OSAL_WAIT_FOREVER=永久等待
 * @retval OSAL_OK   接收成功
 * @retval OSAL_ERR_TIMEOUT 队列空,等待超时
 */
osal_status_t osal_queue_receive(osal_queue_t queue, void *item,
    uint32_t timeout_ms);

/**
 * @brief  发送消息到队列(ISR上下文)
 * @param  queue     队列句柄
 * @param  item      消息内容指针
 * @retval OSAL_OK   发送成功
 * @retval OSAL_ERR_FULL 队列满
 */
osal_status_t osal_queue_send_from_isr(osal_queue_t queue,
    const void *item);

/* ======================== 临界区接口 ======================== */

/**
 * @brief  进入临界区
 * @note   FreeRTOS: taskENTER_CRITICAL()
 *         裸机: __disable_irq()
 */
void osal_critical_enter(void);

/**
 * @brief  退出临界区
 * @note   FreeRTOS: taskEXIT_CRITICAL()
 *         裸机: __enable_irq()
 */
void osal_critical_exit(void);

/**
 * @brief  临界区保护代码块宏
 * @note   用法: OSAL_CRITICAL_SECTION { ... }
 *         自动处理进入/退出临界区
 */
#define OSAL_CRITICAL_SECTION  \
    for (uint32_t _cs = (osal_critical_enter(), 0U); \
         _cs == 0U; \
         (osal_critical_exit(), _cs++))

/* ======================== 裸机模式专用接口 ======================== */

#if !USE_FREERTOS
/**
 * @brief  关中断并返回之前的中断状态(裸机模式专用)
 * @retval 之前的中断状态(PRIMASK值)
 * @note   供osal_mutex/osal_sem的裸机实现内部使用
 */
uint32_t osal_critical_enter_raw(void);

/**
 * @brief  恢复中断状态(裸机模式专用)
 * @param  primask 之前保存的PRIMASK值
 */
void osal_critical_exit_raw(uint32_t primask);

/**
 * @brief  初始化SysTick延时基础(裸机模式专用)
 * @note   配置SysTick为1ms周期倒计数(不使能中断)
 *         必须在首次调用osal_delay_us/ms之前调用
 *         FreeRTOS模式下无需调用(调度器自动配置SysTick)
 *         典型调用位置: main()中SYSCFG_DL_init()之后
 */
void osal_delay_init(void);

/**
 * @brief  检查SysTick延时是否已初始化(裸机模式专用)
 * @retval true  已初始化
 * @retval false 未初始化
 */
bool osal_delay_is_inited(void);
#endif

/* ======================== 延时接口 ======================== */

/**
 * @brief  忙等延时(毫秒)
 * @note   基于SysTick倒计数，可用于ISR和初始化阶段
 *         FreeRTOS任务中优先使用osal_task_delay_ms(让出CPU)
 *         裸机模式: 需先调用osal_delay_init()初始化SysTick
 * @param  ms 延时毫秒数
 */
void osal_delay_ms(uint32_t ms);

/**
 * @brief  忙等延时(微秒)
 * @note   基于SysTick倒计数(80MHz=12.5ns/cycle)，精度约1us
 *         适用于软件I2C时序、短延时等场景
 *         裸机模式: 需先调用osal_delay_init()初始化SysTick
 * @param  us 延时微秒数
 */
void osal_delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_API_H */
