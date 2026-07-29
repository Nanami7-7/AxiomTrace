/**
 * @file    gateway_router.c
 * @brief   Board B 网关路由模块实现。
 * @note    Board B 负责 BLE/OLED Host 与 Board A 之间的协议转发：
 *          1. 重新组帧并重新计算 CRC；
 *          2. 管理事务、序号映射、超时和重试；
 *          3. 缓存状态并提供 OLED 数据源；
 *          4. BLE 物理断开时执行 STOP_ALL/DISABLE 安全停机。
 *
 *          当前版本已取消 HEARTBEAT 机制：
 *          - 不主动发送 HEARTBEAT；
 *          - 不要求 Host 先发送 HEARTBEAT；
 *          - 不使用 Host 控制租约和租约超时；
 *          - 收到旧版本 HEARTBEAT 时仅兼容接收，不转发、不改变控制状态。
 *
 *          所有多字节字段使用 little-endian，不分配堆内存。
 */
#include "gateway_router.h"
#include <string.h>

/* ========================================================================
 * 内部常量
 * ======================================================================== */
#define DEFAULT_MOTOR_COUNT  4u

/* ========================================================================
 * 内部工具 — 时间和临界区
 * ======================================================================== */

/**
 * @brief 获取网关当前时间，失败时返回 0
 * @param r 网关路由器上下文
 * @return 当前系统时间，单位为毫秒；无效上下文返回 0
 */
static uint32_t router_now_ms(const gateway_router_t *r)
{
    if (r != NULL && r->adapter != NULL && r->adapter->now_ms != NULL) {
        return r->adapter->now_ms();
    }
    return 0u;
}

/**
 * @brief 进入函数 router_enter_critical，完成对应模块的功能处理。
 * @param r 函数参数 r。
 * @return 函数执行结果。
 */
static void router_enter_critical(const gateway_router_t *r)
{
    if (r != NULL && r->adapter != NULL && r->adapter->enter_critical != NULL) {
        r->adapter->enter_critical();
    }
}

/**
 * @brief 退出函数 router_exit_critical，完成对应模块的功能处理。
 * @param r 函数参数 r。
 * @return 函数执行结果。
 */
static void router_exit_critical(const gateway_router_t *r)
{
    if (r != NULL && r->adapter != NULL && r->adapter->exit_critical != NULL) {
        r->adapter->exit_critical();
    }
}

/* ========================================================================
 * 内部工具 — 帧发送
 * ======================================================================== */

/**
 * @brief 构建并发送帧到 Board A
 * @param r           路由器上下文
 * @param msg_class   消息类别
 * @param opcode      操作码
 * @param flags       帧标志
 * @param seq         Board A 链路序号
 * @param payload     payload 指针 (payload_len=0 时可为 NULL)
 * @param payload_len payload 长度
 * @retval true  发送成功
 * @retval false 发送失败
 */
static bool send_to_board(gateway_router_t *r, uint8_t msg_class, uint8_t opcode,
                          uint8_t flags, uint16_t seq,
                          const uint8_t *payload, uint16_t payload_len)
{
    if (r == NULL || r->adapter == NULL) {
        return false;
    }
    if (payload_len > 0u && payload == NULL) {
        return false;
    }

    proto_frame_view_t frame = {
        .version     = PROTO_VERSION,
        .flags       = flags,
        .src         = PROTO_ADDR_GATEWAY,
        .dst         = PROTO_ADDR_CONTROLLER,
        .msg_class   = msg_class,
        .opcode      = opcode,
        .seq         = seq,
        .payload_len = payload_len,
        .payload     = payload,
    };

    uint8_t wire[PROTO_MAX_WIRE];
    size_t wire_len;

    if (!proto_frame_encode(&frame, wire, sizeof(wire), &wire_len)) {
        return false;
    }

    if (r->adapter->tx_board_link != NULL) {
        bool ok = r->adapter->tx_board_link(wire, wire_len);
        if (!ok) {
            /* 只记录发送失败，不在协议层重试，重试由事务/安全流程决定。 */
            r->board_tx_fail_count++;
        }
        return ok;
    }
    r->board_tx_fail_count++;
    return false;
}

/**
 * @brief 构建并发送帧到 Host/BLE
 * @param r           路由器上下文
 * @param msg_class   消息类别
 * @param opcode      操作码
 * @param flags       帧标志
 * @param seq         Host 链路序号
 * @param payload     payload 指针 (payload_len=0 时可为 NULL)
 * @param payload_len payload 长度
 * @retval true  发送成功
 * @retval false 发送失败
 */
static bool send_to_ble(gateway_router_t *r, uint8_t msg_class, uint8_t opcode,
                        uint8_t flags, uint16_t seq,
                        const uint8_t *payload, uint16_t payload_len)
{
    if (r == NULL || r->adapter == NULL) {
        return false;
    }
    if (payload_len > 0u && payload == NULL) {
        return false;
    }

    proto_frame_view_t frame = {
        .version     = PROTO_VERSION,
        .flags       = flags,
        .src         = PROTO_ADDR_GATEWAY,
        .dst         = PROTO_ADDR_HOST,
        .msg_class   = msg_class,
        .opcode      = opcode,
        .seq         = seq,
        .payload_len = payload_len,
        .payload     = payload,
    };

    uint8_t wire[PROTO_MAX_WIRE];
    size_t wire_len;

    if (!proto_frame_encode(&frame, wire, sizeof(wire), &wire_len)) {
        return false;
    }

    if (r->adapter->tx_ble != NULL) {
        return r->adapter->tx_ble(wire, wire_len);
    }
    return false;
}

/* ========================================================================
 * 内部工具 — STATUS payload 解析 (规范 §6.4)
 * @note  显式 LE 读取，严禁直接序列化 C struct
 * ======================================================================== */

/**
 * @brief 解析函数 parse_summary_payload，完成对应模块的功能处理。
 * @param p 函数参数 p。
 * @param out 函数参数 out。
 * @return 函数执行结果。
 */
static void parse_summary_payload(const uint8_t *p, proto_snapshot_summary_t *out)
{
    out->state             = p[0];
    out->motor_count       = p[1];
    out->active_motor_mask = proto_get_u16_le(&p[2]);
    out->fault_code        = proto_get_u16_le(&p[4]);
    out->safety_flags      = proto_get_u16_le(&p[6]);
    out->last_command_seq  = proto_get_u16_le(&p[8]);
    out->last_control_seq  = proto_get_u16_le(&p[10]);
    out->uptime_ms         = proto_get_u32_le(&p[12]);
    out->link_age_ms       = proto_get_u16_le(&p[16]);
    out->status_generation = proto_get_u16_le(&p[18]);
}

/**
 * @brief 解析函数 parse_motor_payload，完成对应模块的功能处理。
 * @param p 函数参数 p。
 * @param out 函数参数 out。
 * @return 函数执行结果。
 */
static void parse_motor_payload(const uint8_t *p, proto_snapshot_motor_t *out)
{
    out->motor_id           = p[0];
    out->motor_state        = p[1];
    out->mode               = p[2];
    out->power_enabled      = p[3];
    out->target_value       = proto_get_i32_le(&p[4]);
    out->speed_x100_rpm     = proto_get_i32_le(&p[8]);
    out->position_count     = proto_get_i32_le(&p[12]);
    out->current_ma         = proto_get_i32_le(&p[16]);
    out->voltage_mv         = proto_get_u16_le(&p[20]);
    out->temperature_x100_c = proto_get_i16_le(&p[22]);
    out->fault_code         = proto_get_u16_le(&p[24]);
}

/**
 * @brief 解析函数 parse_sensor_payload，完成对应模块的功能处理。
 * @param p 函数参数 p。
 * @param out 函数参数 out。
 * @return 函数执行结果。
 */
static void parse_sensor_payload(const uint8_t *p, proto_snapshot_sensor_t *out)
{
    out->sample_time_ms     = proto_get_u32_le(&p[0]);
    out->sensor_flags       = proto_get_u16_le(&p[4]);
    out->gyro_x_x1000_dps   = proto_get_i32_le(&p[6]);
    out->gyro_y_x1000_dps   = proto_get_i32_le(&p[10]);
    out->gyro_z_x1000_dps   = proto_get_i32_le(&p[14]);
    out->accel_x_x1000_mg   = proto_get_i32_le(&p[18]);
    out->accel_y_x1000_mg   = proto_get_i32_le(&p[22]);
    out->accel_z_x1000_mg   = proto_get_i32_le(&p[26]);
    out->encoder_count      = proto_get_i32_le(&p[30]);
    out->adc0_mv            = proto_get_u16_le(&p[34]);
    out->adc1_mv            = proto_get_u16_le(&p[36]);
    out->adc2_mv            = proto_get_u16_le(&p[38]);
    out->bus_voltage_mv     = proto_get_u16_le(&p[40]);
    out->motor_current_ma   = proto_get_u16_le(&p[42]);
    out->temperature_x100_c = proto_get_i16_le(&p[44]);
}

/**
 * @brief 解析函数 parse_fault_payload，完成对应模块的功能处理。
 * @param p 函数参数 p。
 * @param out 函数参数 out。
 * @return 函数执行结果。
 */
static void parse_fault_payload(const uint8_t *p, proto_snapshot_fault_t *out)
{
    out->fault_code     = proto_get_u16_le(&p[0]);
    out->fault_flags    = proto_get_u16_le(&p[2]);
    out->first_seen_ms  = proto_get_u32_le(&p[4]);
    out->last_seen_ms   = proto_get_u32_le(&p[8]);
    out->repeat_count   = proto_get_u16_le(&p[12]);
    out->active         = p[14];
    out->reserved       = p[15];
}

/* ========================================================================
 * 内部工具 — 命令 flags
 * ========================================================================
 */

/**
 * @brief 根据 opcode 推导发往 Board A 的帧 flags
 * @note  所有命令均设 ACK_REQ=1 (Board B 需要确认)。
 *        STOP/STOP_ALL/ABORT 额外设 IS_IDEMPOTENT (§8.3)。
 *        旧版 HEARTBEAT flags = 0x30（IS_PERIODIC | IS_IDEMPOTENT）；
 *        当前网关不会主动生成 HEARTBEAT。
 */
static uint8_t command_flags_for_opcode(uint8_t opcode)
{
    uint8_t flags = PROTO_FLAG_ACK_REQ;

    if (opcode == PROTO_OP_STOP || opcode == PROTO_OP_STOP_ALL ||
        opcode == PROTO_OP_ABORT) {
        flags |= PROTO_FLAG_IS_IDEMPOTENT;
    }

    return flags;
}

/**
 * @brief 查询响应中预期的 STATUS 帧数量
 * @param query_opcode QUERY 操作码
 * @param motor_count  电机数量
 * @retval 预期 STATUS 帧数 (不含 ACK)
 * @note   QUERY_STATUS: SUMMARY + N*MOTOR + SENSOR = motor_count + 2
 *         QUERY_INFO/FAULT/SENSOR: 1
 */
static uint8_t expected_status_count(uint8_t query_opcode, uint8_t motor_count)
{
    switch (query_opcode) {
    case PROTO_OP_QUERY_STATUS:
        return (uint8_t)(motor_count + 2u);  /* SUMMARY + N*MOTOR + SENSOR */
    case PROTO_OP_QUERY_INFO:
    case PROTO_OP_QUERY_FAULT:
    case PROTO_OP_QUERY_SENSOR:
        return 1u;
    default:
        return 0u;
    }
}

/* ========================================================================
 * 内部工具 — 本地 NACK 生成
 * ======================================================================== */

/**
 * @brief 向 Host 生成本地 NACK (Board A 无响应时)
 * @param r              路由器上下文
 * @param host_seq       Host 序号
 * @param original_opcode 原始命令 opcode
 * @param error_code     NACK 错误码 (proto_nack_code_t)
 * @param detail_code    NACK 详情码
 * @note  payload: original_seq(u16 LE) + original_opcode(u8) +
 *                 error_code(u8) + detail_code(u16 LE) + retry_after_ms(u16 LE)
 */
static void send_local_nack(gateway_router_t *r, uint16_t host_seq,
                             uint8_t original_opcode, uint8_t error_code,
                             uint16_t detail_code)
{
    uint8_t payload[PROTO_NACK_PAYLOAD_LEN];
    proto_put_u16_le(&payload[0], host_seq);          /* original_seq = host_seq */
    payload[2] = original_opcode;                      /* original_opcode */
    payload[3] = error_code;                           /* error_code */
    proto_put_u16_le(&payload[4], detail_code);        /* detail_code */
    proto_put_u16_le(&payload[6], 0u);                 /* retry_after_ms = 0 */

    send_to_ble(r, PROTO_MSG_NACK, PROTO_OP_ACK, PROTO_FLAG_IS_NACK,
                host_seq, payload, PROTO_NACK_PAYLOAD_LEN);
}

/* ========================================================================
 * 内部工具 — Host 入站语义校验
 * ======================================================================== */

/** @brief 判断是否为合法的 QUERY opcode。 */
static bool is_valid_query_opcode(uint8_t opcode)
{
    return (opcode >= PROTO_OP_QUERY_STATUS && opcode <= PROTO_OP_QUERY_SENSOR);
}

/**
 * @brief 获取 COMMAND 的固定 payload 长度。
 * @param opcode 命令 opcode
 * @param known  输出：opcode 是否属于已定义命令
 */
static uint16_t command_payload_len(uint8_t opcode, bool *known)
{
    if (known != NULL) {
        *known = true;
    }

    switch (opcode) {
    case PROTO_OP_STOP:
        return 2u;
    case PROTO_OP_STOP_ALL:
    case PROTO_OP_ABORT:
    case PROTO_OP_ENABLE:
    case PROTO_OP_DISABLE:
    case PROTO_OP_RUN:
        return 0u;
    case PROTO_OP_SET_TARGET:
        return 10u;
    case PROTO_OP_SET_MODE:
    case PROTO_OP_CLEAR_FAULT:
        return 2u;
    default:
        if (known != NULL) {
            *known = false;
        }
        return 0u;
    }
}

/**
 * @brief 判断断开兜底流程是否仍在执行。
 * @note STOP_ALL/DISABLE 已经发出后，不允许 Host 帧取消该流程。
 */
static bool disconnect_is_in_progress(const gateway_router_t *r)
{
    return (r != NULL &&
            (r->disconnect_state == GATEWAY_DISCONNECT_STOP_ALL_SENT ||
             r->disconnect_state == GATEWAY_DISCONNECT_DISABLE_SENT));
}

/**
 * @brief 严格校验来自 Host 的协议语义。
 * @param view        已通过长度、版本、地址和 CRC 校验的帧
 * @param error_code  输出 NACK error_code
 * @param detail_code 输出 NACK detail_code
 * @retval true  语义合法
 * @retval false 语义非法；调用者应向 Host 返回 NACK，且不得刷新租约
 */
static bool validate_host_request(const proto_frame_view_t *view,
                                  uint8_t *error_code,
                                  uint16_t *detail_code)
{
    if (view == NULL || error_code == NULL || detail_code == NULL) {
        return false;
    }

    *error_code = PROTO_NACK_BAD_CLASS;
    *detail_code = 0u;

    switch (view->msg_class) {
    case PROTO_MSG_HEARTBEAT:
        if (view->opcode != PROTO_OP_HEARTBEAT) {
            *error_code = PROTO_NACK_BAD_OPCODE;
            return false;
        }
        if (view->flags != PROTO_FLAGS_HEARTBEAT) {
            *error_code = PROTO_NACK_BAD_FLAGS;
            *detail_code = PROTO_DETAIL_BAD_FLAGS_CLASS_MISMATCH;
            return false;
        }
        if (view->payload_len != PROTO_HEARTBEAT_PAYLOAD_LEN ||
            view->payload == NULL) {
            *error_code = PROTO_NACK_BAD_LENGTH;
            *detail_code = PROTO_DETAIL_BAD_LENGTH_PAYLOAD;
            return false;
        }
        if (view->payload[2] > PROTO_MODE_ANGLE) {
            *error_code = PROTO_NACK_BAD_PARAM;
            *detail_code = PROTO_DETAIL_BAD_PARAM_MODE;
            return false;
        }
        if (view->payload[3] > 1u) {
            *error_code = PROTO_NACK_BAD_PARAM;
            /* v1 没有单独的 requested_run detail_code，使用 0 表示未细分。 */
            *detail_code = 0u;
            return false;
        }
        return true;

    case PROTO_MSG_COMMAND:
    {
        /* Board B 不解析用户 payload，只检查固定协议边界并透明转发。 */
        if (PROTO_IS_USER_COMMAND_OPCODE(view->opcode)) {
            if ((view->flags & PROTO_FLAG_RESERVED_MASK) != 0u ||
                view->flags != PROTO_FLAG_ACK_REQ) {
                *error_code = PROTO_NACK_BAD_FLAGS;
                *detail_code = ((view->flags & PROTO_FLAG_RESERVED_MASK) != 0u) ?
                               PROTO_DETAIL_BAD_FLAGS_RESERVED :
                               PROTO_DETAIL_BAD_FLAGS_CLASS_MISMATCH;
                return false;
            }
            if (view->payload_len > PROTO_MAX_PAYLOAD ||
                (view->payload_len > 0u && view->payload == NULL)) {
                *error_code = PROTO_NACK_BAD_LENGTH;
                *detail_code = PROTO_DETAIL_BAD_LENGTH_PAYLOAD;
                return false;
            }
            return true;
        }

        bool known;
        uint16_t expected_len = command_payload_len(view->opcode, &known);
        bool stop_command = (view->opcode == PROTO_OP_STOP ||
                             view->opcode == PROTO_OP_STOP_ALL ||
                             view->opcode == PROTO_OP_ABORT);
        uint8_t allowed_flags = PROTO_FLAG_ACK_REQ;

        if (!known) {
            *error_code = PROTO_NACK_BAD_OPCODE;
            return false;
        }
        if (stop_command) {
            allowed_flags |= PROTO_FLAG_IS_IDEMPOTENT;
        }
        if ((view->flags & PROTO_FLAG_RESERVED_MASK) != 0u) {
            *error_code = PROTO_NACK_BAD_FLAGS;
            *detail_code = PROTO_DETAIL_BAD_FLAGS_RESERVED;
            return false;
        }
        if ((view->flags & (uint8_t)~allowed_flags) != 0u ||
            (view->flags & PROTO_FLAG_ACK_REQ) == 0u) {
            *error_code = PROTO_NACK_BAD_FLAGS;
            *detail_code = PROTO_DETAIL_BAD_FLAGS_CLASS_MISMATCH;
            return false;
        }
        if (view->payload_len != expected_len ||
            (expected_len > 0u && view->payload == NULL)) {
            *error_code = PROTO_NACK_BAD_LENGTH;
            *detail_code = PROTO_DETAIL_BAD_LENGTH_PAYLOAD;
            return false;
        }
        return true;
    }

    case PROTO_MSG_QUERY:
        if (!is_valid_query_opcode(view->opcode)) {
            *error_code = PROTO_NACK_BAD_OPCODE;
            return false;
        }
        if (view->flags != PROTO_FLAG_ACK_REQ) {
            *error_code = PROTO_NACK_BAD_FLAGS;
            *detail_code = PROTO_DETAIL_BAD_FLAGS_CLASS_MISMATCH;
            return false;
        }
        if (view->payload_len != 0u) {
            *error_code = PROTO_NACK_BAD_LENGTH;
            *detail_code = PROTO_DETAIL_BAD_LENGTH_PAYLOAD;
            return false;
        }
        return true;

    default:
        /* Host 只能发 COMMAND、QUERY、HEARTBEAT；0x00 及其它类别非法。 */
        *error_code = PROTO_NACK_BAD_CLASS;
        return false;
    }
}


/* ========================================================================
 * 内部工具 — 事务超时回调
 * ======================================================================== */

/**
 * @brief 事务超时回调 (由 gateway_txn_tick 调用)
 * @param mgr       事务管理器
 * @param txn       超时的事务
 * @param can_retry true=可重试 (重发到 Board A)，false=不可重试 (本地 NACK 到 Host)
 * @note  通过 mgr->user_data 获取路由器上下文。
 *        重试时使用同一 board_seq 和相同 payload (§8.3)。
 */
static void txn_timeout_cb(gateway_txn_manager_t *mgr,
                            gateway_transaction_t *txn, bool can_retry)
{
    gateway_router_t *r = (gateway_router_t *)mgr->user_data;
    if (r == NULL) {
        return;
    }

    if (can_retry) {
        /* 重发到 Board A: 同一 board_seq, 同一 payload (§8.3) */
        uint8_t flags;
        if (txn->board_msg_class == PROTO_MSG_QUERY) {
            flags = PROTO_FLAG_ACK_REQ;
        } else {
            flags = command_flags_for_opcode(txn->board_opcode);
        }
        send_to_board(r, txn->board_msg_class, txn->board_opcode, flags,
                      txn->board_seq, txn->payload, txn->payload_len);
    } else {
        /* Board A 无响应：只有 BLE 上位机事务需要生成本地 NACK。 */
        if (txn->notify_host) {
            send_local_nack(r, txn->host_seq, txn->host_opcode,
                            PROTO_NACK_LINK_NOT_READY, PROTO_DETAIL_LINK_STALE);
            r->local_nack_count++;
        }
        gateway_txn_free(mgr, txn);
    }
}

/* ========================================================================
 * 内部工具 — 状态缓存更新
 * ======================================================================== */

/**
 * @brief 从 STATUS 帧更新状态缓存
 * @param r     路由器上下文
 * @param view  Board A STATUS 帧视图
 * @note  根据 opcode 解析 payload 并更新对应缓存项。
 *        同时更新 last_board_rx_ms。
 */
static void update_cache_from_status(gateway_router_t *r,
                                      const proto_frame_view_t *view)
{
    if (view->payload == NULL || view->payload_len == 0u) {
        return;
    }

    switch (view->opcode) {
    case PROTO_OP_STATUS_SUMMARY:
        if (view->payload_len >= PROTO_STATUS_SUMMARY_LEN) {
            proto_snapshot_summary_t summary;
            parse_summary_payload(view->payload, &summary);
            status_cache_update_summary(&r->cache, &summary);
        }
        break;

    case PROTO_OP_STATUS_MOTOR:
        if (view->payload_len >= PROTO_STATUS_MOTOR_LEN) {
            proto_snapshot_motor_t motor;
            parse_motor_payload(view->payload, &motor);
            status_cache_update_motor(&r->cache, motor.motor_id, &motor);
        }
        break;

    case PROTO_OP_STATUS_SENSOR:
        if (view->payload_len >= PROTO_STATUS_SENSOR_LEN) {
            proto_snapshot_sensor_t sensor;
            parse_sensor_payload(view->payload, &sensor);
            status_cache_update_sensor(&r->cache, &sensor);
        }
        break;

    case PROTO_OP_STATUS_FAULT:
        if (view->payload_len >= PROTO_STATUS_FAULT_LEN) {
            proto_snapshot_fault_t fault;
            parse_fault_payload(view->payload, &fault);
            status_cache_update_fault(&r->cache, &fault);
        }
        break;

    default:
        break;
    }

    r->cache.last_board_rx_ms = router_now_ms(r);
}

/* ========================================================================
 * 内部工具 — BLE 断开状态机 (§8.5)
 * ======================================================================== */

/**
 * @brief 启动断开流程: 发送 STOP_ALL (§8.5 步骤 3)
 * @param r       路由器上下文
 * @param now_ms  当前时间戳
 * @note  STOP_ALL flags = ACK_REQ | IS_IDEMPOTENT。
 *        使用 next_board_seq 分配序号 (不走事务表)。
 */
static void disconnect_start(gateway_router_t *r, uint32_t now_ms)
{
    /* 进入失联兜底流程后立即停止喂狗，最终由 Board A 判定 LINK_LOST。 */
    /* 分配 board_seq (不走事务表, 使用独立序号) */
    r->disconnect_seq = r->txn_mgr.next_board_seq++;

    /* 发送 STOP_ALL (ACK_REQ=1, IS_IDEMPOTENT=1) */
    uint8_t flags = PROTO_FLAG_ACK_REQ | PROTO_FLAG_IS_IDEMPOTENT;
    send_to_board(r, PROTO_MSG_COMMAND, PROTO_OP_STOP_ALL, flags,
                  r->disconnect_seq, NULL, 0u);

    r->disconnect_state    = GATEWAY_DISCONNECT_STOP_ALL_SENT;
    r->disconnect_sent_ms  = now_ms;
}

/**
 * @brief 断开流程兜底: 发送 DISABLE (§8.5 步骤 3)
 * @param r       路由器上下文
 * @param now_ms  当前时间戳
 * @note  DISABLE flags = ACK_REQ (非幂等)。
 *        使用新的 board_seq (§8.5: "使用新的 board_seq 发送一次 DISABLE")。
 */
static void disconnect_send_disable(gateway_router_t *r, uint32_t now_ms)
{
    /* 分配新的 board_seq */
    r->disconnect_seq = r->txn_mgr.next_board_seq++;

    /* 发送 DISABLE (ACK_REQ=1, 非幂等) */
    send_to_board(r, PROTO_MSG_COMMAND, PROTO_OP_DISABLE, PROTO_FLAG_ACK_REQ,
                  r->disconnect_seq, NULL, 0u);

    r->disconnect_state   = GATEWAY_DISCONNECT_DISABLE_SENT;
    r->disconnect_sent_ms = now_ms;
}

/**
 * @brief 断开流程 tick — 检查 ACK 超时并推进状态机
 * @param r      路由器上下文
 * @param now_ms 当前时间戳
 * @note  STOP_ALL_SENT: 40ms 无 ACK → 发送 DISABLE
 *        DISABLE_SENT: 40ms 无 ACK → DONE
 */
static void disconnect_tick(gateway_router_t *r, uint32_t now_ms)
{
    if (r->disconnect_state == GATEWAY_DISCONNECT_STOP_ALL_SENT) {
        uint32_t age = (uint32_t)(now_ms - r->disconnect_sent_ms);
        if (age >= PROTO_GATEWAY_ACK_TIMEOUT_MS) {
            /* STOP_ALL 无 ACK → 发送 DISABLE 兜底 */
            disconnect_send_disable(r, now_ms);
        }
    } else if (r->disconnect_state == GATEWAY_DISCONNECT_DISABLE_SENT) {
        uint32_t age = (uint32_t)(now_ms - r->disconnect_sent_ms);
        if (age >= PROTO_GATEWAY_ACK_TIMEOUT_MS) {
            /* DISABLE 无 ACK → 断开流程完成 */
            r->disconnect_state = GATEWAY_DISCONNECT_DONE;
        }
    }
}

/* ========================================================================
 * Host 帧处理
 * ======================================================================== */

/**
 * @brief 兼容接收 Host HEARTBEAT。
 * @note  当前版本已取消 HEARTBEAT 机制：不转发到 Board A，
 *        不建立控制租约，也不触发任何周期发送。保留该入口是为了
 *        兼容旧上位机，收到后直接忽略业务语义。
 */
static void handle_host_heartbeat(gateway_router_t *r,
                                  const proto_frame_view_t *view)
{
    (void)r;
    (void)view;
}

/**
 * @brief 处理 Host COMMAND。
 * @note  当前版本不使用控制租约，合法命令直接进入事务转发流程。
 *        BLE 断开安全停机流程进行期间，暂时拒绝新的控制命令。
 *        创建事务后重新组帧（src=GATEWAY，dst=CONTROLLER，seq=board_seq）发送到 Board A。
 */
static void handle_host_command(gateway_router_t *r,
                                 const proto_frame_view_t *view)
{
    /* 断开兜底流程期间不接受新的控制命令，防止
     * STOP_ALL/DISABLE 与恢复命令交错。流程完成后由下一条
     * 合法控制命令继续工作。 */
    if (disconnect_is_in_progress(r)) {
        send_local_nack(r, view->seq, view->opcode,
                        PROTO_NACK_LINK_NOT_READY, PROTO_DETAIL_LINK_STALE);
        r->local_nack_count++;
        return;
    }
    if (r->disconnect_state == GATEWAY_DISCONNECT_DONE) {
        r->disconnect_state = GATEWAY_DISCONNECT_IDLE;
    }

    /*
     * 已取消 HEARTBEAT 和 Host 控制租约门控：
     * 合法 COMMAND 直接进入统一事务转发流程。
     * BLE 物理断开时仍由 disconnect_start() 执行 STOP_ALL/DISABLE 兜底。
     */
    if (r->disconnect_state != GATEWAY_DISCONNECT_IDLE) {
        r->disconnect_state = GATEWAY_DISCONNECT_IDLE;
    }

    uint32_t now = router_now_ms(r);
    gateway_transaction_t *txn = gateway_txn_alloc(&r->txn_mgr);
    if (txn == NULL) {
        /* 事务表已满 → 本地 NACK (QUEUE_FULL) */
        send_local_nack(r, view->seq, view->opcode,
                        PROTO_NACK_QUEUE_FULL, 0u);
        r->local_nack_count++;
        return;
    }

    /* 填充事务字段 */
    txn->host_seq        = view->seq;
    txn->host_msg_class  = view->msg_class;
    txn->board_msg_class = PROTO_MSG_COMMAND;
    txn->host_opcode     = view->opcode;
    txn->board_opcode    = view->opcode;
    txn->notify_host      = true;
    txn->payload_len     = view->payload_len;
    if (view->payload_len > 0u && view->payload != NULL) {
        memcpy(txn->payload, view->payload, view->payload_len);
    }

    /* 重新组帧并发送到 Board A (§8.2) */
    uint8_t flags = command_flags_for_opcode(view->opcode);

    router_enter_critical(r);
    send_to_board(r, PROTO_MSG_COMMAND, view->opcode, flags,
                  txn->board_seq, txn->payload, txn->payload_len);
    gateway_txn_mark_sent(&r->txn_mgr, txn, now);
    router_exit_critical(r);
}

/**
 * @brief 处理 Host QUERY (§8.2, §8.3)
 * @note  QUERY 始终允许，不依赖 HEARTBEAT 或 Host 控制租约。
 *        创建事务 (初始 WAIT_BOARD_ACK)，ACK 后转为 WAIT_BOARD_STATUS。
 */
static void handle_host_query(gateway_router_t *r,
                               const proto_frame_view_t *view)
{
    uint32_t now = router_now_ms(r);

    /* 分配事务槽 */
    gateway_transaction_t *txn = gateway_txn_alloc(&r->txn_mgr);
    if (txn == NULL) {
        send_local_nack(r, view->seq, view->opcode,
                        PROTO_NACK_QUEUE_FULL, 0u);
        r->local_nack_count++;
        return;
    }

    /* 填充事务字段 */
    txn->host_seq        = view->seq;
    txn->host_msg_class  = view->msg_class;
    txn->board_msg_class = PROTO_MSG_QUERY;
    txn->host_opcode     = view->opcode;
    txn->board_opcode    = view->opcode;
    txn->notify_host      = true;
    txn->payload_len     = view->payload_len;
    if (view->payload_len > 0u && view->payload != NULL) {
        memcpy(txn->payload, view->payload, view->payload_len);
    }

    /* 重新组帧并发送到 Board A (§8.2) */
    router_enter_critical(r);
    send_to_board(r, PROTO_MSG_QUERY, view->opcode, PROTO_FLAG_ACK_REQ,
                  txn->board_seq, txn->payload, txn->payload_len);
    gateway_txn_mark_sent(&r->txn_mgr, txn, now);
    router_exit_critical(r);
}

/* ========================================================================
 * Board A 上行帧语义校验
 * ======================================================================== */

/**
 * @brief 校验 Board A 返回帧的类别、opcode、flags 和固定 payload 长度。
 * @note  该校验必须在更新缓存、事务匹配和转发 Host 之前完成。
 */
static bool is_valid_fault_code(uint16_t fault_code)
{
    return fault_code <= PROTO_FAULT_OVERTEMP;
}

static bool is_retryable_nack(uint8_t error_code)
{
    return error_code == PROTO_NACK_LINK_NOT_READY ||
           error_code == PROTO_NACK_QUEUE_FULL ||
           error_code == PROTO_NACK_BUSY;
}

static bool validate_status_fault_payload(const uint8_t *p)
{
    uint16_t fault_code;
    uint16_t fault_flags;

    if (p == NULL) {
        return false;
    }

    fault_code = proto_get_u16_le(&p[0]);
    fault_flags = proto_get_u16_le(&p[2]);
    if (!is_valid_fault_code(fault_code) ||
        (fault_flags & (uint16_t)~0x000Fu) != 0u ||
        p[14] > 1u || p[15] != 0u) {
        return false;
    }
    /* active 字段必须与 fault_flags.bit0 一致。 */
    return p[14] == ((fault_flags & PROTO_FAULT_FLAG_ACTIVE) ? 1u : 0u);
}

static bool validate_board_response(gateway_router_t *r,
                                    const proto_frame_view_t *view)
{
    if (view == NULL) {
        return false;
    }

    switch (view->msg_class) {
    case PROTO_MSG_ACK:
        if (view->opcode != PROTO_OP_ACK ||
            view->flags != PROTO_FLAG_IS_ACK ||
            view->payload_len != PROTO_ACK_PAYLOAD_LEN ||
            view->payload == NULL) {
            return false;
        }
        /* original_opcode=0x00 是非法值；result_code 仅允许 v1 三个值。 */
        if (view->payload[2] == 0u || view->payload[3] > PROTO_RESULT_ALREADY_IN_STATE) {
            return false;
        }
        return true;

    case PROTO_MSG_NACK:
    {
        uint8_t error_code;
        uint16_t retry_after_ms;

        if (view->opcode != PROTO_OP_ACK ||
            view->flags != PROTO_FLAG_IS_NACK ||
            view->payload_len != PROTO_NACK_PAYLOAD_LEN ||
            view->payload == NULL) {
            return false;
        }
        error_code = view->payload[3];
        retry_after_ms = proto_get_u16_le(&view->payload[6]);
        if (view->payload[2] == 0u ||
            error_code < PROTO_NACK_BAD_VERSION ||
            error_code > PROTO_NACK_BUSY ||
            retry_after_ms > PROTO_GATEWAY_MAX_RETRY_DELAY_MS) {
            return false;
        }
        /* 只有 LINK_NOT_READY/QUEUE_FULL/BUSY 允许携带重试建议。 */
        if (retry_after_ms != 0u && !is_retryable_nack(error_code)) {
            return false;
        }
        return true;
    }

    case PROTO_MSG_STATUS:
        if ((view->flags != 0x00u && view->flags != PROTO_FLAG_IS_PERIODIC) ||
            view->payload == NULL) {
            return false;
        }
        switch (view->opcode) {
        case PROTO_OP_STATUS_SUMMARY:
            if (view->payload_len != PROTO_STATUS_SUMMARY_LEN) {
                return false;
            }
            return view->payload[0] <= PROTO_STATE_LINK_LOST &&
                   is_valid_fault_code(proto_get_u16_le(&view->payload[4])) &&
                   (proto_get_u16_le(&view->payload[6]) & (uint16_t)~0x00FFu) == 0u;

        case PROTO_OP_STATUS_MOTOR:
            if (view->payload_len != PROTO_STATUS_MOTOR_LEN) {
                return false;
            }
            return view->payload[1] <= PROTO_MOTOR_DISABLED &&
                   view->payload[2] <= PROTO_MODE_ANGLE &&
                   view->payload[3] <= 1u &&
                   is_valid_fault_code(proto_get_u16_le(&view->payload[24]));

        case PROTO_OP_STATUS_SENSOR:
            if (view->payload_len != PROTO_STATUS_SENSOR_LEN) {
                return false;
            }
            return (proto_get_u16_le(&view->payload[4]) & (uint16_t)~0x03FFu) == 0u;

        case PROTO_OP_STATUS_FAULT:
            return view->payload_len == PROTO_STATUS_FAULT_LEN &&
                   validate_status_fault_payload(view->payload);

        case PROTO_OP_STATUS_INFO:
            if (view->payload_len != PROTO_STATUS_INFO_LEN) {
                return false;
            }
            /* device_type 允许厂商扩展；board_role 是封闭枚举。 */
            return (view->payload[1] == PROTO_BOARD_ROLE_CONTROLLER ||
                    view->payload[1] == PROTO_BOARD_ROLE_GATEWAY) &&
                   view->payload[2] == PROTO_VERSION &&
                   view->payload[7] == 0u &&
                   (proto_get_u32_le(&view->payload[8]) & ~0x000007FFu) == 0u;

        default:
            return false;
        }

    case PROTO_MSG_EVENT:
        if (view->flags != PROTO_FLAG_IS_EVENT ||
            view->payload == NULL) {
            return false;
        }
        switch (view->opcode) {
        case PROTO_OP_EVT_FAULT:
            return view->payload_len == PROTO_EVT_FAULT_LEN &&
                   validate_status_fault_payload(view->payload);

        case PROTO_OP_EVT_LINK_LOST:
            return view->payload_len == PROTO_EVT_LINK_LOST_LEN &&
                   is_valid_fault_code(proto_get_u16_le(&view->payload[4])) &&
                   proto_get_u16_le(&view->payload[6]) == 0u;

        case PROTO_OP_EVT_STATE_CHANGED:
            if (view->payload_len != PROTO_EVT_STATE_CHANGED_LEN ||
                view->payload[0] > PROTO_STATE_LINK_LOST ||
                view->payload[1] > PROTO_STATE_LINK_LOST ||
                view->payload[3] != 0u) {
                return false;
            }
            /* reason 的保留值按未知原因处理，但不得丢弃整个 EVENT。 */
            if (view->payload[2] > PROTO_REASON_INIT && r != NULL) {
                r->invalid_enum_count++;
            }
            return true;

        default:
            return false;
        }

    default:
        return false;
    }
}
/* ========================================================================
 * Board A 帧处理
 * ======================================================================== */

/**
 * @brief 处理 Board A ACK (§8.2)
 * @note  1. 检查是否为断开流程的 ACK (匹配 disconnect_seq)
 *        2. 匹配事务 (board_seq)
 *        3. 重新组帧: original_seq 改为 host_seq, src=GATEWAY, dst=HOST, seq=host_seq
 *        4. COMMAND 事务 → 标记完成并释放
 *           QUERY 事务 → 转为 WAIT_BOARD_STATUS, 设置预期 STATUS 帧数
 */
static bool disconnect_response_matches(const gateway_router_t *r,
                                           const proto_frame_view_t *view,
                                           uint8_t expected_opcode)
{
    if (r == NULL || view == NULL || view->payload == NULL ||
        view->seq != r->disconnect_seq) {
        return false;
    }

    /* ACK 和 NACK 都必须回显本次断开命令的 board_seq/opcode。 */
    if (view->msg_class == PROTO_MSG_ACK &&
        view->payload_len == PROTO_ACK_PAYLOAD_LEN) {
        return proto_get_u16_le(&view->payload[0]) == r->disconnect_seq &&
               view->payload[2] == expected_opcode;
    }

    if (view->msg_class == PROTO_MSG_NACK &&
        view->payload_len == PROTO_NACK_PAYLOAD_LEN) {
        return proto_get_u16_le(&view->payload[0]) == r->disconnect_seq &&
               view->payload[2] == expected_opcode;
    }

    return false;
}

static void handle_board_ack(gateway_router_t *r,
                              const proto_frame_view_t *view)
{
    /* 1. 检查断开流程 ACK */
    if (r->disconnect_state == GATEWAY_DISCONNECT_STOP_ALL_SENT) {
        if (disconnect_response_matches(r, view, PROTO_OP_STOP_ALL)) {
            /* STOP_ALL ACK：停止动作已被 Board A 接收，断开流程完成。 */
            r->disconnect_state = GATEWAY_DISCONNECT_DONE;
            return;
        }
    } else if (r->disconnect_state == GATEWAY_DISCONNECT_DISABLE_SENT) {
        if (disconnect_response_matches(r, view, PROTO_OP_DISABLE)) {
            /* DISABLE ACK：输出禁止命令已被 Board A 接收，流程完成。 */
            r->disconnect_state = GATEWAY_DISCONNECT_DONE;
            return;
        }
    }

    /* 2. 匹配事务 */
    gateway_transaction_t *txn = gateway_txn_find_by_board_seq(&r->txn_mgr,
                                                                view->seq);
    if (txn == NULL) {
        r->unmatched_response_count++;
        return;
    }

    /* 2.5 校验 ACK payload 字段 (规范 §6.5.3 步骤4) */
    if (view->payload_len == PROTO_ACK_PAYLOAD_LEN && view->payload != NULL) {
        uint16_t orig_seq    = proto_get_u16_le(&view->payload[0]);
        uint8_t  orig_opcode = view->payload[2];
        if (orig_seq != txn->board_seq || orig_opcode != txn->board_opcode) {
            r->unmatched_response_count++;
            return;
        }
    }

    /* 3. 修改 ACK payload: original_seq 从 board_seq 改为 host_seq (§8.2) */
    uint8_t ack_payload[PROTO_ACK_PAYLOAD_LEN];
    if (view->payload_len == PROTO_ACK_PAYLOAD_LEN && view->payload != NULL) {
        memcpy(ack_payload, view->payload, PROTO_ACK_PAYLOAD_LEN);
    } else {
        memset(ack_payload, 0, sizeof(ack_payload));
    }
    proto_put_u16_le(&ack_payload[0], txn->host_seq);

    /* 4. BLE 上位机命令需要回传 ACK；OLED 本地模拟命令不回传 BLE。 */
    if (txn->notify_host) {
        send_to_ble(r, PROTO_MSG_ACK, PROTO_OP_ACK, PROTO_FLAG_IS_ACK,
                    txn->host_seq, ack_payload, PROTO_ACK_PAYLOAD_LEN);
    }

    /* 5. 更新事务状态 */
    if (txn->board_msg_class == PROTO_MSG_QUERY) {
        /* QUERY: ACK 后等待 STATUS 帧 */
        txn->state = PROTO_TXN_WAIT_BOARD_STATUS;

        size_t idx = (size_t)(txn - r->txn_mgr.table);
        r->txn_status_expected[idx] = expected_status_count(txn->board_opcode,
                                                             r->motor_count);
        r->txn_status_received[idx] = 0u;
    } else {
        /* COMMAND: 完成 */
        gateway_txn_complete(&r->txn_mgr, txn);
        gateway_txn_free(&r->txn_mgr, txn);
    }
}

/**
 * @brief 处理 Board A NACK (§8.2, §8.3)
 * @note  1. 检查是否为断开流程的 NACK
 *        2. 匹配事务 (board_seq)
 *        3. 重新组帧: original_seq 改为 host_seq
 *        4. 发送到 Host, 标记失败并释放
 *        Board A 明确返回 NACK 时，Board B 不盲目重试 (§8.3)
 */
static void handle_board_nack(gateway_router_t *r,
                               const proto_frame_view_t *view)
{
    /* 1. 检查断开流程 NACK */
    if (r->disconnect_state == GATEWAY_DISCONNECT_STOP_ALL_SENT) {
        if (disconnect_response_matches(r, view, PROTO_OP_STOP_ALL)) {
            /* STOP_ALL 被明确拒绝，继续发送 DISABLE 兜底。 */
            uint32_t now = router_now_ms(r);
            disconnect_send_disable(r, now);
            return;
        }
    } else if (r->disconnect_state == GATEWAY_DISCONNECT_DISABLE_SENT) {
        if (disconnect_response_matches(r, view, PROTO_OP_DISABLE)) {
            /* DISABLE 也被拒绝，安全流程到此结束，等待 Board A 自身失联保护。 */
            r->disconnect_state = GATEWAY_DISCONNECT_DONE;
            return;
        }
    }

    /* 2. 匹配事务 */
    gateway_transaction_t *txn = gateway_txn_find_by_board_seq(&r->txn_mgr,
                                                                view->seq);
    if (txn == NULL) {
        r->unmatched_response_count++;
        return;
    }

    /* 2.5 校验 NACK payload 字段 (规范 §6.5.3 步骤4) */
    if (view->payload_len == PROTO_NACK_PAYLOAD_LEN && view->payload != NULL) {
        uint16_t orig_seq    = proto_get_u16_le(&view->payload[0]);
        uint8_t  orig_opcode = view->payload[2];
        if (orig_seq != txn->board_seq || orig_opcode != txn->board_opcode) {
            r->unmatched_response_count++;
            return;
        }
    }

    /* 3. 修改 NACK payload: original_seq 从 board_seq 改为 host_seq (§8.2) */
    uint8_t nack_payload[PROTO_NACK_PAYLOAD_LEN];
    if (view->payload_len == PROTO_NACK_PAYLOAD_LEN && view->payload != NULL) {
        memcpy(nack_payload, view->payload, PROTO_NACK_PAYLOAD_LEN);
    } else {
        memset(nack_payload, 0, sizeof(nack_payload));
    }
    proto_put_u16_le(&nack_payload[0], txn->host_seq);

    /* 4. BLE 上位机命令需要回传 NACK；OLED 本地模拟命令不回传 BLE。 */
    if (txn->notify_host) {
        send_to_ble(r, PROTO_MSG_NACK, PROTO_OP_ACK, PROTO_FLAG_IS_NACK,
                    txn->host_seq, nack_payload, PROTO_NACK_PAYLOAD_LEN);
    }

    /* 5. 标记失败并释放 (不重试, §8.3) */
    gateway_txn_fail(&r->txn_mgr, txn);
    gateway_txn_free(&r->txn_mgr, txn);
}

/**
 * @brief 处理 Board A STATUS (§8.2, §8.4)
 * @note  1. 始终更新状态缓存 (§8.4)
 *        2. 若匹配 WAIT_BOARD_STATUS 事务: 转发到 Host, 递增接收计数
 *        3. QUERY_STATUS 的 SUMMARY 帧到达时, 用实际 motor_count 更新预期计数
 *        4. 接收数 >= 预期数 → 标记完成并释放
 */
static void handle_board_status(gateway_router_t *r,
                                 const proto_frame_view_t *view)
{
    /* 1. 更新状态缓存 (始终执行, §8.4) */
    update_cache_from_status(r, view);

    /* 2. 匹配事务 */
    gateway_transaction_t *txn = gateway_txn_find_by_board_seq(&r->txn_mgr,
                                                                view->seq);
    if (txn == NULL) {
        /* 无匹配事务 (周期 STATUS 或超时后的响应) → 仅更新缓存 */
        return;
    }

    /* 仅 WAIT_BOARD_STATUS 状态转发 STATUS 到 Host */
    if (txn->state != PROTO_TXN_WAIT_BOARD_STATUS) {
        return;
    }

    /* 3. 转发 STATUS 到 Host (src=GATEWAY, dst=HOST, seq=host_seq) */
    send_to_ble(r, PROTO_MSG_STATUS, view->opcode, view->flags,
                txn->host_seq, view->payload, view->payload_len);

    /* 4. 递增接收计数 */
    size_t idx = (size_t)(txn - r->txn_mgr.table);
    r->txn_status_received[idx]++;

    /* 5. QUERY_STATUS: 用 SUMMARY 中的实际 motor_count 更新预期 */
    if (txn->board_opcode == PROTO_OP_QUERY_STATUS &&
        view->opcode == PROTO_OP_STATUS_SUMMARY &&
        view->payload_len >= 2u && view->payload != NULL) {
        uint8_t actual_motor_count = view->payload[1];
        r->txn_status_expected[idx] = (uint8_t)(actual_motor_count + 2u);
    }

    /* 6. 检查是否已收齐所有预期 STATUS 帧 */
    if (r->txn_status_received[idx] >= r->txn_status_expected[idx]) {
        gateway_txn_complete(&r->txn_mgr, txn);
        gateway_txn_free(&r->txn_mgr, txn);
    }
}

/**
 * @brief 处理 Board A EVENT (§8.2)
 * @note  EVT_FAULT 更新故障缓存，所有 EVENT 转发到 Host (使用 host_event_seq)。
 */
static void handle_board_event(gateway_router_t *r,
                                const proto_frame_view_t *view)
{
    /* EVT_FAULT: 更新故障缓存 */
    if (view->opcode == PROTO_OP_EVT_FAULT &&
        view->payload_len >= PROTO_EVT_FAULT_LEN && view->payload != NULL) {
        proto_snapshot_fault_t fault;
        parse_fault_payload(view->payload, &fault);
        status_cache_update_fault(&r->cache, &fault);
    }

    /* 转发 EVENT 到 Host (src=GATEWAY, dst=HOST, seq=host_event_seq) */
    send_to_ble(r, PROTO_MSG_EVENT, view->opcode, PROTO_FLAG_IS_EVENT,
                r->host_event_seq++, view->payload, view->payload_len);
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化函数 gateway_router_init，完成对应模块的功能处理。
 * @param r 函数参数 r。
 * @param adapter 函数参数 adapter。
 * @return 函数执行结果。
 */
void gateway_router_init(gateway_router_t *r,
                         const gateway_router_adapter_t *adapter)
{
    if (r == NULL) {
        return;
    }

    memset(r, 0, sizeof(*r));

    r->adapter     = adapter;
    r->motor_count = DEFAULT_MOTOR_COUNT;

    /* 保留兼容字段，但当前版本不使用 HEARTBEAT 和 Host 控制租约。 */
    r->heartbeat_active = false;
    r->requested_mode = PROTO_MODE_SPEED;
    r->requested_run = 0U;

    /* 初始化子模块 */
    gateway_txn_init(&r->txn_mgr);
    r->txn_mgr.user_data = r;  /* 供超时回调访问路由器上下文 */
    status_cache_init(&r->cache, r->motor_count);

    /* 序号从 1 开始 (推荐, §8.1) */
    r->heartbeat_seq   = 1u;
    r->host_status_seq = 1u;
    r->host_event_seq  = 1u;
    r->control_epoch   = 1u;
}

/**
 * @brief 执行函数 gateway_router_on_host_frame，完成对应模块的功能处理。
 * @param r 函数参数 r。
 * @param decoded 函数参数 decoded。
 * @param len 函数参数 len。
 * @return 函数执行结果。
 */
bool gateway_router_send_custom_command(gateway_router_t *r,
                                         uint8_t opcode,
                                         const uint8_t *payload,
                                         uint16_t payload_len)
{
    gateway_transaction_t *txn;
    uint32_t now;
    bool sent;

    if (r == NULL || r->adapter == NULL ||
        !PROTO_IS_USER_COMMAND_OPCODE(opcode) ||
        payload_len > PROTO_MAX_PAYLOAD ||
        (payload_len > 0u && payload == NULL)) {
        return false;
    }

    /* 该接口由 OLED 菜单任务调用，和网关任务共享事务表，必须保护临界区。 */
    router_enter_critical(r);
    txn = gateway_txn_alloc(&r->txn_mgr);
    if (txn == NULL) {
        router_exit_critical(r);
        return false;
    }

    now = router_now_ms(r);
    txn->host_seq        = 0u;
    txn->host_msg_class  = PROTO_MSG_INVALID;
    txn->board_msg_class = PROTO_MSG_COMMAND;
    txn->host_opcode     = opcode;
    txn->board_opcode    = opcode;
    txn->notify_host      = false;
    txn->payload_len     = payload_len;
    if (payload_len > 0u) {
        memcpy(txn->payload, payload, payload_len);
    }

    sent = send_to_board(r, PROTO_MSG_COMMAND, opcode, PROTO_FLAG_ACK_REQ,
                         txn->board_seq, txn->payload, txn->payload_len);
    if (sent) {
        gateway_txn_mark_sent(&r->txn_mgr, txn, now);
    } else {
        gateway_txn_free(&r->txn_mgr, txn);
    }
    router_exit_critical(r);
    return sent;
}
void gateway_router_on_host_frame(gateway_router_t *r,
                                  const uint8_t *decoded, size_t len)
{
    if (r == NULL || decoded == NULL) {
        return;
    }

    /* 1. 帧校验: src=HOST, dst=GATEWAY (§8.2) */
    proto_frame_view_t view;
    if (!proto_frame_validate(decoded, len,
                              PROTO_ADDR_HOST, PROTO_ADDR_GATEWAY, &view)) {
        return;  /* 校验失败: 静默丢弃 */
    }

    /* 2. 语义校验失败时返回 NACK；非法帧不得建立 Host 租约。 */
    uint8_t error_code;
    uint16_t detail_code;
    if (!validate_host_request(&view, &error_code, &detail_code)) {
        send_local_nack(r, view.seq, view.opcode, error_code, detail_code);
        r->local_nack_count++;
        return;
    }

    r->host_frames_received++;
    /* 合法帧才表示 Host/BLE 仍在线；QUERY 仍不刷新控制租约。 */
    r->ble_connected = true;

    /* 3. 按 msg_class 分发 */
    switch (view.msg_class) {
    case PROTO_MSG_HEARTBEAT:
        handle_host_heartbeat(r, &view);
        break;

    case PROTO_MSG_COMMAND:
        handle_host_command(r, &view);
        break;

    case PROTO_MSG_QUERY:
        handle_host_query(r, &view);
        break;

    default:
        /* 其他 msg_class (ACK/NACK/STATUS/EVENT) 不应来自 Host，忽略 */
        break;
    }
}

/**
 * @brief 执行函数 gateway_router_on_board_frame，完成对应模块的功能处理。
 * @param r 函数参数 r。
 * @param decoded 函数参数 decoded。
 * @param len 函数参数 len。
 * @return 函数执行结果。
 */
void gateway_router_on_board_frame(gateway_router_t *r,
                                   const uint8_t *decoded, size_t len)
{
    if (r == NULL || decoded == NULL) {
        return;
    }

    /* 1. 帧校验: src=CONTROLLER, dst=GATEWAY (§8.2) */
    proto_frame_view_t view;
    if (!proto_frame_validate(decoded, len,
                              PROTO_ADDR_CONTROLLER, PROTO_ADDR_GATEWAY, &view)) {
        return;  /* 校验失败: 静默丢弃 */
    }

    /* 2. 严格校验上行类别、opcode、flags 和 payload 长度。 */
    if (!validate_board_response(r, &view)) {
        r->malformed_board_frame_count++;
        return;
    }

    r->board_frames_received++;

    /* 3. 按 msg_class 分发 */
    switch (view.msg_class) {
    case PROTO_MSG_ACK:
        handle_board_ack(r, &view);
        break;

    case PROTO_MSG_NACK:
        handle_board_nack(r, &view);
        break;

    case PROTO_MSG_STATUS:
        handle_board_status(r, &view);
        break;

    case PROTO_MSG_EVENT:
        handle_board_event(r, &view);
        break;

    default:
        /* 其他 msg_class 不应来自 Board A，忽略 */
        break;
    }
}

/**
 * @brief 执行周期处理函数 gateway_router_tick，完成对应模块的功能处理。
 * @param r 函数参数 r。
 * @param now_ms 函数参数 now_ms。
 * @return 函数执行结果。
 */
void gateway_router_tick(gateway_router_t *r, uint32_t now_ms)
{
    if (r == NULL) {
        return;
    }

    /*
     * 已取消 Host 控制租约和周期 HEARTBEAT。
     * 普通 COMMAND/QUERY 直接按事务表转发；BLE 物理断开仍单独执行安全停机。
     */

    /* 3. BLE 断开状态机推进 (§8.5) */
    disconnect_tick(r, now_ms);

    /* 4. 事务超时检查 (重试或本地 NACK, §8.3) */
    gateway_txn_tick(&r->txn_mgr, now_ms, txn_timeout_cb);

    /* 5. 同步统计到缓存 (§8.4) */
    r->cache.timeout_count = r->txn_mgr.timeout_count;
    r->cache.retry_count   = r->txn_mgr.retry_count;
}

/**
 * @brief 执行函数 gateway_router_on_ble_connected，完成对应模块的功能处理。
 * @param r 函数参数 r。
 * @return 函数执行结果。
 */
void gateway_router_on_ble_connected(gateway_router_t *r)
{
    if (r == NULL) {
        return;
    }

    r->ble_connected = true;
    /* 当前版本不使用 HEARTBEAT/控制租约，BLE 连接后即可转发合法协议帧。 */
}

/**
 * @brief 执行函数 gateway_router_on_ble_disconnected，完成对应模块的功能处理。
 * @param r 函数参数 r。
 * @return 函数执行结果。
 */
void gateway_router_on_ble_disconnected(gateway_router_t *r)
{
    if (r == NULL) {
        return;
    }

    uint32_t now = router_now_ms(r);

    /* 1. 标记 BLE 断开 (§8.5) */
    r->ble_connected = false;
    r->lease_valid   = false;

    /* BLE 断开时仍执行 STOP_ALL/DISABLE 安全停机；不再处理 HEARTBEAT。 */
    r->requested_run = 0U;

    /* 3. 启动断开流程: STOP_ALL → DISABLE (§8.5 步骤 3) */
    if (r->disconnect_state == GATEWAY_DISCONNECT_IDLE ||
        r->disconnect_state == GATEWAY_DISCONNECT_DONE) {
        disconnect_start(r, now);
    }
}
