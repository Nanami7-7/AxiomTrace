/**
 * @file    osal_queue.c
 * @brief   OSAL消息队列实现
 * @note    根据USE_FREERTOS宏条件编译:
 *          - FreeRTOS模式: 封装xQueueCreate/xQueueSend/xQueueReceive等
 *          - 裸机模式: 环形缓冲区+关中断保护(无阻塞)
 */
#include "osal_api.h"

#if USE_FREERTOS
/* ======================== FreeRTOS模式实现 ======================== */

osal_queue_t osal_queue_create(uint32_t queue_len, uint32_t item_size)
{
    return xQueueCreate((UBaseType_t)queue_len, (UBaseType_t)item_size);
}

void osal_queue_delete(osal_queue_t queue)
{
    if (NULL != queue) {
        vQueueDelete(queue);
    }
}

osal_status_t osal_queue_send(osal_queue_t queue, const void *item,
    uint32_t timeout_ms)
{
    if ((NULL == queue) || (NULL == item)) {
        return OSAL_ERR_INVALID_PARAM;
    }

    TickType_t ticks;
    if (0U == timeout_ms) {
        ticks = 0U;
    } else if (OSAL_WAIT_FOREVER == timeout_ms) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(timeout_ms);
    }

    if (pdTRUE == xQueueSend(queue, item, ticks)) {
        return OSAL_OK;
    }

    return OSAL_ERR_TIMEOUT;
}

osal_status_t osal_queue_receive(osal_queue_t queue, void *item,
    uint32_t timeout_ms)
{
    if ((NULL == queue) || (NULL == item)) {
        return OSAL_ERR_INVALID_PARAM;
    }

    TickType_t ticks;
    if (OSAL_WAIT_FOREVER == timeout_ms) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(timeout_ms);
    }

    if (pdTRUE == xQueueReceive(queue, item, ticks)) {
        return OSAL_OK;
    }

    return OSAL_ERR_TIMEOUT;
}

osal_status_t osal_queue_send_from_isr(osal_queue_t queue,
    const void *item)
{
    if ((NULL == queue) || (NULL == item)) {
        return OSAL_ERR_INVALID_PARAM;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;

    if (pdTRUE == xQueueSendFromISR(queue, item,
        &higher_priority_task_woken)) {
        portYIELD_FROM_ISR(higher_priority_task_woken);
        return OSAL_OK;
    }

    return OSAL_ERR_BUSY;
}

#else /* !USE_FREERTOS */
/* ======================== 裸机模式实现 ======================== */

/**
 * @brief 裸机模式队列结构体(环形缓冲区)
 */
struct osal_queue {
    uint8_t    *buf;       /**< 数据缓冲区指针 */
    uint32_t    head;      /**< 读索引 */
    uint32_t    tail;      /**< 写索引 */
    uint32_t    count;     /**< 当前消息数 */
    uint32_t    capacity;  /**< 队列容量(消息条数) */
    uint32_t    item_size; /**< 每条消息大小(字节) */
};

osal_queue_t osal_queue_create(uint32_t queue_len, uint32_t item_size)
{
    (void)queue_len;
    (void)item_size;
    /* 裸机模式: 需调用者提供静态存储，此处返回NULL */
    return NULL;
}

void osal_queue_delete(osal_queue_t queue)
{
    (void)queue;
}

osal_status_t osal_queue_send(osal_queue_t queue, const void *item,
    uint32_t timeout_ms)
{
    (void)timeout_ms;

    if ((NULL == queue) || (NULL == item)) {
        return OSAL_ERR_INVALID_PARAM;
    }

    uint32_t primask = osal_critical_enter_raw();

    if (queue->count >= queue->capacity) {
        osal_critical_exit_raw(primask);
        return OSAL_ERR_BUSY;
    }

    /* 复制消息到环形缓冲区 */
    uint8_t *dst = queue->buf + queue->tail * queue->item_size;
    const uint8_t *src = (const uint8_t *)item;
    for (uint32_t i = 0U; i < queue->item_size; i++) {
        dst[i] = src[i];
    }

    queue->tail = (queue->tail + 1U) % queue->capacity;
    queue->count++;

    osal_critical_exit_raw(primask);

    return OSAL_OK;
}

osal_status_t osal_queue_receive(osal_queue_t queue, void *item,
    uint32_t timeout_ms)
{
    (void)timeout_ms;

    if ((NULL == queue) || (NULL == item)) {
        return OSAL_ERR_INVALID_PARAM;
    }

    uint32_t primask = osal_critical_enter_raw();

    if (0U == queue->count) {
        osal_critical_exit_raw(primask);
        return OSAL_ERR_TIMEOUT;
    }

    /* 从环形缓冲区复制消息 */
    uint8_t *dst = (uint8_t *)item;
    const uint8_t *src = queue->buf + queue->head * queue->item_size;
    for (uint32_t i = 0U; i < queue->item_size; i++) {
        dst[i] = src[i];
    }

    queue->head = (queue->head + 1U) % queue->capacity;
    queue->count--;

    osal_critical_exit_raw(primask);

    return OSAL_OK;
}

osal_status_t osal_queue_send_from_isr(osal_queue_t queue,
    const void *item)
{
    /* 裸机模式: ISR中直接调用send(已在ISR中无需额外关中断) */
    if ((NULL == queue) || (NULL == item)) {
        return OSAL_ERR_INVALID_PARAM;
    }

    if (queue->count >= queue->capacity) {
        return OSAL_ERR_BUSY;
    }

    uint8_t *dst = queue->buf + queue->tail * queue->item_size;
    const uint8_t *src = (const uint8_t *)item;
    for (uint32_t i = 0U; i < queue->item_size; i++) {
        dst[i] = src[i];
    }

    queue->tail = (queue->tail + 1U) % queue->capacity;
    queue->count++;

    return OSAL_OK;
}

#endif /* USE_FREERTOS */
