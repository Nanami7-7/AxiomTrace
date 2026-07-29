/**
 * @file    status_cache.h
 * @brief   Board B 状态缓存模块 (规范 §8.4)
 * @note    缓存最近一次 STATUS_SUMMARY, STATUS_MOTOR[], STATUS_SENSOR, STATUS_FAULT。
 *          使用有符号半范围比较判断 status_generation 新旧:
 *            is_newer(a,b) = ((int16_t)(a-b) > 0)
 *          相等视为重复，差值超过 32767 时按回绕处理。
 *          OLED 刷新不能直接读取协议 RX 缓冲区，必须读取状态缓存。
 *          不分配堆内存。
 */
#ifndef STATUS_CACHE_H
#define STATUS_CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include "proto_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量
 * ======================================================================== */

/** 缓存支持的最大电机数量 (不分配堆内存，使用固定数组) */
#ifndef STATUS_CACHE_MAX_MOTORS
#define STATUS_CACHE_MAX_MOTORS  8u
#endif

/* ========================================================================
 * 状态缓存上下文
 * ======================================================================== */

typedef struct {
    /* SUMMARY 缓存 */
    proto_snapshot_summary_t summary;
    bool                     summary_valid;
    uint16_t                 summary_generation;  /**< 已缓存的 generation (用于比较) */

    /* MOTOR 缓存 (每电机独立) */
    proto_snapshot_motor_t   motors[STATUS_CACHE_MAX_MOTORS];
    bool                     motor_valid[STATUS_CACHE_MAX_MOTORS];
    uint8_t                  motor_count;         /**< 配置的电机数量 */

    /* SENSOR 缓存 */
    proto_snapshot_sensor_t  sensor;
    bool                     sensor_valid;

    /* FAULT 缓存 */
    proto_snapshot_fault_t   fault;
    bool                     fault_valid;

    /* 链路统计 (规范 §8.4: last_board_rx_ms, CRC 错误数, 超时数, 重试数) */
    uint32_t                 last_board_rx_ms;
    uint32_t                 crc_error_count;
    uint32_t                 timeout_count;
    uint32_t                 retry_count;
} status_cache_t;

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief  初始化状态缓存
 * @param  cache       状态缓存上下文
 * @param  motor_count 电机数量 (不得超过 STATUS_CACHE_MAX_MOTORS，超出则截断)
 */
void status_cache_init(status_cache_t *cache, uint8_t motor_count);

/**
 * @brief  更新 SUMMARY 缓存
 * @param  cache   状态缓存上下文
 * @param  summary 收到的 STATUS_SUMMARY 快照
 * @retval true    收到的 generation 比缓存新 (或缓存为空)，已更新
 * @retval false   收到的 generation 不比缓存新 (重复或旧)，未更新
 * @note   使用 is_newer(a,b) = ((int16_t)(a-b) > 0) 比较新旧。
 *         相等视为重复，差值超过 32767 时按回绕处理。
 */
bool status_cache_update_summary(status_cache_t *cache,
                                 const proto_snapshot_summary_t *summary);

/**
 * @brief  更新 MOTOR 缓存
 * @param  cache    状态缓存上下文
 * @param  motor_id 电机 ID
 * @param  motor    收到的 STATUS_MOTOR 快照
 * @retval true     motor_id 有效且已更新
 * @retval false    motor_id 超出范围，未更新
 */
bool status_cache_update_motor(status_cache_t *cache, uint8_t motor_id,
                               const proto_snapshot_motor_t *motor);

/**
 * @brief  更新 SENSOR 缓存
 * @param  cache  状态缓存上下文
 * @param  sensor 收到的 STATUS_SENSOR 快照
 * @retval true   已更新
 */
bool status_cache_update_sensor(status_cache_t *cache,
                                const proto_snapshot_sensor_t *sensor);

/**
 * @brief  更新 FAULT 缓存
 * @param  cache 状态缓存上下文
 * @param  fault 收到的 STATUS_FAULT 快照
 * @retval true  已更新
 */
bool status_cache_update_fault(status_cache_t *cache,
                               const proto_snapshot_fault_t *fault);

/**
 * @brief  读取缓存的 SUMMARY 快照
 * @param  cache 状态缓存上下文
 * @param  out   输出: 快照 (若缓存无效则填零)
 */
void status_cache_get_summary(const status_cache_t *cache,
                              proto_snapshot_summary_t *out);

/**
 * @brief  读取缓存的 MOTOR 快照
 * @param  cache    状态缓存上下文
 * @param  motor_id 电机 ID
 * @param  out      输出: 快照 (若缓存无效则填零)
 */
void status_cache_get_motor(const status_cache_t *cache, uint8_t motor_id,
                            proto_snapshot_motor_t *out);

/**
 * @brief  有符号半范围比较: status_generation 新旧判断
 * @param  a 候选 generation
 * @param  b 基准 generation
 * @retval true  a 比 b 新
 * @retval false a 不比 b 新 (相等或更旧)
 * @note   is_newer(a,b) = ((int16_t)(a-b) > 0)
 *         相等视为重复，差值超过 32767 时按回绕处理 (规范 §6.2.1, §8.4)。
 */
bool status_cache_is_newer(uint16_t a, uint16_t b);

#ifdef __cplusplus
}
#endif
#endif /* STATUS_CACHE_H */
