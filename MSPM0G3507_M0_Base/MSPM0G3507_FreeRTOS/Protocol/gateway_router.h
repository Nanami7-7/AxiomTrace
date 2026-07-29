/**
 * @file    gateway_router.h
 * @brief   Board B 网关路由接口。
 * @note    Board B 负责 BLE/OLED 主机与 Board A 之间的协议转发、事务管理、
 *          状态缓存以及 BLE 物理断开时的 STOP_ALL/DISABLE 安全停机。
 *
 *          当前版本已取消 HEARTBEAT 保活机制：
 *          - 不主动发送 HEARTBEAT；
 *          - 不要求主机先发送 HEARTBEAT；
 *          - 不使用主机控制租约和租约超时；
 *          - 收到旧版本 HEARTBEAT 时仅兼容接收，不转发、不改变控制状态。
 *
 *          普通 COMMAND/QUERY 可在 BLE 连接后直接转发到 Board A。
 *          所有多字节字段使用 little-endian，不分配堆内存。
 */
#ifndef GATEWAY_ROUTER_H
#define GATEWAY_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "proto_types.h"
#include "proto_frame.h"
#include "proto_cobs.h"
#include "proto_crc.h"
#include "gateway_transactions.h"
#include "status_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 适配器回调接口：由 BSP/应用层提供
 * ======================================================================== */

typedef struct {
    /** 发送已编码的协议帧到 Board A（COBS 数据 + 0x00 分隔符）。 */
    bool (*tx_board_link)(const uint8_t *wire, size_t len);

    /** 发送已编码的协议帧到主机/BLE（COBS 数据 + 0x00 分隔符）。 */
    bool (*tx_ble)(const uint8_t *wire, size_t len);

    /** 获取当前系统毫秒计数。 */
    uint32_t (*now_ms)(void);

    /** 可选的临界区保护回调；传入 NULL 表示不使用临界区保护。 */
    void (*enter_critical)(void);
    void (*exit_critical)(void);
} gateway_router_adapter_t;

/* ========================================================================
 * BLE 断开状态机
 * ======================================================================== */

typedef enum {
    GATEWAY_DISCONNECT_IDLE          = 0,  /**< 空闲。 */
    GATEWAY_DISCONNECT_STOP_ALL_SENT = 1,  /**< STOP_ALL 已发送，等待 ACK 或约 40 ms。 */
    GATEWAY_DISCONNECT_DISABLE_SENT  = 2,  /**< DISABLE 已发送，等待 ACK 或约 40 ms。 */
    GATEWAY_DISCONNECT_DONE          = 3,  /**< 断开安全停机流程已完成。 */
} gateway_disconnect_state_t;

/* ========================================================================
 * 网关路由器上下文
 * ======================================================================== */

typedef struct {
    /* 子模块。 */
    gateway_txn_manager_t              txn_mgr;       /**< 事务表管理器。 */
    status_cache_t                     cache;         /**< Board A 状态缓存。 */
    const gateway_router_adapter_t    *adapter;      /**< 底层发送、时间和临界区回调。 */

    /* 电机配置。 */
    uint8_t                             motor_count;  /**< 电机数量；用于 QUERY_STATUS 预期帧数。 */

    /* 序号管理。 */
    uint16_t                            heartbeat_seq;  /**< 旧版 HEARTBEAT 兼容字段，当前不使用。 */
    uint16_t                            host_status_seq;/**< 转发到主机的 STATUS 独立序号。 */
    uint16_t                            host_event_seq; /**< 转发到主机的 EVENT 独立序号。 */

    /* 旧版 HEARTBEAT/租约字段：保留结构布局，当前不参与控制逻辑。 */
    uint16_t                            control_epoch;      /**< 旧版控制代数，当前不递增。 */
    uint8_t                             requested_mode;     /**< 旧版请求模式，当前不使用。 */
    uint8_t                             requested_run;      /**< 旧版运行意图，当前不使用。 */
    uint32_t                            last_heartbeat_ms;  /**< 旧版最近心跳时间，当前不使用。 */
    bool                                heartbeat_active;   /**< 旧版心跳开关，当前固定为 false。 */

    /* BLE 连接状态；不再表示主机控制租约。 */
    uint32_t                            last_host_activity_ms; /**< 兼容字段，当前不使用。 */
    bool                                ble_connected;         /**< BLE 物理连接状态。 */
    bool                                lease_valid;            /**< 兼容字段，当前不使用。 */

    /* BLE 断开安全停机状态机。 */
    gateway_disconnect_state_t          disconnect_state;
    uint32_t                            disconnect_sent_ms;   /**< 当前断开步骤的发送时间。 */
    uint16_t                            disconnect_seq;       /**< 当前断开步骤使用的 Board A 序号。 */
    bool                                lease_expired_handled;/**< 兼容字段，当前不使用。 */

    /* QUERY 响应状态跟踪，独立于事务表。 */
    uint8_t                             txn_status_expected[PROTO_GATEWAY_TXN_TABLE_SIZE];
    uint8_t                             txn_status_received[PROTO_GATEWAY_TXN_TABLE_SIZE];

    /* 统计信息。 */
    uint32_t                            unmatched_response_count;   /**< 未匹配事务的 ACK/NACK 数量。 */
    uint32_t                            malformed_board_frame_count;/**< Board A 上行帧语义非法数量。 */
    uint32_t                            invalid_enum_count;         /**< 收到保留枚举值的次数。 */
    uint32_t                            board_tx_fail_count;        /**< 发往 Board A 的底层发送失败次数。 */
    uint32_t                            local_nack_count;           /**< Board B 本地生成的 NACK 数量。 */
    uint32_t                            host_frames_received;       /**< 收到的主机帧数量。 */
    uint32_t                            board_frames_received;      /**< 收到的 Board A 帧数量。 */
} gateway_router_t;

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief  初始化 Board B 网关路由器。
 * @param  r        路由器上下文。
 * @param  adapter  Board A、BLE、时间和临界区适配器；允许为 NULL。
 * @note   motor_count 未设置时，底层初始化默认使用 4 个电机。
 */
void gateway_router_init(gateway_router_t *r,
                         const gateway_router_adapter_t *adapter);

/**
 * @brief  从 Board B 本地 OLED Mode 向 Board A 发送自定义命令。
 * @param  r           路由器上下文。
 * @param  opcode      自定义命令操作码。
 * @param  payload     命令参数；无参数时可传 NULL。
 * @param  payload_len 参数长度。
 * @return true 表示已提交到底层发送流程，false 表示参数错误或发送失败。
 * @note   使用正常事务、ACK/NACK、超时和重试流程，但不向 BLE 主动回传结果。
 */
bool gateway_router_send_custom_command(gateway_router_t *r,
                                         uint8_t opcode,
                                         const uint8_t *payload,
                                         uint16_t payload_len);

/**
 * @brief  处理主机发来的 COBS 解码帧。
 * @param  r       路由器上下文。
 * @param  decoded COBS 解码后的逻辑帧。
 * @param  len     逻辑帧长度。
 * @note   HEARTBEAT 仅为旧上位机保留兼容接收，不参与租约、转发或状态恢复。
 *         COMMAND 和 QUERY 在 BLE 已连接时可以直接转发到 Board A。
 */
void gateway_router_on_host_frame(gateway_router_t *r,
                                  const uint8_t *decoded,
                                  size_t len);

/**
 * @brief  处理 Board A 发来的 COBS 解码帧。
 * @param  r       路由器上下文。
 * @param  decoded COBS 解码后的逻辑帧。
 * @param  len     逻辑帧长度。
 * @note   ACK/NACK 使用事务表匹配；STATUS/EVENT 更新 Board B 缓存并按需转发。
 *         找不到匹配事务的 ACK/NACK 只递增 unmatched_response_count。
 */
void gateway_router_on_board_frame(gateway_router_t *r,
                                   const uint8_t *decoded,
                                   size_t len);

/**
 * @brief  周期维护网关事务和 BLE 断开安全流程。
 * @param  r      路由器上下文。
 * @param  now_ms 当前系统毫秒计数。
 * @note   只处理事务超时重试、超时 NACK 和 BLE 断开状态机；
 *         不处理控制租约，也不周期发送 HEARTBEAT。
 */
void gateway_router_tick(gateway_router_t *r, uint32_t now_ms);

/**
 * @brief  通知网关 BLE 已连接。
 * @param  r 路由器上下文。
 * @note   BLE 连接后无需先发送 HEARTBEAT，合法 COMMAND/QUERY 可直接转发。
 */
void gateway_router_on_ble_connected(gateway_router_t *r);

/**
 * @brief  通知网关 BLE 已断开，执行安全停机兜底流程。
 * @param  r 路由器上下文。
 * @note   断开后停止向 BLE 转发结果，并按顺序执行：
 *         1. 标记 BLE/主机不可用；
 *         2. 发送 STOP_ALL，要求 ACK；
 *         3. 约 40 ms 内未收到 ACK 时发送 DISABLE，要求 ACK。
 *         本流程与 HEARTBEAT/控制租约无关。
 */
void gateway_router_on_ble_disconnected(gateway_router_t *r);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_ROUTER_H */
