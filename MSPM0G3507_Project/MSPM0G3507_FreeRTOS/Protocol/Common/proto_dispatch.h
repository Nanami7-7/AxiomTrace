/**
 * @file    proto_dispatch.h
 * @brief   命令分发器 + 安全状态机 + ACK/NACK/STATUS/EVENT 生成 (规范 §2.2, §6, §7)
 * @note    P2 范围: STOP / STOP_ALL / ABORT / DISABLE / CLEAR_FAULT + HEARTBEAT
 *          P3 范围: ENABLE / RUN / SET_TARGET / SET_MODE + HEARTBEAT 增强
 *          P4 范围: QUERY_INFO / QUERY_STATUS / QUERY_FAULT / QUERY_SENSOR + STATUS 帧
 *          P5 范围: EVT_FAULT / EVT_STATE_CHANGED 完整化 + 周期 STATUS
 *          通过适配器回调与硬件解耦，不直接访问寄存器。
 *          不分配堆内存。
 */
#ifndef PROTO_DISPATCH_H
#define PROTO_DISPATCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "proto_types.h"
#include "proto_frame.h"
#include "proto_replay.h"
#include "proto_link_watchdog.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 用户自定义命令结果
 * ======================================================================== */
typedef enum {
    PROTO_USER_RESULT_ACCEPTED = 0,          /**< 已接受，后续异步执行 */
    PROTO_USER_RESULT_COMPLETED = 1,         /**< 应用状态已完成 */
    PROTO_USER_RESULT_ALREADY_IN_STATE = 2,  /**< 目标状态已经满足 */
    PROTO_USER_RESULT_BAD_PARAM = 3,         /**< 参数错误，返回 NACK */
    PROTO_USER_RESULT_BUSY = 4,              /**< 当前忙，返回 NACK */
    PROTO_USER_RESULT_UNSUPPORTED = 5,       /**< 不支持，返回 NACK */
    PROTO_USER_RESULT_INTERNAL_ERROR = 6     /**< 内部错误，返回 NACK */
} proto_user_result_t;
/* ========================================================================
 * 适配器回调 — 应用层必须提供，协议层通过它操作硬件
 * ======================================================================== */
typedef struct {
    /* 时间 */
    uint32_t (*now_ms)(void);                         /**< 当前毫秒时间戳 */

    /* P1 安全门控：返回 true 才允许执行对应命令。 */
    bool     (*command_allowed)(uint8_t opcode);

    /* 用户自定义命令：协议核心只负责长度、状态和 ACK/NACK。 */
    bool     (*user_command_get_info)(uint8_t opcode,
                                      uint16_t *min_payload_len,
                                      uint16_t *max_payload_len,
                                      bool *idempotent);
    proto_user_result_t (*user_command_execute)(uint8_t opcode,
                                                const uint8_t *payload,
                                                uint16_t payload_len,
                                                uint16_t *detail_code);

    /* 电机控制 (P2 停止类) */
    void     (*motor_stop)(uint8_t motor_id, uint8_t stop_type);  /**< 停止单电机 */
    void     (*motor_stop_all)(void);                              /**< 停止全部电机 */
    void     (*motor_abort)(void);                                 /**< 终止位置/角度动作 */
    void     (*motor_disable_output)(void);                        /**< 关闭电机输出 */

    /* 电机控制 (P3 运行类) */
    void     (*motor_enable_output)(void);                         /**< 打开电机输出使能 */
    bool     (*motor_run)(void);                                    /**< 启动控制循环; false=条件不满足(目标/模式无效) */
    void     (*motor_set_target)(uint8_t motor_id, uint8_t mode,   /**< 设置单电机目标 */
                                 int32_t target_value, int32_t limit_value);
    void     (*motor_set_mode)(uint8_t motor_id, uint8_t mode);    /**< 设置控制模式 */
    uint8_t  (*motor_get_mode)(uint8_t motor_id);                  /**< 查询当前模式; 0xFF=未知/未设置 */

    /* 故障管理 */
    bool     (*fault_clear)(uint16_t fault_code);      /**< 尝试清除故障，true=成功 */
    bool     (*fault_has_active_hardware)(void);        /**< 是否有活动不可清除硬件故障 */

    /* 状态快照 (P4 QUERY 响应 + P5 周期 STATUS) */
    void     (*status_get_summary)(proto_snapshot_summary_t *out);   /**< 填充 SUMMARY 快照 */
    void     (*status_get_motor)(uint8_t motor_id, proto_snapshot_motor_t *out);  /**< 填充单电机快照 */
    void     (*status_get_sensor)(proto_snapshot_sensor_t *out);     /**< 填充传感器快照 */
    void     (*status_get_fault)(proto_snapshot_fault_t *out);       /**< 填充故障快照 */
    void     (*status_get_info)(proto_snapshot_info_t *out);         /**< 填充设备信息快照 */
    uint32_t (*get_uptime_ms)(void);                                  /**< 设备运行时间 (ms) */

    /* 发送 */
    bool     (*tx_send)(const uint8_t *wire, size_t len);  /**< 发送线上帧 (COBS+0x00) */

    /* 临界区保护 (可选，NULL=无保护) */
    void     (*enter_critical)(void);
    void     (*exit_critical)(void);
} proto_dispatch_adapter_t;

/* ========================================================================
 * 分发器上下文
 * ======================================================================== */
typedef struct {
    proto_state_t         state;                /**< 当前安全状态机状态 */
    proto_watchdog_t      watchdog;             /**< 失联 watchdog */
    proto_replay_cache_t  replay;               /**< 重复包缓存 */

    uint16_t              queue_generation;     /**< 命令接受代数 (§6.2.1) */
    uint16_t              last_command_seq;     /**< 最近命令 seq */
    uint16_t              last_control_seq;     /**< 最近控制刷新 seq */
    uint16_t              event_seq;            /**< 事件帧独立 seq */
    uint16_t              status_generation;    /**< 状态快照代数 (§6.2.1) */
    uint16_t              periodic_status_seq;  /**< 周期 STATUS 独立 seq */
    uint32_t              last_periodic_status_ms; /**< 上次周期 STATUS 发送时间 */

    uint32_t              bad_heartbeat_count;  /**< 坏心跳计数 */
    uint32_t              invalid_enum_count;   /**< 非法枚举值计数 */
    uint8_t               motor_count;          /**< 有效电机数量 (用于 motor_id 校验) */
    bool                  link_recovery_started;/**< LINK_LOST 后是否已收到有效 HEARTBEAT (§7.2.1) */

    const proto_dispatch_adapter_t *adapter;    /**< 适配器回调 */
    proto_stats_t        *stats;                /**< 协议统计 (可为 NULL) */
} proto_dispatch_ctx_t;

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief  初始化分发器上下文
 * @param  ctx      分发器上下文
 * @param  adapter  适配器回调 (必须非 NULL，now_ms 和 tx_send 必须提供)
 * @param  stats    协议统计 (可为 NULL)
 * @note   初始状态为 PROTO_STATE_SAFE。
 */
void proto_dispatch_init(proto_dispatch_ctx_t *ctx,
                         const proto_dispatch_adapter_t *adapter,
                         proto_stats_t *stats);

/**
 * @brief  处理一个已 COBS 解码的逻辑帧
 * @param  ctx          分发器上下文
 * @param  decoded      COBS 解码后的逻辑帧数据
 * @param  decoded_len  逻辑帧长度
 * @note   完整流程: 帧校验 → flags 校验 → 重复包检测 →
 *         opcode/payload 校验 → 状态机检查 → 执行 → ACK/NACK → 重放缓存。
 *         HEARTBEAT 和有效控制命令刷新 watchdog。
 */
void proto_dispatch_process_frame(proto_dispatch_ctx_t *ctx,
                                  const uint8_t *decoded, size_t decoded_len);

/**
 * @brief  周期 tick — 在每个控制周期调用
 * @param  ctx  分发器上下文
 * @note   检查 watchdog 超时，若超时则触发 LINK_LOST:
 *         安全停机 → 关闭输出 → 锁存 FAULT_LINK_LOST → 状态 → LINK_LOST → 发送事件。
 *         若当前为 FAULT，不覆盖硬件故障。
 */
void proto_dispatch_tick(proto_dispatch_ctx_t *ctx);

/**
 * @brief  获取当前安全状态
 */
proto_state_t proto_dispatch_get_state(const proto_dispatch_ctx_t *ctx);

/**
 * @brief  获取距最近控制刷新的时间 (ms)
 */
uint32_t proto_dispatch_link_age_ms(const proto_dispatch_ctx_t *ctx);

/**
 * @brief  获取当前状态快照代数
 */
uint16_t proto_dispatch_get_status_generation(const proto_dispatch_ctx_t *ctx);

/**
 * @brief  周期状态发布 — 在控制周期中调用
 * @param  ctx             分发器上下文
 * @param  now_ms          当前时间戳
 * @param  period_ms       周期 STATUS 发送间隔 (如 100ms)
 * @note   生成 STATUS_SUMMARY + N×STATUS_MOTOR + STATUS_SENSOR 序列。
 *         使用 periodic_status_seq 递增序号，flags=IS_PERIODIC。
 *         不发送 ACK/NACK，不刷新 watchdog。
 *         若 adapter 缺少快照回调，静默跳过。
 */
void proto_dispatch_periodic_status(proto_dispatch_ctx_t *ctx,
                                    uint32_t now_ms, uint32_t period_ms);

/**
 * @brief  通知硬件故障发生 — 触发 EVT_FAULT 事件
 * @param  ctx             分发器上下文
 * @param  fault_snapshot  故障快照 (若 NULL，通过 adapter 获取)
 * @note   将状态转换为 FAULT (若当前非 FAULT)，发送 EVT_FAULT 事件。
 *         若当前已在 FAULT，仅更新故障快照并发送事件。
 */
void proto_dispatch_notify_fault(proto_dispatch_ctx_t *ctx,
                                 const proto_snapshot_fault_t *fault_snapshot);

#ifdef __cplusplus
}
#endif
#endif /* PROTO_DISPATCH_H */
