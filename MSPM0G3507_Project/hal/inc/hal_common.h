/**
 * @file    hal_common.h
 * @brief   HAL common type definitions (error codes, IRQ priority, assertions)
 */

#ifndef HAL_COMMON_H
#define HAL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Includes ---------- */
#include <stdint.h>
#include <stddef.h>

/* ---------- HAL status codes ---------- */
typedef enum {
    HAL_OK       = 0,
    HAL_ERROR    = -1,
    HAL_BUSY     = -2,
    HAL_TIMEOUT  = -3,
    HAL_INVALID  = -4,
} hal_status_t;

/* ---------- IRQ priority levels ---------- */
typedef enum {
    HAL_IRQ_PRIORITY_LOWEST  = 0,
    HAL_IRQ_PRIORITY_LOW     = 1,
    HAL_IRQ_PRIORITY_MEDIUM  = 2,
    HAL_IRQ_PRIORITY_HIGH    = 3,
    HAL_IRQ_PRIORITY_HIGHEST = 4,
} hal_irq_priority_t;

/* ---------- Bit position macro ---------- */
#ifndef BIT
#define BIT(n)  (1U << (n))
#endif

/* ---------- HAL assertion ---------- */
#ifdef HAL_DEBUG
  #define HAL_ASSERT(cond)  do { if (!(cond)) { hal_assert_fail(__FILE__, __LINE__); } } while(0)
  void hal_assert_fail(const char *file, uint32_t line);
#else
  #define HAL_ASSERT(cond)  ((void)0)
#endif

/* ---------- NULL check helper ---------- */
#define HAL_CHECK_PTR(ptr)  do { if ((ptr) == NULL) { return HAL_INVALID; } } while(0)

/* ---------- Timeout constants ---------- */
#define HAL_WAIT_FOREVER  0xFFFFFFFF  /* Infinite timeout */

/* ---------- Tick counter (HAL-level, RTOS-agnostic) ---------- */
/**
 * @brief  Get current system tick count in milliseconds
 * @note   Default implementation uses SysTick; if HAL_USE_FREERTOS_TICK is defined,
 *         uses xTaskGetTickCount() from FreeRTOS instead.
 * @return Tick count in milliseconds
 */
uint32_t hal_get_tick(void);

/**
 * @brief  Check if a timeout has expired
 * @param  start_tick  Starting tick (from hal_get_tick())
 * @param  timeout_ms  Timeout in milliseconds
 * @return 1 if expired, 0 if not
 */
static inline uint32_t hal_timeout_expired(uint32_t start_tick, uint32_t timeout_ms)
{
    if (timeout_ms == HAL_WAIT_FOREVER) return 0;
    return (hal_get_tick() - start_tick) >= timeout_ms;
}

#ifdef __cplusplus
}
#endif

#endif /* HAL_COMMON_H */
