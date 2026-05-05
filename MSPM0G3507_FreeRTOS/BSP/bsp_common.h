/**
 * @file    bsp_common.h
 * @brief   BSP板级支持包公共定义
 * @note    提供BSP层统一的错误码、通用类型、断言宏和环形缓冲区
 *          所有BSP模块包含此头文件以获取公共定义
 */
#ifndef BSP_COMMON_H
#define BSP_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ======================== BSP错误码 ======================== */

/**
 * @brief BSP操作返回状态码
 * @note  扩展自hal_status_t，增加BSP层业务错误码
 */
typedef enum {
    BSP_OK                = 0,   /**< 操作成功 */
    BSP_ERR_NULL_PTR      = -1,  /**< 空指针参数 */
    BSP_ERR_INVALID_PARAM = -2,  /**< 无效参数 */
    BSP_ERR_BUSY          = -3,  /**< 设备忙 */
    BSP_ERR_TIMEOUT       = -4,  /**< 操作超时 */
    BSP_ERR_NOT_INIT      = -5,  /**< 设备未初始化 */
    BSP_ERR_NAK           = -6,  /**< I2C从机无应答 */
    BSP_ERR_BUF_FULL      = -7,  /**< 缓冲区已满 */
    BSP_ERR_BUF_EMPTY     = -8,  /**< 缓冲区为空 */
    BSP_ERR_HW_FAULT      = -9,  /**< 硬件故障 */
} bsp_status_t;

/* ======================== 通用宏 ======================== */

/** 数组元素个数 */
#define BSP_ARRAY_SIZE(arr)    (sizeof(arr) / sizeof((arr)[0]))

/** 最小值 */
#define BSP_MIN(a, b)         ((a) < (b) ? (a) : (b))
/** 最大值 */
#define BSP_MAX(a, b)         ((a) > (b) ? (a) : (b))

/** 将x对齐到align的整数倍(向上) */
#define BSP_ALIGN_UP(x, align) \
    (((x) + (align) - 1U) & ~((align) - 1U))

/* ======================== BSP断言 ======================== */

/**
 * @brief  BSP断言宏
 * @note   DEBUG模式下捕获编程错误(死循环)，Release模式下忽略
 */
#ifdef DEBUG
#define BSP_ASSERT(cond)        \
    do {                       \
        if (!(cond)) {         \
            for (;;) {         \
            }                  \
        }                      \
    } while (0)
#else
#define BSP_ASSERT(cond) ((void)0)
#endif

/* ======================== 环形缓冲区 ======================== */

/**
 * @brief 环形缓冲区结构体
 * @note  用于UART接收缓冲区等场景
 *        单生产者(ISR写入)单消费者(任务读取)无需加锁
 *        多生产者/多消费者需用OSAL临界区保护
 */
typedef struct {
    uint8_t    *buf;       /**< 数据缓冲区指针 */
    uint32_t    buf_size;  /**< 缓冲区总大小(字节,必须为2的幂) */
    volatile uint32_t head; /**< 读索引(消费者递增) */
    volatile uint32_t tail; /**< 写索引(生产者递增) */
} bsp_ringbuf_t;

/**
 * @brief  初始化环形缓冲区
 * @param  rb       环形缓冲区指针
 * @param  buf      数据缓冲区(大小必须为2的幂)
 * @param  buf_size 缓冲区大小(字节)
 */
static inline void bsp_ringbuf_init(bsp_ringbuf_t *rb,
    uint8_t *buf, uint32_t buf_size)
{
    rb->buf      = buf;
    rb->buf_size = buf_size;
    rb->head     = 0U;
    rb->tail     = 0U;
}

/**
 * @brief  向环形缓冲区写入一个字节(生产者调用)
 * @param  rb   环形缓冲区指针
 * @param  data 待写入字节
 * @retval true  写入成功
 * @retval false 缓冲区已满
 */
static inline bool bsp_ringbuf_put(bsp_ringbuf_t *rb, uint8_t data)
{
    uint32_t next_tail = (rb->tail + 1U) & (rb->buf_size - 1U);

    /* 缓冲区满判断: next_tail追上head */
    if (next_tail == rb->head) {
        return false;
    }

    rb->buf[rb->tail] = data;
    rb->tail = next_tail;
    return true;
}

/**
 * @brief  从环形缓冲区读取一个字节(消费者调用)
 * @param  rb   环形缓冲区指针
 * @param  data 读取字节存放地址
 * @retval true  读取成功
 * @retval false 缓冲区为空
 */
static inline bool bsp_ringbuf_get(bsp_ringbuf_t *rb, uint8_t *data)
{
    /* 缓冲区空判断: tail等于head */
    if (rb->tail == rb->head) {
        return false;
    }

    *data = rb->buf[rb->head];
    rb->head = (rb->head + 1U) & (rb->buf_size - 1U);
    return true;
}

/**
 * @brief  查询环形缓冲区中的数据长度
 * @param  rb 环形缓冲区指针
 * @retval 缓冲区中有效数据字节数
 */
static inline uint32_t bsp_ringbuf_count(const bsp_ringbuf_t *rb)
{
    return (rb->tail - rb->head) & (rb->buf_size - 1U);
}

/**
 * @brief  查询环形缓冲区是否为空
 * @param  rb 环形缓冲区指针
 * @retval true  缓冲区为空
 * @retval false 缓冲区有数据
 */
static inline bool bsp_ringbuf_is_empty(const bsp_ringbuf_t *rb)
{
    return (rb->tail == rb->head);
}

/**
 * @brief  清空环形缓冲区
 * @param  rb 环形缓冲区指针
 */
static inline void bsp_ringbuf_flush(bsp_ringbuf_t *rb)
{
    rb->head = 0U;
    rb->tail = 0U;
}

#ifdef __cplusplus
}
#endif

#endif /* BSP_COMMON_H */
