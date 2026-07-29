/**
 * @file    status_cache.c
 * @brief   Board B 状态缓存模块实现 (规范 §8.4)
 * @note    缓存最近一次 STATUS_SUMMARY, STATUS_MOTOR[], STATUS_SENSOR, STATUS_FAULT。
 *          使用有符号半范围比较判断 status_generation 新旧。
 *          不分配堆内存。
 */
#include "status_cache.h"
#include <string.h>

/* ========================================================================
 * 公共 API
 * ======================================================================== */

void status_cache_init(status_cache_t *cache, uint8_t motor_count)
{
    if (cache == NULL) {
        return;
    }

    memset(cache, 0, sizeof(*cache));

    /* 电机数量截断到最大值 */
    if (motor_count > STATUS_CACHE_MAX_MOTORS) {
        motor_count = STATUS_CACHE_MAX_MOTORS;
    }
    cache->motor_count = motor_count;
}

/**
 * @brief 判断函数 status_cache_is_newer，完成对应模块的功能处理。
 * @param a 函数参数 a。
 * @param b 函数参数 b。
 * @return 函数执行结果。
 */
bool status_cache_is_newer(uint16_t a, uint16_t b)
{
    /*
     * 有符号半范围比较 (规范 §6.2.1, §8.4):
     *   is_newer(a,b) = ((int16_t)(a-b) > 0)
     * 相等 (a==b) 时差值为 0，结果为 false (重复)。
     * 差值超过 32767 时按回绕处理：例如 a=0x0000, b=0xFFFF，
     *   (int16_t)(0x0000 - 0xFFFF) = (int16_t)(0x0001) = 1 > 0 → true
     */
    return ((int16_t)(a - b) > 0);
}

/**
 * @brief 更新函数 status_cache_update_summary，完成对应模块的功能处理。
 * @param cache 函数参数 cache。
 * @param summary 函数参数 summary。
 * @return 函数执行结果。
 */
bool status_cache_update_summary(status_cache_t *cache,
                                 const proto_snapshot_summary_t *summary)
{
    if (cache == NULL || summary == NULL) {
        return false;
    }

    uint16_t incoming_gen = summary->status_generation;

    /*
     * 首次缓存或收到更新的 generation 时更新。
     * 使用有符号半范围比较: 仅当 incoming_gen 比缓存新时才更新。
     * 相等视为重复，不更新 (规范 §8.4)。
     */
    if (!cache->summary_valid ||
        status_cache_is_newer(incoming_gen, cache->summary_generation)) {
        cache->summary            = *summary;
        cache->summary_valid      = true;
        cache->summary_generation = incoming_gen;
        return true;
    }

    /* 重复或更旧的 generation，不覆盖缓存 */
    return false;
}

/**
 * @brief 更新函数 status_cache_update_motor，完成对应模块的功能处理。
 * @param cache 函数参数 cache。
 * @param motor_id 函数参数 motor_id。
 * @param motor 函数参数 motor。
 * @return 函数执行结果。
 */
bool status_cache_update_motor(status_cache_t *cache, uint8_t motor_id,
                               const proto_snapshot_motor_t *motor)
{
    if (cache == NULL || motor == NULL) {
        return false;
    }

    /*
     * motor_id 校验: 不得超过配置的电机数量。
     * STATUS_MOTOR 不携带 status_generation，通过同一批次 SUMMARY 的 generation
     * 关联 (规范 §6.2.1)。收到合法 MOTOR 帧时直接更新缓存。
     */
    if (motor_id >= cache->motor_count) {
        return false;
    }

    cache->motors[motor_id]      = *motor;
    cache->motor_valid[motor_id] = true;
    return true;
}

/**
 * @brief 更新函数 status_cache_update_sensor，完成对应模块的功能处理。
 * @param cache 函数参数 cache。
 * @param sensor 函数参数 sensor。
 * @return 函数执行结果。
 */
bool status_cache_update_sensor(status_cache_t *cache,
                                const proto_snapshot_sensor_t *sensor)
{
    if (cache == NULL || sensor == NULL) {
        return false;
    }

    /*
     * STATUS_SENSOR 不携带 status_generation，通过同一批次关联 (规范 §6.2.1)。
     * 收到合法 SENSOR 帧时直接更新缓存。
     */
    cache->sensor       = *sensor;
    cache->sensor_valid = true;
    return true;
}

/**
 * @brief 更新函数 status_cache_update_fault，完成对应模块的功能处理。
 * @param cache 函数参数 cache。
 * @param fault 函数参数 fault。
 * @return 函数执行结果。
 */
bool status_cache_update_fault(status_cache_t *cache,
                               const proto_snapshot_fault_t *fault)
{
    if (cache == NULL || fault == NULL) {
        return false;
    }

    cache->fault       = *fault;
    cache->fault_valid = true;
    return true;
}

/**
 * @brief 获取函数 status_cache_get_summary，完成对应模块的功能处理。
 * @param cache 函数参数 cache。
 * @param out 函数参数 out。
 * @return 函数执行结果。
 */
void status_cache_get_summary(const status_cache_t *cache,
                              proto_snapshot_summary_t *out)
{
    if (out == NULL) {
        return;
    }

    if (cache == NULL || !cache->summary_valid) {
        memset(out, 0, sizeof(*out));
        return;
    }

    *out = cache->summary;
}

/**
 * @brief 获取函数 status_cache_get_motor，完成对应模块的功能处理。
 * @param cache 函数参数 cache。
 * @param motor_id 函数参数 motor_id。
 * @param out 函数参数 out。
 * @return 函数执行结果。
 */
void status_cache_get_motor(const status_cache_t *cache, uint8_t motor_id,
                            proto_snapshot_motor_t *out)
{
    if (out == NULL) {
        return;
    }

    if (cache == NULL ||
        motor_id >= cache->motor_count ||
        !cache->motor_valid[motor_id]) {
        memset(out, 0, sizeof(*out));
        return;
    }

    *out = cache->motors[motor_id];
}
