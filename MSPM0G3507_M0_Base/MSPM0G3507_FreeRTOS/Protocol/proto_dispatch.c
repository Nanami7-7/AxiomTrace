/**
 * @file    proto_dispatch.c
 * @brief   命令分发器 + 安全状态机 + ACK/NACK 生成实现 (规范 §2.2, §6, §7)
 * @note    P2 范围: STOP / STOP_ALL / ABORT / DISABLE / CLEAR_FAULT + HEARTBEAT
 *          P3 范围: ENABLE / RUN / SET_TARGET / SET_MODE + HEARTBEAT 增强
 *          P4 范围: QUERY_INFO / QUERY_STATUS / QUERY_FAULT / QUERY_SENSOR + STATUS 帧
 *          P5 范围: EVT_FAULT / EVT_STATE_CHANGED + 周期 STATUS + 状态转换事件
 *
 * 状态转换规则 (规范 §7.2):
 *   RUNNING  -> ENABLED   : STOP / ABORT
 *   RUNNING  -> DISABLED  : DISABLE / STOP_ALL
 *   ENABLED  -> DISABLED  : DISABLE / STOP_ALL
 *   FAULT    -> DISABLED  : CLEAR_FAULT 成功且故障已消失
 *   LINK_LOST-> DISABLED  : HEARTBEAT 后 CLEAR_FAULT(FAULT_LINK_LOST) 成功
 *   任意->LINK_LOST       : watchdog 超时 (FAULT 状态除外)
 *
 * LINK_LOST 恢复 (规范 §7.2.1):
 *   1. HEARTBEAT (刷新 watchdog，标记 link_recovery_started，不改变状态)
 *   2. CLEAR_FAULT(FAULT_LINK_LOST 或 0x0000)
 *   3. 满足条件时 LINK_LOST -> DISABLED
 *
 * 通过适配器回调与硬件解耦，不直接访问寄存器。不分配堆内存。
 */
#include "proto_dispatch.h"
#include "proto_cobs.h"
#include <string.h>

/* ========================================================================
 * 内部常量
 * ======================================================================== */
#define BOARD_A_ADDR          PROTO_ADDR_CONTROLLER
#define DEFAULT_MOTOR_COUNT   4u

/* ========================================================================
 * 内部工具
 * ======================================================================== */

/**
 * @brief ?????????????
 * @param ctx ?????????
 * @return ???????????
 */
static uint32_t dispatch_now_ms(const proto_dispatch_ctx_t *ctx)
{
    if (ctx != NULL && ctx->adapter != NULL && ctx->adapter->now_ms != NULL) {
        return ctx->adapter->now_ms();
    }
    return 0u;
}

/**
 * @brief 分发函数 dispatch_enter_critical，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @return 函数执行结果。
 */
static void dispatch_enter_critical(const proto_dispatch_ctx_t *ctx)
{
    if (ctx != NULL && ctx->adapter != NULL && ctx->adapter->enter_critical != NULL) {
        ctx->adapter->enter_critical();
    }
}

/**
 * @brief 分发函数 dispatch_exit_critical，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @return 函数执行结果。
 */
static void dispatch_exit_critical(const proto_dispatch_ctx_t *ctx)
{
    if (ctx != NULL && ctx->adapter != NULL && ctx->adapter->exit_critical != NULL) {
        ctx->adapter->exit_critical();
    }
}

/**
 * @brief 刷新函数 refresh_watchdog，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param seq 函数参数 seq。
 * @return 函数执行结果。
 */
static void refresh_watchdog(proto_dispatch_ctx_t *ctx, uint16_t seq)
{
    uint32_t now = dispatch_now_ms(ctx);
    proto_watchdog_refresh(&ctx->watchdog, now, seq);
    ctx->last_control_seq = seq;
}

/* ========================================================================
 * 响应帧发送 — 构建逻辑帧 + COBS 编码 + 发送 + (可选)重放缓存
 * ======================================================================== */

/**
 * @brief 构建并发送一帧响应 (ACK/NACK/EVENT)
 * @param ctx          分发器上下文
 * @param frame        响应帧视图 (payload 指针必须有效)
 * @param cache_req    若非 NULL，将逻辑帧存入重放缓存 (键=cache_req)
 */
static void send_frame(proto_dispatch_ctx_t *ctx,
                       const proto_frame_view_t *frame,
                       const proto_frame_view_t *cache_req)
{
    uint8_t logical[PROTO_MAX_DECODED];
    uint8_t wire[PROTO_MAX_WIRE];
    size_t  logical_len;
    size_t  cobs_len;

    if (!proto_frame_build_logical(frame, logical, sizeof(logical), &logical_len)) {
        if (ctx->stats != NULL) { ctx->stats->tx_dropped++; }
        return;
    }

    if (!proto_cobs_encode(logical, logical_len,
                           &wire[0], sizeof(wire) - 1u, &cobs_len)) {
        if (ctx->stats != NULL) { ctx->stats->tx_dropped++; }
        return;
    }
    wire[cobs_len] = PROTO_DELIMITER;

    if (ctx->adapter != NULL && ctx->adapter->tx_send != NULL) {
        ctx->adapter->tx_send(wire, cobs_len + 1u);
    }
    if (ctx->stats != NULL) {
        ctx->stats->tx_frames++;
    }

    if (cache_req != NULL) {
        proto_replay_store(&ctx->replay, cache_req, logical, logical_len);
    }
}

/* ========================================================================
 * ACK 发送
 * ======================================================================== */

/**
 * @brief 发送 ACK 响应
 * @param ctx         分发器上下文
 * @param req         原始请求帧视图
 * @param result_code proto_ack_result_t (ACCEPTED / COMPLETED / ALREADY_IN_STATE)
 * @param cache       是否存入重放缓存
 */
static void send_ack(proto_dispatch_ctx_t *ctx,
                     const proto_frame_view_t *req,
                     uint8_t result_code, bool cache)
{
    uint8_t payload[PROTO_ACK_PAYLOAD_LEN];
    proto_put_u16_le(&payload[0], req->seq);             /* original_seq */
    payload[2] = req->opcode;                            /* original_opcode */
    payload[3] = result_code;                            /* result_code */
    proto_put_u16_le(&payload[4], ctx->queue_generation);/* queue_generation */

    proto_frame_view_t resp = {
        .version     = PROTO_VERSION,
        .flags       = PROTO_FLAG_IS_ACK,
        .src         = BOARD_A_ADDR,
        .dst         = req->src,
        .msg_class   = PROTO_MSG_ACK,
        .opcode      = PROTO_OP_ACK,
        .seq         = req->seq,
        .payload_len = PROTO_ACK_PAYLOAD_LEN,
        .payload     = payload,
    };

    send_frame(ctx, &resp, cache ? req : NULL);
}

/* ========================================================================
 * NACK 发送
 * ======================================================================== */

/**
 * @brief 发送 NACK 响应
 * @param ctx            分发器上下文
 * @param req            原始请求帧视图
 * @param error_code     proto_nack_code_t
 * @param detail_code    补充详情 (proto_types.h PROTO_DETAIL_* 宏)
 * @param retry_after_ms 建议重试等待 (0=不指定)
 * @param cache          是否存入重放缓存
 */
static void send_nack(proto_dispatch_ctx_t *ctx,
                      const proto_frame_view_t *req,
                      uint8_t error_code, uint16_t detail_code,
                      uint16_t retry_after_ms, bool cache)
{
    uint8_t payload[PROTO_NACK_PAYLOAD_LEN];
    proto_put_u16_le(&payload[0], req->seq);             /* original_seq */
    payload[2] = req->opcode;                            /* original_opcode */
    payload[3] = error_code;                             /* error_code */
    proto_put_u16_le(&payload[4], detail_code);          /* detail_code */
    proto_put_u16_le(&payload[6], retry_after_ms);       /* retry_after_ms */

    proto_frame_view_t resp = {
        .version     = PROTO_VERSION,
        .flags       = PROTO_FLAG_IS_NACK,
        .src         = BOARD_A_ADDR,
        .dst         = req->src,
        .msg_class   = PROTO_MSG_NACK,
        .opcode      = PROTO_OP_ACK,
        .seq         = req->seq,
        .payload_len = PROTO_NACK_PAYLOAD_LEN,
        .payload     = payload,
    };

    send_frame(ctx, &resp, cache ? req : NULL);
}

/* ========================================================================
 * EVENT 发送 — 异步事件帧 (不存入重放缓存)
 * ======================================================================== */

/**
 * @brief 发送 EVENT_LINK_LOST 事件帧 (规范 §6.6, §7.3)
 * @param ctx 分发器上下文
 * @note  payload 8 字节 (规范 §6.6):
 *          last_valid_seq:u16 + age_ms:u16(饱和) + fault_code:u16 + reserved:u16
 */
static void send_event_link_lost(proto_dispatch_ctx_t *ctx)
{
    uint8_t payload[8];
    uint32_t now = dispatch_now_ms(ctx);
    uint32_t age = proto_watchdog_age_ms(&ctx->watchdog, now);

    proto_put_u16_le(&payload[0], ctx->watchdog.last_control_seq);            /* last_valid_seq */
    proto_put_u16_le(&payload[2], (uint16_t)(age > 0xFFFFu ? 0xFFFFu : age)); /* age_ms (u16 饱和, §3.7) */
    proto_put_u16_le(&payload[4], PROTO_FAULT_LINK_LOST);                     /* fault_code */
    proto_put_u16_le(&payload[6], 0x0000u);                                   /* reserved */

    proto_frame_view_t evt = {
        .version     = PROTO_VERSION,
        .flags       = PROTO_FLAG_IS_EVENT,
        .src         = BOARD_A_ADDR,
        .dst         = PROTO_ADDR_HOST,
        .msg_class   = PROTO_MSG_EVENT,
        .opcode      = PROTO_OP_EVT_LINK_LOST,
        .seq         = ctx->event_seq++,
        .payload_len = 8,
        .payload     = payload,
    };

    send_frame(ctx, &evt, NULL);
}

/* ========================================================================
 * EVENT 发送 — EVT_FAULT (规范 §6.6)
 * ======================================================================== */

/**
 * @brief 发送 EVT_FAULT 事件帧 (payload 使用 STATUS_FAULT 格式，16 字节)
 * @param ctx     分发器上下文
 * @param fault   故障快照 (若 NULL，通过 adapter 获取)
 */
static void send_event_fault(proto_dispatch_ctx_t *ctx,
                             const proto_snapshot_fault_t *fault)
{
    proto_snapshot_fault_t fs;
    if (fault != NULL) {
        fs = *fault;
    } else {
        if (ctx->adapter == NULL || ctx->adapter->status_get_fault == NULL) {
            return;
        }
        ctx->adapter->status_get_fault(&fs);
    }

    uint8_t payload[PROTO_EVT_FAULT_LEN];
    proto_put_u16_le(&payload[0],  fs.fault_code);
    proto_put_u16_le(&payload[2],  fs.fault_flags);
    proto_put_u32_le(&payload[4],  fs.first_seen_ms);
    proto_put_u32_le(&payload[8],  fs.last_seen_ms);
    proto_put_u16_le(&payload[12], fs.repeat_count);
    payload[14] = fs.active;
    payload[15] = fs.reserved;

    proto_frame_view_t evt = {
        .version     = PROTO_VERSION,
        .flags       = PROTO_FLAG_IS_EVENT,
        .src         = BOARD_A_ADDR,
        .dst         = PROTO_ADDR_HOST,
        .msg_class   = PROTO_MSG_EVENT,
        .opcode      = PROTO_OP_EVT_FAULT,
        .seq         = ctx->event_seq++,
        .payload_len = PROTO_EVT_FAULT_LEN,
        .payload     = payload,
    };

    send_frame(ctx, &evt, NULL);
}

/* ========================================================================
 * EVENT 发送 — EVT_STATE_CHANGED (规范 §6.6)
 * ======================================================================== */

/**
 * @brief 发送 EVT_STATE_CHANGED 事件帧 (payload 4 字节)
 * @param ctx       分发器上下文
 * @param old_state 转换前状态
 * @param new_state 转换后状态
 * @param reason    转换原因 (proto_state_reason_t)
 */
static void send_event_state_changed(proto_dispatch_ctx_t *ctx,
                                     proto_state_t old_state,
                                     proto_state_t new_state,
                                     uint8_t reason)
{
    uint8_t payload[PROTO_EVT_STATE_CHANGED_LEN];
    payload[0] = (uint8_t)old_state;
    payload[1] = (uint8_t)new_state;
    payload[2] = reason;
    payload[3] = 0x00u;  /* reserved */

    proto_frame_view_t evt = {
        .version     = PROTO_VERSION,
        .flags       = PROTO_FLAG_IS_EVENT,
        .src         = BOARD_A_ADDR,
        .dst         = PROTO_ADDR_HOST,
        .msg_class   = PROTO_MSG_EVENT,
        .opcode      = PROTO_OP_EVT_STATE_CHANGED,
        .seq         = ctx->event_seq++,
        .payload_len = PROTO_EVT_STATE_CHANGED_LEN,
        .payload     = payload,
    };

    send_frame(ctx, &evt, NULL);
}

/**
 * @brief 带事件通知的状态转换
 * @note  若新旧状态不同，发送 EVT_STATE_CHANGED；
 *        调用方在转换前已完成安全动作 (停机/使能等)。
 */
static void transition_state(proto_dispatch_ctx_t *ctx,
                             proto_state_t new_state, uint8_t reason)
{
    proto_state_t old = ctx->state;
    if (old == new_state) {
        return;
    }
    ctx->state = new_state;
    send_event_state_changed(ctx, old, new_state, reason);
}

/* ========================================================================
 * STATUS payload 构建器 — 显式 LE 编码 (规范 §6.4)
 * ======================================================================== */

/**
 * @brief 构建 STATUS_SUMMARY payload (20 字节)
 */
static void build_summary_payload(const proto_snapshot_summary_t *s,
                                  uint8_t *out)
{
    out[0]  = s->state;
    out[1]  = s->motor_count;
    proto_put_u16_le(&out[2],  s->active_motor_mask);
    proto_put_u16_le(&out[4],  s->fault_code);
    proto_put_u16_le(&out[6],  s->safety_flags);
    proto_put_u16_le(&out[8],  s->last_command_seq);
    proto_put_u16_le(&out[10], s->last_control_seq);
    proto_put_u32_le(&out[12], s->uptime_ms);
    proto_put_u16_le(&out[16], s->link_age_ms);
    proto_put_u16_le(&out[18], s->status_generation);
}

/**
 * @brief 构建 STATUS_MOTOR payload (26 字节)
 */
static void build_motor_payload(const proto_snapshot_motor_t *m,
                                uint8_t *out)
{
    out[0]  = m->motor_id;
    out[1]  = m->motor_state;
    out[2]  = m->mode;
    out[3]  = m->power_enabled;
    proto_put_i32_le(&out[4],  m->target_value);
    proto_put_i32_le(&out[8],  m->speed_x100_rpm);
    proto_put_i32_le(&out[12], m->position_count);
    proto_put_i32_le(&out[16], m->current_ma);
    proto_put_u16_le(&out[20], m->voltage_mv);
    proto_put_i16_le(&out[22], m->temperature_x100_c);
    proto_put_u16_le(&out[24], m->fault_code);
}

/**
 * @brief 构建 STATUS_SENSOR payload (46 字节)
 */
static void build_sensor_payload(const proto_snapshot_sensor_t *s,
                                 uint8_t *out)
{
    proto_put_u32_le(&out[0],  s->sample_time_ms);
    proto_put_u16_le(&out[4],  s->sensor_flags);
    proto_put_i32_le(&out[6],  s->gyro_x_x1000_dps);
    proto_put_i32_le(&out[10], s->gyro_y_x1000_dps);
    proto_put_i32_le(&out[14], s->gyro_z_x1000_dps);
    proto_put_i32_le(&out[18], s->accel_x_x1000_mg);
    proto_put_i32_le(&out[22], s->accel_y_x1000_mg);
    proto_put_i32_le(&out[26], s->accel_z_x1000_mg);
    proto_put_i32_le(&out[30], s->encoder_count);
    proto_put_u16_le(&out[34], s->adc0_mv);
    proto_put_u16_le(&out[36], s->adc1_mv);
    proto_put_u16_le(&out[38], s->adc2_mv);
    proto_put_u16_le(&out[40], s->bus_voltage_mv);
    proto_put_u16_le(&out[42], s->motor_current_ma);
    proto_put_i16_le(&out[44], s->temperature_x100_c);
}

/**
 * @brief 构建 STATUS_FAULT payload (16 字节)
 */
static void build_fault_payload(const proto_snapshot_fault_t *f,
                                uint8_t *out)
{
    proto_put_u16_le(&out[0],  f->fault_code);
    proto_put_u16_le(&out[2],  f->fault_flags);
    proto_put_u32_le(&out[4],  f->first_seen_ms);
    proto_put_u32_le(&out[8],  f->last_seen_ms);
    proto_put_u16_le(&out[12], f->repeat_count);
    out[14] = f->active;
    out[15] = f->reserved;
}

/**
 * @brief 构建 STATUS_INFO payload (16 字节)
 */
static void build_info_payload(const proto_snapshot_info_t *i,
                               uint8_t *out)
{
    out[0]  = i->device_type;
    out[1]  = i->board_role;
    out[2]  = i->protocol_version;
    out[3]  = i->hardware_revision;
    out[4]  = i->firmware_major;
    out[5]  = i->firmware_minor;
    out[6]  = i->firmware_patch;
    out[7]  = i->reserved;
    proto_put_u32_le(&out[8],  i->capability_flags);
    proto_put_u32_le(&out[12], i->serial_number);
}

/* ========================================================================
 * STATUS 帧发送 — 构建完整 STATUS 帧 (msg_class=0x03)
 * ======================================================================== */

/**
 * @brief 发送一个 STATUS 帧
 * @param ctx       分发器上下文
 * @param opcode    STATUS opcode (STATUS_SUMMARY / STATUS_MOTOR / ...)
 * @param payload   已构建的 payload 数据
 * @param payload_len payload 长度
 * @param seq       帧序号 (QUERY 响应用请求 seq；周期 STATUS 用独立 seq)
 * @param flags     帧 flags (周期 STATUS 用 IS_PERIODIC；QUERY 响应用 0x00)
 * @param dst       目的地址
 */
static void send_status_frame(proto_dispatch_ctx_t *ctx,
                              uint8_t opcode,
                              const uint8_t *payload, uint16_t payload_len,
                              uint16_t seq, uint8_t flags, uint8_t dst)
{
    proto_frame_view_t resp = {
        .version     = PROTO_VERSION,
        .flags       = flags,
        .src         = BOARD_A_ADDR,
        .dst         = dst,
        .msg_class   = PROTO_MSG_STATUS,
        .opcode      = opcode,
        .seq         = seq,
        .payload_len = payload_len,
        .payload     = payload,
    };

    send_frame(ctx, &resp, NULL);  /* STATUS 不存入重放缓存 */
}

/* ========================================================================
 * 快照采集工具 — 从 adapter 获取一致快照
 * ======================================================================== */

/**
 * @brief 采集 SUMMARY 快照并填充协议层字段
 * @note   递增 status_generation，填充 state/uptime/link_age 等
 */
static bool collect_summary(proto_dispatch_ctx_t *ctx,
                            proto_snapshot_summary_t *out)
{
    if (ctx->adapter == NULL || ctx->adapter->status_get_summary == NULL) {
        return false;
    }

    ctx->adapter->status_get_summary(out);

    /* 协议层填充 */
    out->state             = (uint8_t)ctx->state;
    out->motor_count       = ctx->motor_count;
    out->last_command_seq  = ctx->last_command_seq;
    out->last_control_seq  = ctx->last_control_seq;

    uint32_t now = dispatch_now_ms(ctx);
    uint32_t age = proto_watchdog_age_ms(&ctx->watchdog, now);
    out->link_age_ms = (uint16_t)(age > 0xFFFFu ? 0xFFFFu : age);

    if (ctx->adapter->get_uptime_ms != NULL) {
        out->uptime_ms = ctx->adapter->get_uptime_ms();
    } else {
        out->uptime_ms = now;
    }

    /* safety_flags 由 adapter 填充，补充协议层已知位 */
    if (ctx->watchdog.watchdog_armed) {
        out->safety_flags |= PROTO_SAFETY_WATCHDOG_ARMED;
    }
    if (ctx->watchdog.link_lost_latched) {
        out->safety_flags |= PROTO_SAFETY_LINK_LOST_LATCH;
    }
    if (ctx->state == PROTO_STATE_RUNNING || ctx->state == PROTO_STATE_ENABLED) {
        out->safety_flags |= PROTO_SAFETY_MOTOR_OUT_EN;
    }
    if (ctx->state == PROTO_STATE_FAULT) {
        out->safety_flags |= PROTO_SAFETY_HW_FAULT;
    }

    out->status_generation = ctx->status_generation;
    return true;
}

/** 状态是否允许接受控制命令 (非 LINK_LOST / INIT) */
static bool state_accepts_commands(proto_state_t s)
{
    return (s != PROTO_STATE_LINK_LOST && s != PROTO_STATE_INIT);
}

/* ========================================================================
 * 命令处理: STOP — 停止单电机 (规范 §6.1)
 * ======================================================================== */

/**
 * @brief 停止函数 cmd_stop，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void cmd_stop(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 校验 */
    if (req->payload_len != 2u || req->payload == NULL) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }
    uint8_t motor_id  = req->payload[0];
    uint8_t stop_type = req->payload[1];
    if (motor_id >= ctx->motor_count) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_MOTOR_ID, 0, true);
        return;
    }
    if (stop_type > PROTO_STOP_TYPE_FAST_SAFETY) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_STOP_TYPE, 0, true);
        return;
    }

    /* 2. 状态检查 */
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 3. 执行: RUNNING -> ENABLED，其他 -> ALREADY_IN_STATE */
    if (ctx->state == PROTO_STATE_RUNNING) {
        if (ctx->adapter != NULL && ctx->adapter->motor_stop != NULL) {
            ctx->adapter->motor_stop(motor_id, stop_type);
        }
        transition_state(ctx, PROTO_STATE_ENABLED, PROTO_REASON_COMMAND);
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
    } else {
        /* SAFE / DISABLED / ENABLED / FAULT — 电机已停 */
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
    }
}

/* ========================================================================
 * 命令处理: STOP_ALL — 停止全部电机并关闭输出 (规范 §6.1, §6.1 STOP_ALL语义)
 * ======================================================================== */

/**
 * @brief 停止函数 cmd_stop_all，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void cmd_stop_all(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 校验 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. 状态检查 */
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 3. 执行: RUNNING/ENABLED -> DISABLED，其他 -> ALREADY_IN_STATE */
    if (ctx->state == PROTO_STATE_RUNNING || ctx->state == PROTO_STATE_ENABLED) {
        if (ctx->adapter != NULL) {
            if (ctx->adapter->motor_stop_all != NULL) {
                ctx->adapter->motor_stop_all();
            }
            if (ctx->adapter->motor_disable_output != NULL) {
                ctx->adapter->motor_disable_output();
            }
        }
        transition_state(ctx, PROTO_STATE_DISABLED, PROTO_REASON_COMMAND);
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
    } else {
        /* SAFE / DISABLED / FAULT — 已停且已断电 */
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
    }
}

/* ========================================================================
 * 命令处理: ABORT — 终止位置/角度动作 (规范 §6.1)
 * ======================================================================== */

/**
 * @brief 执行函数 cmd_abort，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void cmd_abort(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 校验 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. 状态检查 */
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 3. 执行: RUNNING -> ENABLED，其他 -> ALREADY_IN_STATE */
    if (ctx->state == PROTO_STATE_RUNNING) {
        if (ctx->adapter != NULL && ctx->adapter->motor_abort != NULL) {
            ctx->adapter->motor_abort();
        }
        transition_state(ctx, PROTO_STATE_ENABLED, PROTO_REASON_COMMAND);
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
    } else {
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
    }
}

/* ========================================================================
 * 命令处理: DISABLE — 关闭电机输出 (规范 §6.1, §6.1 STOP_ALL与DISABLE语义)
 * ======================================================================== */

/**
 * @brief 禁用函数 cmd_disable，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void cmd_disable(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 校验 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. 状态检查 */
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 3. 执行: RUNNING/ENABLED/SAFE -> DISABLED，DISABLED/FAULT -> ALREADY_IN_STATE */
    if (ctx->state == PROTO_STATE_RUNNING) {
        /* RUNNING 先安全停机再断电 */
        if (ctx->adapter != NULL) {
            if (ctx->adapter->motor_stop_all != NULL) {
                ctx->adapter->motor_stop_all();
            }
            if (ctx->adapter->motor_disable_output != NULL) {
                ctx->adapter->motor_disable_output();
            }
        }
        transition_state(ctx, PROTO_STATE_DISABLED, PROTO_REASON_COMMAND);
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
    } else if (ctx->state == PROTO_STATE_ENABLED) {
        if (ctx->adapter != NULL && ctx->adapter->motor_disable_output != NULL) {
            ctx->adapter->motor_disable_output();
        }
        transition_state(ctx, PROTO_STATE_DISABLED, PROTO_REASON_COMMAND);
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
    } else if (ctx->state == PROTO_STATE_SAFE) {
        /* SAFE -> DISABLED (规范 §7.2: "SAFE -> DISABLED 收到 DISABLE") */
        if (ctx->adapter != NULL && ctx->adapter->motor_disable_output != NULL) {
            ctx->adapter->motor_disable_output();
        }
        transition_state(ctx, PROTO_STATE_DISABLED, PROTO_REASON_COMMAND);
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
    } else {
        /* DISABLED / FAULT — 已断电 */
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
    }
}

/* ========================================================================
 * 命令处理: CLEAR_FAULT — 清除故障锁存 (规范 §6.1, §6.1.1, §7.2.1)
 * ======================================================================== */

/**
 * @brief 清除函数 cmd_clear_fault，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void cmd_clear_fault(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 校验 */
    if (req->payload_len != 2u || req->payload == NULL) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }
    uint16_t fault_code = proto_get_u16_le(&req->payload[0]);

    /* 2. fault_code 有效性校验 (规范 §6.1.1: 未定义的非零 fault_code 必须返回 BAD_PARAM) */
    if (fault_code > PROTO_FAULT_OVERTEMP) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_FAULT_CODE, 0, true);
        return;
    }

    /* 3. INIT 状态拒绝 */
    if (ctx->state == PROTO_STATE_INIT) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 4. 按状态分支处理 */

    /* --- LINK_LOST 状态: 需先 HEARTBEAT 再 CLEAR_FAULT (§7.2.1) --- */
    if (ctx->state == PROTO_STATE_LINK_LOST) {
        /* 仅接受 FAULT_LINK_LOST 或 0x0000 */
        if (fault_code != PROTO_FAULT_LINK_LOST && fault_code != PROTO_FAULT_NONE) {
            send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                      PROTO_DETAIL_BAD_PARAM_FAULT_CODE, 0, true);
            return;
        }
        /* 检查前置条件: 已收到有效 HEARTBEAT (link_recovery_started) */
        if (!ctx->link_recovery_started) {
            send_nack(ctx, req, PROTO_NACK_LINK_NOT_READY,
                      PROTO_DETAIL_LINK_NO_HEARTBEAT, 0, true);
            return;
        }
        /* 检查链路新鲜度: 最近控制刷新不超过 200ms */
        uint32_t now = dispatch_now_ms(ctx);
        if (proto_watchdog_is_timeout(&ctx->watchdog, now)) {
            send_nack(ctx, req, PROTO_NACK_LINK_NOT_READY,
                      PROTO_DETAIL_LINK_STALE, 0, true);
            return;
        }
        /* 检查无活动硬件故障 */
        if (ctx->adapter != NULL && ctx->adapter->fault_has_active_hardware != NULL &&
            ctx->adapter->fault_has_active_hardware()) {
            send_nack(ctx, req, PROTO_NACK_FAULT_LATCHED, 0, 0, true);
            return;
        }
        /* 清除 LINK_LOST 锁存 */
        proto_watchdog_clear_link_lost(&ctx->watchdog);
        if (ctx->adapter != NULL && ctx->adapter->fault_clear != NULL) {
            ctx->adapter->fault_clear(fault_code);
        }
        ctx->link_recovery_started = false;
        transition_state(ctx, PROTO_STATE_DISABLED, PROTO_REASON_RECOVERY);
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        /* CLEAR_FAULT 不刷新 watchdog (规范: CLEAR_FAULT 不得自动恢复运行) */
        send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
        return;
    }

    /* --- FAULT 状态: 清除硬件故障 -> DISABLED --- */
    if (ctx->state == PROTO_STATE_FAULT) {
        /* 活动硬件故障不可清除 */
        if (ctx->adapter != NULL && ctx->adapter->fault_has_active_hardware != NULL &&
            ctx->adapter->fault_has_active_hardware()) {
            send_nack(ctx, req, PROTO_NACK_FAULT_LATCHED, 0, 0, true);
            return;
        }
        /* 尝试清除 */
        bool cleared = true;
        if (ctx->adapter != NULL && ctx->adapter->fault_clear != NULL) {
            cleared = ctx->adapter->fault_clear(fault_code);
        }
        if (!cleared) {
            send_nack(ctx, req, PROTO_NACK_FAULT_CLEAR_DENIED, 0, 0, true);
            return;
        }
        /* 同时清除 LINK_LOST 锁存 (若有) */
        if (fault_code == PROTO_FAULT_LINK_LOST || fault_code == PROTO_FAULT_NONE) {
            proto_watchdog_clear_link_lost(&ctx->watchdog);
            ctx->link_recovery_started = false;
        }
        transition_state(ctx, PROTO_STATE_DISABLED, PROTO_REASON_RECOVERY);
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
        return;
    }

    /* --- 其他状态 (SAFE / DISABLED / ENABLED / RUNNING) --- */
    /* 无活动故障 -> ALREADY_IN_STATE; 有锁存 -> 清除 */
    if (fault_code == PROTO_FAULT_LINK_LOST || fault_code == PROTO_FAULT_NONE) {
        if (ctx->watchdog.link_lost_latched) {
            proto_watchdog_clear_link_lost(&ctx->watchdog);
            ctx->link_recovery_started = false;
            if (ctx->adapter != NULL && ctx->adapter->fault_clear != NULL) {
                ctx->adapter->fault_clear(fault_code);
            }
            ctx->queue_generation++;
            ctx->last_command_seq = req->seq;
            send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
            return;
        }
    }
    /* 无可清除故障 */
    send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
}

/* ========================================================================
 * 命令处理: ENABLE — 允许电机输出 (规范 §6.1, §7.2)
 * ======================================================================== */

/**
 * @brief 启用函数 cmd_enable，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void cmd_enable(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 校验 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. 状态检查: 仅 DISABLED 可 ENABLE (§7.2: DISABLED->ENABLED 且无锁存故障) */
    if (ctx->state == PROTO_STATE_ENABLED || ctx->state == PROTO_STATE_RUNNING) {
        /* 已使能 — 幂等 */
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
        return;
    }
    if (ctx->state != PROTO_STATE_DISABLED) {
        /* INIT / SAFE / FAULT / LINK_LOST — 不允许 */
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 3. 检查无锁存故障 */
    if (ctx->watchdog.link_lost_latched) {
        send_nack(ctx, req, PROTO_NACK_FAULT_LATCHED, 0, 0, true);
        return;
    }
    if (ctx->adapter != NULL && ctx->adapter->fault_has_active_hardware != NULL &&
        ctx->adapter->fault_has_active_hardware()) {
        send_nack(ctx, req, PROTO_NACK_FAULT_LATCHED, 0, 0, true);
        return;
    }

    /* 4. 执行: DISABLED -> ENABLED */
    if (ctx->adapter != NULL && ctx->adapter->motor_enable_output != NULL) {
        ctx->adapter->motor_enable_output();
    }
    transition_state(ctx, PROTO_STATE_ENABLED, PROTO_REASON_COMMAND);
    ctx->queue_generation++;
    ctx->last_command_seq = req->seq;
    refresh_watchdog(ctx, req->seq);
    send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
}

/* ========================================================================
 * 命令处理: RUN — 按已设置目标运行 (规范 §6.1, §7.2)
 * ======================================================================== */

/**
 * @brief 执行函数 cmd_run，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void cmd_run(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 校验 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. 状态检查 */
    if (ctx->state == PROTO_STATE_RUNNING) {
        /* 已在运行 — 幂等 */
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
        return;
    }
    if (ctx->state != PROTO_STATE_ENABLED) {
        /* 需要 ENABLED 才能 RUN (§7.2: ENABLED->RUNNING) */
        if (ctx->state == PROTO_STATE_DISABLED || ctx->state == PROTO_STATE_SAFE) {
            send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                      PROTO_DETAIL_INVALID_STATE_NOT_ENABLED, 0, true);
        } else {
            /* INIT / FAULT / LINK_LOST */
            send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                      PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        }
        return;
    }

    /* 3. 执行: ENABLED -> RUNNING (目标/模式有效性由适配器检查) */
    bool ok = true;
    if (ctx->adapter != NULL && ctx->adapter->motor_run != NULL) {
        ok = ctx->adapter->motor_run();
    }
    if (!ok) {
        /* 适配器拒绝: 目标/模式无效 */
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }
    transition_state(ctx, PROTO_STATE_RUNNING, PROTO_REASON_COMMAND);
    ctx->queue_generation++;
    ctx->last_command_seq = req->seq;
    refresh_watchdog(ctx, req->seq);
    send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
}

/* ========================================================================
 * 命令处理: SET_TARGET — 设置单电机目标 (规范 §6.1)
 * ======================================================================== */

/**
 * @brief 获取函数 cmd_set_target，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void cmd_set_target(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 校验: 10 字节 */
    if (req->payload_len != 10u || req->payload == NULL) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    uint8_t  motor_id     = req->payload[0];
    uint8_t  mode         = req->payload[1];
    int32_t  target_value = proto_get_i32_le(&req->payload[2]);
    int32_t  limit_value  = proto_get_i32_le(&req->payload[6]);

    /* 2. motor_id 校验 */
    if (motor_id >= ctx->motor_count) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_MOTOR_ID, 0, true);
        return;
    }

    /* 3. mode 枚举校验 */
    if (mode > PROTO_MODE_ANGLE) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_MODE, 0, true);
        return;
    }

    /* 4. 当前阶段仅支持 SPEED; POSITION/ANGLE 返回 UNSUPPORTED (§6.1) */
    if (mode != PROTO_MODE_SPEED) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0, 0, true);
        return;
    }

    /* 5. SPEED 模式 target_value 范围校验 (§6.1: abs(speed) > 100000 拒绝) */
    if (target_value > 100000 || target_value < -100000) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_TARGET, 0, true);
        return;
    }

    /* 6. 状态检查: 非 LINK_LOST / INIT */
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 7. 执行: 设置模式和目标 (不改变状态) */
    if (ctx->adapter != NULL) {
        if (ctx->adapter->motor_set_mode != NULL) {
            ctx->adapter->motor_set_mode(motor_id, mode);
        }
        if (ctx->adapter->motor_set_target != NULL) {
            ctx->adapter->motor_set_target(motor_id, mode, target_value, limit_value);
        }
    }
    ctx->queue_generation++;
    ctx->last_command_seq = req->seq;
    refresh_watchdog(ctx, req->seq);
    send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
}

/* ========================================================================
 * 命令处理: SET_MODE — 设置控制模式 (规范 §6.1)
 * ======================================================================== */

/**
 * @brief 设置函数 cmd_set_mode，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void cmd_set_mode(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 校验: 2 字节 */
    if (req->payload_len != 2u || req->payload == NULL) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    uint8_t motor_id = req->payload[0];
    uint8_t mode     = req->payload[1];

    /* 2. motor_id 校验 */
    if (motor_id >= ctx->motor_count) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_MOTOR_ID, 0, true);
        return;
    }

    /* 3. mode 枚举校验 */
    if (mode > PROTO_MODE_ANGLE) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_MODE, 0, true);
        return;
    }

    /* 4. 当前阶段仅支持 SPEED; POSITION/ANGLE 返回 UNSUPPORTED (§6.1) */
    if (mode != PROTO_MODE_SPEED) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0, 0, true);
        return;
    }

    /* 5. 状态检查: 非 LINK_LOST / INIT */
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 6. ALREADY_IN_STATE 判断: 当前模式 == 请求模式 (§6.5.1) */
    if (ctx->adapter != NULL && ctx->adapter->motor_get_mode != NULL) {
        if (ctx->adapter->motor_get_mode(motor_id) == mode) {
            refresh_watchdog(ctx, req->seq);
            send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
            return;
        }
    }

    /* 7. 执行: 设置模式 (不改变状态) */
    if (ctx->adapter != NULL && ctx->adapter->motor_set_mode != NULL) {
        ctx->adapter->motor_set_mode(motor_id, mode);
    }
    ctx->queue_generation++;
    ctx->last_command_seq = req->seq;
    refresh_watchdog(ctx, req->seq);
    send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
}

/* ========================================================================
 * HEARTBEAT 处理 (规范 §6.2, §7.2.1)
 * ======================================================================== */

/**
 * @brief 处理函数 handle_heartbeat，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void handle_heartbeat(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 校验 */
    if (req->payload_len != PROTO_HEARTBEAT_PAYLOAD_LEN || req->payload == NULL) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. flags 精确校验 (规范 §6.2: HEARTBEAT flags 必须精确为 0x30) */
    if (req->flags != PROTO_FLAGS_HEARTBEAT) {
        ctx->bad_heartbeat_count++;
        if (req->flags & PROTO_FLAG_ACK_REQ) {
            /* ACK_REQ=1 时返回 NACK(BAD_FLAGS) */
            send_nack(ctx, req, PROTO_NACK_BAD_FLAGS,
                      PROTO_DETAIL_BAD_FLAGS_CLASS_MISMATCH, 0, true);
        }
        /* ACK_REQ=0 时不回复，不刷新 watchdog (§6.2) */
        return;
    }

    /* 3. requested_mode / requested_run 校验 (规范 §6.2) */
    uint8_t requested_mode = req->payload[2];
    uint8_t requested_run  = req->payload[3];
    if (requested_mode > PROTO_MODE_ANGLE || requested_run > 1u) {
        ctx->bad_heartbeat_count++;
        /* 不刷新 watchdog; HEARTBEAT ACK_REQ=0 故不回复 NACK (§6.2) */
        return;
    }

    /* 4. FAULT 状态: 拒绝 HEARTBEAT，要求先 CLEAR_FAULT */
    if (ctx->state == PROTO_STATE_FAULT) {
        ctx->bad_heartbeat_count++;
        send_nack(ctx, req, PROTO_NACK_FAULT_LATCHED, 0, 0, true);
        return;
    }

    /* 5. INIT 状态: 拒绝 */
    if (ctx->state == PROTO_STATE_INIT) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 6. LINK_LOST 状态: 只刷新 watchdog + 标记恢复已开始，不改变状态 (§7.2.1) */
    if (ctx->state == PROTO_STATE_LINK_LOST) {
        ctx->link_recovery_started = true;
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ACCEPTED, true);
        return;
    }

    /* 7. 正常状态 (SAFE / DISABLED / ENABLED / RUNNING): 刷新 watchdog */
    /* requested_mode / requested_run 仅报告 Board B 意图，不单独使能或运行电机 (§6.2) */
    refresh_watchdog(ctx, req->seq);
    send_ack(ctx, req, PROTO_RESULT_ACCEPTED, true);
}

/* ========================================================================
 * QUERY 处理 — 只读查询，不刷新 watchdog，不改变状态 (规范 §6.3, §7.2.1)
 * ======================================================================== */

/**
 * @brief 处理 QUERY_STATUS: ACK + STATUS_SUMMARY + N×STATUS_MOTOR + STATUS_SENSOR
 * @note  所有帧使用请求 seq，来自同一份快照，status_generation 一致。
 *        LINK_LOST 状态允许响应 (只读)。
 */
static void query_status(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* payload 必须为 0 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 检查 adapter 是否提供快照回调 */
    if (ctx->adapter == NULL ||
        ctx->adapter->status_get_summary == NULL ||
        ctx->adapter->status_get_motor == NULL ||
        ctx->adapter->status_get_sensor == NULL) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0, 0, true);
        return;
    }

    /* 1. 采集一致快照 */
    ctx->status_generation++;  /* 提交新快照时递增 (§6.2.1) */

    proto_snapshot_summary_t summary;
    if (!collect_summary(ctx, &summary)) {
        send_nack(ctx, req, PROTO_NACK_INTERNAL_ERROR, 0, 0, true);
        return;
    }

    /* 2. 发送 ACK (COMPLETED, 规范 §6.5.1) */
    send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);

    /* 3. 发送 STATUS_SUMMARY */
    uint8_t pl[PROTO_MAX_PAYLOAD];
    build_summary_payload(&summary, pl);
    send_status_frame(ctx, PROTO_OP_STATUS_SUMMARY,
                      pl, PROTO_STATUS_SUMMARY_LEN,
                      req->seq, 0x00u, req->src);

    /* 4. 发送 N×STATUS_MOTOR */
    for (uint8_t i = 0u; i < ctx->motor_count; i++) {
        proto_snapshot_motor_t motor;
        ctx->adapter->status_get_motor(i, &motor);
        motor.motor_id = i;  /* 确保 motor_id 与索引一致 */
        build_motor_payload(&motor, pl);
        send_status_frame(ctx, PROTO_OP_STATUS_MOTOR,
                          pl, PROTO_STATUS_MOTOR_LEN,
                          req->seq, 0x00u, req->src);
    }

    /* 5. 发送 STATUS_SENSOR */
    proto_snapshot_sensor_t sensor;
    ctx->adapter->status_get_sensor(&sensor);

    /* 多电机时 encoder_count 无效 (规范 §6.4.1) */
    if (ctx->motor_count > 1u) {
        sensor.encoder_count = 0;
        sensor.sensor_flags &= (uint16_t)~PROTO_SENSOR_ENCODER_VALID;
    }

    build_sensor_payload(&sensor, pl);
    send_status_frame(ctx, PROTO_OP_STATUS_SENSOR,
                      pl, PROTO_STATUS_SENSOR_LEN,
                      req->seq, 0x00u, req->src);
}

/**
 * @brief 处理 QUERY_INFO: ACK + STATUS_INFO
 */
static void query_info(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    if (ctx->adapter == NULL || ctx->adapter->status_get_info == NULL) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0, 0, true);
        return;
    }

    proto_snapshot_info_t info;
    ctx->adapter->status_get_info(&info);

    /* 协议层强制固定位 */
    info.device_type      = PROTO_DEVICE_MOTOR_CONTROLLER;
    info.board_role       = PROTO_BOARD_ROLE_CONTROLLER;
    info.protocol_version = PROTO_VERSION;

    send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);

    uint8_t pl[PROTO_STATUS_INFO_LEN];
    build_info_payload(&info, pl);
    send_status_frame(ctx, PROTO_OP_STATUS_INFO,
                      pl, PROTO_STATUS_INFO_LEN,
                      req->seq, 0x00u, req->src);
}

/**
 * @brief 处理 QUERY_FAULT: ACK + STATUS_FAULT
 */
static void query_fault(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    if (ctx->adapter == NULL || ctx->adapter->status_get_fault == NULL) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0, 0, true);
        return;
    }

    proto_snapshot_fault_t fault;
    ctx->adapter->status_get_fault(&fault);

    /* 确保 active 与 fault_flags.bit0 一致 (规范 §6.4.1) */
    if (fault.fault_flags & PROTO_FAULT_FLAG_ACTIVE) {
        fault.active = 0x01u;
    } else {
        fault.active = 0x00u;
    }

    send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);

    uint8_t pl[PROTO_STATUS_FAULT_LEN];
    build_fault_payload(&fault, pl);
    send_status_frame(ctx, PROTO_OP_STATUS_FAULT,
                      pl, PROTO_STATUS_FAULT_LEN,
                      req->seq, 0x00u, req->src);
}

/**
 * @brief 处理 QUERY_SENSOR: ACK + STATUS_SENSOR
 */
static void query_sensor(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    if (ctx->adapter == NULL || ctx->adapter->status_get_sensor == NULL) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0, 0, true);
        return;
    }

    proto_snapshot_sensor_t sensor;
    ctx->adapter->status_get_sensor(&sensor);

    /* 多电机时 encoder_count 无效 */
    if (ctx->motor_count > 1u) {
        sensor.encoder_count = 0;
        sensor.sensor_flags &= (uint16_t)~PROTO_SENSOR_ENCODER_VALID;
    }

    send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);

    uint8_t pl[PROTO_STATUS_SENSOR_LEN];
    build_sensor_payload(&sensor, pl);
    send_status_frame(ctx, PROTO_OP_STATUS_SENSOR,
                      pl, PROTO_STATUS_SENSOR_LEN,
                      req->seq, 0x00u, req->src);
}

/**
 * @brief QUERY 分发路由
 */
static void dispatch_query(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    switch (req->opcode) {
        case PROTO_OP_QUERY_STATUS:
            query_status(ctx, req);
            break;
        case PROTO_OP_QUERY_INFO:
            query_info(ctx, req);
            break;
        case PROTO_OP_QUERY_FAULT:
            query_fault(ctx, req);
            break;
        case PROTO_OP_QUERY_SENSOR:
            query_sensor(ctx, req);
            break;
        default:
            if (ctx->stats != NULL) { ctx->invalid_enum_count++; }
            send_nack(ctx, req, PROTO_NACK_BAD_OPCODE, 0, 0, true);
            break;
    }
}

/* ========================================================================
 * 命令分发 — 按 opcode 路由
 * ======================================================================== */

/**
 * @brief 分发函数 dispatch_command，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param req 函数参数 req。
 * @return 函数执行结果。
 */
static void dispatch_command(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* P1 安全门控：未允许的运动命令不得进入应用层。 */
    if (ctx != NULL && ctx->adapter != NULL &&
        ctx->adapter->command_allowed != NULL &&
        !ctx->adapter->command_allowed(req->opcode)) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0u, 0u, true);
        return;
    }

    switch (req->opcode) {
        case PROTO_OP_STOP:
            cmd_stop(ctx, req);
            break;
        case PROTO_OP_STOP_ALL:
            cmd_stop_all(ctx, req);
            break;
        case PROTO_OP_ABORT:
            cmd_abort(ctx, req);
            break;
        case PROTO_OP_DISABLE:
            cmd_disable(ctx, req);
            break;
        case PROTO_OP_CLEAR_FAULT:
            cmd_clear_fault(ctx, req);
            break;
        case PROTO_OP_ENABLE:
            cmd_enable(ctx, req);
            break;
        case PROTO_OP_RUN:
            cmd_run(ctx, req);
            break;
        case PROTO_OP_SET_TARGET:
            cmd_set_target(ctx, req);
            break;
        case PROTO_OP_SET_MODE:
            cmd_set_mode(ctx, req);
            break;
        default:
            if (ctx->stats != NULL) { ctx->invalid_enum_count++; }
            send_nack(ctx, req, PROTO_NACK_BAD_OPCODE, 0, 0, true);
            break;
    }
}

/* ========================================================================
 * 帧处理主入口 — proto_dispatch_process_frame
 * ======================================================================== */

/**
 * @brief 分发函数 proto_dispatch_process_frame，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param decoded 函数参数 decoded。
 * @param decoded_len 函数参数 decoded_len。
 * @return 函数执行结果。
 */
void proto_dispatch_process_frame(proto_dispatch_ctx_t *ctx,
                                  const uint8_t *decoded, size_t decoded_len)
{
    if (ctx == NULL || decoded == NULL || decoded_len == 0u) {
        return;
    }

    if (ctx->stats != NULL) { ctx->stats->rx_frames++; }

    /* 1. 帧校验: 接受 src=HOST 或 src=GATEWAY，dst=CONTROLLER */
    proto_frame_view_t view;
    bool valid = proto_frame_validate(decoded, decoded_len,
                                      PROTO_ADDR_HOST, BOARD_A_ADDR, &view);
    if (!valid) {
        valid = proto_frame_validate(decoded, decoded_len,
                                     PROTO_ADDR_GATEWAY, BOARD_A_ADDR, &view);
    }
    if (!valid) {
        /* 帧无效或非本机地址 — 静默丢弃 (规范 §5.2) */
        return;
    }

    /* 2. flags 校验: 保留位 bit6-7 必须为 0 */
    if ((view.flags & PROTO_FLAG_RESERVED_MASK) != 0u) {
        send_nack(ctx, &view, PROTO_NACK_BAD_FLAGS,
                  PROTO_DETAIL_BAD_FLAGS_RESERVED, 0, true);
        return;
    }

    /* 3. 重复包检测 (规范 §10.1) */
    proto_replay_result_t replay = proto_replay_check(&ctx->replay, &view);
    if (replay == PROTO_REPLAY_DUPLICATE) {
        /* 重复请求: 重发缓存的 ACK/NACK 逻辑帧 */
        uint8_t cached[PROTO_MAX_DECODED];
        size_t  cached_len;
        if (proto_replay_get_result(&ctx->replay, &view,
                                    cached, sizeof(cached), &cached_len)) {
            uint8_t wire[PROTO_MAX_WIRE];
            size_t  cobs_len;
            if (proto_cobs_encode(cached, cached_len,
                                  &wire[0], sizeof(wire) - 1u, &cobs_len)) {
                wire[cobs_len] = PROTO_DELIMITER;
                if (ctx->adapter != NULL && ctx->adapter->tx_send != NULL) {
                    ctx->adapter->tx_send(wire, cobs_len + 1u);
                }
                if (ctx->stats != NULL) {
                    ctx->stats->tx_frames++;
                    ctx->stats->rx_replay_hits++;
                }
            }
        }
        return;
    }
    if (replay == PROTO_REPLAY_CONFLICT) {
        /* seq 相同但 payload 不同 — 冲突 */
        send_nack(ctx, &view, PROTO_NACK_DUPLICATE_CONFLICT, 0, 0, true);
        return;
    }

    /* 4. 按消息类别分发 */
    dispatch_enter_critical(ctx);

    switch (view.msg_class) {
        case PROTO_MSG_COMMAND:
            dispatch_command(ctx, &view);
            break;

        case PROTO_MSG_HEARTBEAT:
            handle_heartbeat(ctx, &view);
            break;

        case PROTO_MSG_QUERY:
            /* QUERY 只读查询 — 不刷新 watchdog，不改变状态 (规范 §6.3, §7.2.1) */
            dispatch_query(ctx, &view);
            break;

        default:
            /* ACK / NACK / STATUS / EVENT 为上行帧，收到时忽略 */
            break;
    }

    dispatch_exit_critical(ctx);
}

/* ========================================================================
 * 周期 tick — watchdog 超时检查 (规范 §7.3)
 * ======================================================================== */

/**
 * @brief 执行周期处理函数 proto_dispatch_tick，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @return 函数执行结果。
 */
void proto_dispatch_tick(proto_dispatch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    uint32_t now = dispatch_now_ms(ctx);

    /* watchdog 未武装 (从未收到有效控制) — 不触发 */
    if (!ctx->watchdog.watchdog_armed) {
        return;
    }

    /* 已在 LINK_LOST — 不重复触发 */
    if (ctx->state == PROTO_STATE_LINK_LOST) {
        return;
    }

    /* FAULT 状态不覆盖为 LINK_LOST (规范 §7.3) */
    if (ctx->state == PROTO_STATE_FAULT) {
        return;
    }

    /* INIT / SAFE 状态不因 watchdog 超时改变状态 (规范 §7.3) */
    if (ctx->state == PROTO_STATE_INIT ||
        ctx->state == PROTO_STATE_SAFE) {
        return;
    }

    /* 检查超时 */
    if (!proto_watchdog_is_timeout(&ctx->watchdog, now)) {
        return;
    }

    /* === 触发 LINK_LOST === */

    /* 1. 安全停机 */
    if (ctx->adapter != NULL) {
        if (ctx->adapter->motor_stop_all != NULL) {
            ctx->adapter->motor_stop_all();
        }
        if (ctx->adapter->motor_disable_output != NULL) {
            ctx->adapter->motor_disable_output();
        }
    }

    /* 2. 锁存 LINK_LOST */
    proto_watchdog_set_link_lost(&ctx->watchdog);
    ctx->link_recovery_started = false;

    /* 3. 状态转换 */
    transition_state(ctx, PROTO_STATE_LINK_LOST, PROTO_REASON_WATCHDOG_TIMEOUT);

    /* 4. 发送 EVENT_LINK_LOST */
    send_event_link_lost(ctx);
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化函数 proto_dispatch_init，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param adapter 函数参数 adapter。
 * @param stats 函数参数 stats。
 * @return 函数执行结果。
 */
void proto_dispatch_init(proto_dispatch_ctx_t *ctx,
                         const proto_dispatch_adapter_t *adapter,
                         proto_stats_t *stats)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));

    ctx->state    = PROTO_STATE_SAFE;
    ctx->adapter  = adapter;
    ctx->stats    = stats;
    ctx->motor_count = DEFAULT_MOTOR_COUNT;
    ctx->event_seq   = 1u;   /* 事件 seq 从 1 开始 (0 保留给初始化) */
    ctx->status_generation = 0u;
    ctx->periodic_status_seq = 1u;
    ctx->last_periodic_status_ms = 0u;

    proto_watchdog_init(&ctx->watchdog);
    proto_replay_init(&ctx->replay);
}

/**
 * @brief 获取函数 proto_dispatch_get_state，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @return 函数执行结果。
 */
proto_state_t proto_dispatch_get_state(const proto_dispatch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return PROTO_STATE_INIT;
    }
    return ctx->state;
}

/**
 * @brief 分发函数 proto_dispatch_link_age_ms，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @return 函数执行结果。
 */
uint32_t proto_dispatch_link_age_ms(const proto_dispatch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return 0u;
    }
    uint32_t now = dispatch_now_ms(ctx);
    return proto_watchdog_age_ms(&ctx->watchdog, now);
}

/* ========================================================================
 * 公共 API — 状态快照代数
 * ======================================================================== */

/**
 * @brief 获取函数 proto_dispatch_get_status_generation，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @return 函数执行结果。
 */
uint16_t proto_dispatch_get_status_generation(const proto_dispatch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return 0u;
    }
    return ctx->status_generation;
}

/* ========================================================================
 * 公共 API — 周期状态发布 (规范 §6.4, §9.3)
 * ======================================================================== */

/**
 * @brief 分发函数 proto_dispatch_periodic_status，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param now_ms 函数参数 now_ms。
 * @param period_ms 函数参数 period_ms。
 * @return 函数执行结果。
 */
void proto_dispatch_periodic_status(proto_dispatch_ctx_t *ctx,
                                    uint32_t now_ms, uint32_t period_ms)
{
    if (ctx == NULL || period_ms == 0u) {
        return;
    }

    /* 时间未到 */
    if ((uint32_t)(now_ms - ctx->last_periodic_status_ms) < period_ms) {
        return;
    }
    ctx->last_periodic_status_ms = now_ms;

    /* 检查 adapter 快照回调 */
    if (ctx->adapter == NULL ||
        ctx->adapter->status_get_summary == NULL ||
        ctx->adapter->status_get_motor == NULL ||
        ctx->adapter->status_get_sensor == NULL) {
        return;
    }

    /* 递增 status_generation (提交新快照) */
    ctx->status_generation++;
    uint16_t gen = ctx->status_generation;
    uint16_t pseq = ctx->periodic_status_seq++;

    /* 1. STATUS_SUMMARY */
    proto_snapshot_summary_t summary;
    if (!collect_summary(ctx, &summary)) {
        return;
    }

    uint8_t pl[PROTO_MAX_PAYLOAD];
    build_summary_payload(&summary, pl);
    send_status_frame(ctx, PROTO_OP_STATUS_SUMMARY,
                      pl, PROTO_STATUS_SUMMARY_LEN,
                      pseq, PROTO_FLAG_IS_PERIODIC, PROTO_ADDR_HOST);

    /* 2. N×STATUS_MOTOR */
    for (uint8_t i = 0u; i < ctx->motor_count; i++) {
        proto_snapshot_motor_t motor;
        ctx->adapter->status_get_motor(i, &motor);
        motor.motor_id = i;
        build_motor_payload(&motor, pl);
        send_status_frame(ctx, PROTO_OP_STATUS_MOTOR,
                          pl, PROTO_STATUS_MOTOR_LEN,
                          pseq, PROTO_FLAG_IS_PERIODIC, PROTO_ADDR_HOST);
    }

    /* 3. STATUS_SENSOR */
    proto_snapshot_sensor_t sensor;
    ctx->adapter->status_get_sensor(&sensor);

    if (ctx->motor_count > 1u) {
        sensor.encoder_count = 0;
        sensor.sensor_flags &= (uint16_t)~PROTO_SENSOR_ENCODER_VALID;
    }

    build_sensor_payload(&sensor, pl);
    send_status_frame(ctx, PROTO_OP_STATUS_SENSOR,
                      pl, PROTO_STATUS_SENSOR_LEN,
                      pseq, PROTO_FLAG_IS_PERIODIC, PROTO_ADDR_HOST);

    /* gen 用于一致性检查 (所有帧共享同一 generation) */
    (void)gen;
}

/* ========================================================================
 * 公共 API — 硬件故障通知 (规范 §6.6, §7.2)
 * ======================================================================== */

/**
 * @brief 分发函数 proto_dispatch_notify_fault，完成对应模块的功能处理。
 * @param ctx 函数参数 ctx。
 * @param fault_snapshot 函数参数 fault_snapshot。
 * @return 函数执行结果。
 */
void proto_dispatch_notify_fault(proto_dispatch_ctx_t *ctx,
                                 const proto_snapshot_fault_t *fault_snapshot)
{
    if (ctx == NULL) {
        return;
    }

    /* FAULT 状态不覆盖 (但仍发送事件) */
    if (ctx->state != PROTO_STATE_FAULT) {
        /* 安全停机 */
        if (ctx->adapter != NULL) {
            if (ctx->adapter->motor_stop_all != NULL) {
                ctx->adapter->motor_stop_all();
            }
            if (ctx->adapter->motor_disable_output != NULL) {
                ctx->adapter->motor_disable_output();
            }
        }
        transition_state(ctx, PROTO_STATE_FAULT, PROTO_REASON_FAULT);
    }

    /* 发送 EVT_FAULT */
    send_event_fault(ctx, fault_snapshot);
}
