/**
 * @file    proto_dispatch.c
 * @brief   Board A 协议命令分发、状态机和 ACK/NACK 处理。
 * @note    当前项目已取消 HEARTBEAT 保活和协议 watchdog：
 *          - 不因通信间隔自动触发 LINK_LOST；
 *          - 不因通信间隔自动停止电机；
 *          - 收到旧版 HEARTBEAT 时仍保留兼容解析；
 *          - 普通 COMMAND/QUERY 按原有协议流程处理。
 *
 *          如需恢复旧版 watchdog，可将 project_config.h 中的
 *          PRJ_PROTOCOL_WATCHDOG_ENABLE 改为 1，但当前 Board A 电机工程禁止这样做。
 */
#include "proto_dispatch.h"
#include "project_config.h"
#include "proto_cobs.h"
#include <string.h>

/* ========================================================================
 * 鍐呴儴甯搁噺
 * ======================================================================== */
#define BOARD_A_ADDR          PROTO_ADDR_CONTROLLER
#define DEFAULT_MOTOR_COUNT   4u

/* ========================================================================
 * 鍐呴儴宸ュ叿
 * ======================================================================== */

/**
 * @brief 获取协议分发器当前时间
 * @param ctx 协议分发器上下文
 * @return 当前系统时间，单位为毫秒
 */
static uint32_t dispatch_now_ms(const proto_dispatch_ctx_t *ctx)
{
    if (ctx != NULL && ctx->adapter != NULL && ctx->adapter->now_ms != NULL) {
        return ctx->adapter->now_ms();
    }
    return 0u;
}

/**
 * @brief 鍒嗗彂鍑芥暟 dispatch_enter_critical锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void dispatch_enter_critical(const proto_dispatch_ctx_t *ctx)
{
    if (ctx != NULL && ctx->adapter != NULL && ctx->adapter->enter_critical != NULL) {
        ctx->adapter->enter_critical();
    }
}

/**
 * @brief 鍒嗗彂鍑芥暟 dispatch_exit_critical锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void dispatch_exit_critical(const proto_dispatch_ctx_t *ctx)
{
    if (ctx != NULL && ctx->adapter != NULL && ctx->adapter->exit_critical != NULL) {
        ctx->adapter->exit_critical();
    }
}

/**
 * @brief 鍒锋柊鍑芥暟 refresh_watchdog锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param seq 鍑芥暟鍙傛暟 seq銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void refresh_watchdog(proto_dispatch_ctx_t *ctx, uint16_t seq)
{
    if (ctx == NULL) {
        return;
    }

    /* 无论是否启用 watchdog，都记录最近一次控制序号。 */
    ctx->last_control_seq = seq;

#if (PRJ_PROTOCOL_WATCHDOG_ENABLE != 0U)
    uint32_t now = dispatch_now_ms(ctx);
    proto_watchdog_refresh(&ctx->watchdog, now, seq);
#endif
}
/* ========================================================================
 * 鍝嶅簲甯у彂閫?鈥?鏋勫缓閫昏緫甯?+ COBS 缂栫爜 + 鍙戦€?+ (鍙€?閲嶆斁缂撳瓨
 * ======================================================================== */

/**
 * @brief 鏋勫缓骞跺彂閫佷竴甯у搷搴?(ACK/NACK/EVENT)
 * @param ctx          鍒嗗彂鍣ㄤ笂涓嬫枃
 * @param frame        鍝嶅簲甯ц鍥?(payload 鎸囬拡蹇呴』鏈夋晥)
 * @param cache_req    鑻ラ潪 NULL锛屽皢閫昏緫甯у瓨鍏ラ噸鏀剧紦瀛?(閿?cache_req)
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
 * ACK 鍙戦€? * ======================================================================== */

/**
 * @brief 鍙戦€?ACK 鍝嶅簲
 * @param ctx         鍒嗗彂鍣ㄤ笂涓嬫枃
 * @param req         鍘熷璇锋眰甯ц鍥? * @param result_code proto_ack_result_t (ACCEPTED / COMPLETED / ALREADY_IN_STATE)
 * @param cache       鏄惁瀛樺叆閲嶆斁缂撳瓨
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
 * NACK 鍙戦€? * ======================================================================== */

/**
 * @brief 鍙戦€?NACK 鍝嶅簲
 * @param ctx            鍒嗗彂鍣ㄤ笂涓嬫枃
 * @param req            鍘熷璇锋眰甯ц鍥? * @param error_code     proto_nack_code_t
 * @param detail_code    琛ュ厖璇︽儏 (proto_types.h PROTO_DETAIL_* 瀹?
 * @param retry_after_ms 寤鸿閲嶈瘯绛夊緟 (0=涓嶆寚瀹?
 * @param cache          鏄惁瀛樺叆閲嶆斁缂撳瓨
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
 * EVENT 鍙戦€?鈥?寮傛浜嬩欢甯?(涓嶅瓨鍏ラ噸鏀剧紦瀛?
 * ======================================================================== */

/**
 * @brief 鍙戦€?EVENT_LINK_LOST 浜嬩欢甯?(瑙勮寖 搂6.6, 搂7.3)
 * @param ctx 鍒嗗彂鍣ㄤ笂涓嬫枃
 * @note  payload 8 瀛楄妭 (瑙勮寖 搂6.6):
 *          last_valid_seq:u16 + age_ms:u16(楗卞拰) + fault_code:u16 + reserved:u16
 */
static void send_event_link_lost(proto_dispatch_ctx_t *ctx)
{
    uint8_t payload[8];
    uint32_t now = dispatch_now_ms(ctx);
    uint32_t age = proto_watchdog_age_ms(&ctx->watchdog, now);

    proto_put_u16_le(&payload[0], ctx->watchdog.last_control_seq);            /* last_valid_seq */
    proto_put_u16_le(&payload[2], (uint16_t)(age > 0xFFFFu ? 0xFFFFu : age)); /* age_ms (u16 楗卞拰, 搂3.7) */
    proto_put_u16_le(&payload[4], PROTO_FAULT_LINK_LOST);                     /* fault_code */
    proto_put_u16_le(&payload[6], 0x0000u);                                   /* reserved */

    proto_frame_view_t evt = {
        .version     = PROTO_VERSION,
        .flags       = PROTO_FLAG_IS_EVENT,
        .src         = BOARD_A_ADDR,
        .dst         = PROTO_ADDR_GATEWAY,
        .msg_class   = PROTO_MSG_EVENT,
        .opcode      = PROTO_OP_EVT_LINK_LOST,
        .seq         = ctx->event_seq++,
        .payload_len = 8,
        .payload     = payload,
    };

    send_frame(ctx, &evt, NULL);
}

/* ========================================================================
 * EVENT 鍙戦€?鈥?EVT_FAULT (瑙勮寖 搂6.6)
 * ======================================================================== */

/**
 * @brief 鍙戦€?EVT_FAULT 浜嬩欢甯?(payload 浣跨敤 STATUS_FAULT 鏍煎紡锛?6 瀛楄妭)
 * @param ctx     鍒嗗彂鍣ㄤ笂涓嬫枃
 * @param fault   鏁呴殰蹇収 (鑻?NULL锛岄€氳繃 adapter 鑾峰彇)
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
        .dst         = PROTO_ADDR_GATEWAY,
        .msg_class   = PROTO_MSG_EVENT,
        .opcode      = PROTO_OP_EVT_FAULT,
        .seq         = ctx->event_seq++,
        .payload_len = PROTO_EVT_FAULT_LEN,
        .payload     = payload,
    };

    send_frame(ctx, &evt, NULL);
}

/* ========================================================================
 * EVENT 鍙戦€?鈥?EVT_STATE_CHANGED (瑙勮寖 搂6.6)
 * ======================================================================== */

/**
 * @brief 鍙戦€?EVT_STATE_CHANGED 浜嬩欢甯?(payload 4 瀛楄妭)
 * @param ctx       鍒嗗彂鍣ㄤ笂涓嬫枃
 * @param old_state 杞崲鍓嶇姸鎬? * @param new_state 杞崲鍚庣姸鎬? * @param reason    杞崲鍘熷洜 (proto_state_reason_t)
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
        .dst         = PROTO_ADDR_GATEWAY,
        .msg_class   = PROTO_MSG_EVENT,
        .opcode      = PROTO_OP_EVT_STATE_CHANGED,
        .seq         = ctx->event_seq++,
        .payload_len = PROTO_EVT_STATE_CHANGED_LEN,
        .payload     = payload,
    };

    send_frame(ctx, &evt, NULL);
}

/**
 * @brief 甯︿簨浠堕€氱煡鐨勭姸鎬佽浆鎹? * @note  鑻ユ柊鏃х姸鎬佷笉鍚岋紝鍙戦€?EVT_STATE_CHANGED锛? *        璋冪敤鏂瑰湪杞崲鍓嶅凡瀹屾垚瀹夊叏鍔ㄤ綔 (鍋滄満/浣胯兘绛?銆? */
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
 * STATUS payload 鏋勫缓鍣?鈥?鏄惧紡 LE 缂栫爜 (瑙勮寖 搂6.4)
 * ======================================================================== */

/**
 * @brief 鏋勫缓 STATUS_SUMMARY payload (20 瀛楄妭)
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
 * @brief 鏋勫缓 STATUS_MOTOR payload (26 瀛楄妭)
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
 * @brief 鏋勫缓 STATUS_SENSOR payload (46 瀛楄妭)
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
 * @brief 鏋勫缓 STATUS_FAULT payload (16 瀛楄妭)
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
 * @brief 鏋勫缓 STATUS_INFO payload (16 瀛楄妭)
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
 * STATUS 甯у彂閫?鈥?鏋勫缓瀹屾暣 STATUS 甯?(msg_class=0x03)
 * ======================================================================== */

/**
 * @brief 鍙戦€佷竴涓?STATUS 甯? * @param ctx       鍒嗗彂鍣ㄤ笂涓嬫枃
 * @param opcode    STATUS opcode (STATUS_SUMMARY / STATUS_MOTOR / ...)
 * @param payload   宸叉瀯寤虹殑 payload 鏁版嵁
 * @param payload_len payload 闀垮害
 * @param seq       甯у簭鍙?(QUERY 鍝嶅簲鐢ㄨ姹?seq锛涘懆鏈?STATUS 鐢ㄧ嫭绔?seq)
 * @param flags     甯?flags (鍛ㄦ湡 STATUS 鐢?IS_PERIODIC锛決UERY 鍝嶅簲鐢?0x00)
 * @param dst       鐩殑鍦板潃
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

    send_frame(ctx, &resp, NULL);  /* STATUS 涓嶅瓨鍏ラ噸鏀剧紦瀛?*/
}

/* ========================================================================
 * 蹇収閲囬泦宸ュ叿 鈥?浠?adapter 鑾峰彇涓€鑷村揩鐓? * ======================================================================== */

/**
 * @brief 閲囬泦 SUMMARY 蹇収骞跺～鍏呭崗璁眰瀛楁
 * @note   閫掑 status_generation锛屽～鍏?state/uptime/link_age 绛? */
static bool collect_summary(proto_dispatch_ctx_t *ctx,
                            proto_snapshot_summary_t *out)
{
    if (ctx->adapter == NULL || ctx->adapter->status_get_summary == NULL) {
        return false;
    }

    ctx->adapter->status_get_summary(out);

    /* 鍗忚灞傚～鍏?*/
    out->state             = (uint8_t)ctx->state;
    out->motor_count       = ctx->motor_count;
    out->last_command_seq  = ctx->last_command_seq;
    out->last_control_seq  = ctx->last_control_seq;

    uint32_t now = dispatch_now_ms(ctx);
#if (PRJ_PROTOCOL_WATCHDOG_ENABLE != 0U)
    uint32_t age = proto_watchdog_age_ms(&ctx->watchdog, now);
    out->link_age_ms = (uint16_t)(age > 0xFFFFu ? 0xFFFFu : age);
#else
    /* 已取消 HEARTBEAT/watchdog，link_age 不再代表安全失联计时。 */
    out->link_age_ms = 0u;
#endif

    if (ctx->adapter->get_uptime_ms != NULL) {
        out->uptime_ms = ctx->adapter->get_uptime_ms();
    } else {
        out->uptime_ms = now;
    }

    /* safety_flags 鐢?adapter 濉厖锛岃ˉ鍏呭崗璁眰宸茬煡浣?*/
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

/** 鐘舵€佹槸鍚﹀厑璁告帴鍙楁帶鍒跺懡浠?(闈?LINK_LOST / INIT) */
static bool state_accepts_commands(proto_state_t s)
{
    return (s != PROTO_STATE_LINK_LOST && s != PROTO_STATE_INIT);
}

/* ========================================================================
 * 鍛戒护澶勭悊: STOP 鈥?鍋滄鍗曠數鏈?(瑙勮寖 搂6.1)
 * ======================================================================== */

/**
 * @brief 鍋滄鍑芥暟 cmd_stop锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void cmd_stop(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 鏍￠獙 */
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

    /* 2. 鐘舵€佹鏌?*/
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 3. 鎵ц: RUNNING -> ENABLED锛屽叾浠?-> ALREADY_IN_STATE */
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
        /* SAFE / DISABLED / ENABLED / FAULT 鈥?鐢垫満宸插仠 */
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
    }
}

/* ========================================================================
 * 鍛戒护澶勭悊: STOP_ALL 鈥?鍋滄鍏ㄩ儴鐢垫満骞跺叧闂緭鍑?(瑙勮寖 搂6.1, 搂6.1 STOP_ALL璇箟)
 * ======================================================================== */

/**
 * @brief 鍋滄鍑芥暟 cmd_stop_all锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void cmd_stop_all(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 鏍￠獙 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. 鐘舵€佹鏌?*/
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 3. 鎵ц: RUNNING/ENABLED -> DISABLED锛屽叾浠?-> ALREADY_IN_STATE */
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
        /* SAFE / DISABLED / FAULT 鈥?宸插仠涓斿凡鏂數 */
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
    }
}

/* ========================================================================
 * 鍛戒护澶勭悊: ABORT 鈥?缁堟浣嶇疆/瑙掑害鍔ㄤ綔 (瑙勮寖 搂6.1)
 * ======================================================================== */

/**
 * @brief 鎵ц鍑芥暟 cmd_abort锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void cmd_abort(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 鏍￠獙 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. 鐘舵€佹鏌?*/
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 3. 鎵ц: RUNNING -> ENABLED锛屽叾浠?-> ALREADY_IN_STATE */
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
 * 鍛戒护澶勭悊: DISABLE 鈥?鍏抽棴鐢垫満杈撳嚭 (瑙勮寖 搂6.1, 搂6.1 STOP_ALL涓嶥ISABLE璇箟)
 * ======================================================================== */

/**
 * @brief 绂佺敤鍑芥暟 cmd_disable锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void cmd_disable(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 鏍￠獙 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. 鐘舵€佹鏌?*/
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 3. 鎵ц: RUNNING/ENABLED/SAFE -> DISABLED锛孌ISABLED/FAULT -> ALREADY_IN_STATE */
    if (ctx->state == PROTO_STATE_RUNNING) {
        /* RUNNING 鍏堝畨鍏ㄥ仠鏈哄啀鏂數 */
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
        /* SAFE -> DISABLED (瑙勮寖 搂7.2: "SAFE -> DISABLED 鏀跺埌 DISABLE") */
        if (ctx->adapter != NULL && ctx->adapter->motor_disable_output != NULL) {
            ctx->adapter->motor_disable_output();
        }
        transition_state(ctx, PROTO_STATE_DISABLED, PROTO_REASON_COMMAND);
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
    } else {
        /* DISABLED / FAULT 鈥?宸叉柇鐢?*/
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
    }
}

/* ========================================================================
 * 鍛戒护澶勭悊: CLEAR_FAULT 鈥?娓呴櫎鏁呴殰閿佸瓨 (瑙勮寖 搂6.1, 搂6.1.1, 搂7.2.1)
 * ======================================================================== */

/**
 * @brief 娓呴櫎鍑芥暟 cmd_clear_fault锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void cmd_clear_fault(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 鏍￠獙 */
    if (req->payload_len != 2u || req->payload == NULL) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }
    uint16_t fault_code = proto_get_u16_le(&req->payload[0]);

    /* 2. fault_code 鏈夋晥鎬ф牎楠?(瑙勮寖 搂6.1.1: 鏈畾涔夌殑闈為浂 fault_code 蹇呴』杩斿洖 BAD_PARAM) */
    if (fault_code > PROTO_FAULT_OVERTEMP) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_FAULT_CODE, 0, true);
        return;
    }

    /* 3. INIT 鐘舵€佹嫆缁?*/
    if (ctx->state == PROTO_STATE_INIT) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 4. 鎸夌姸鎬佸垎鏀鐞?*/

    /* --- LINK_LOST 鐘舵€? 闇€鍏?HEARTBEAT 鍐?CLEAR_FAULT (搂7.2.1) --- */
    if (ctx->state == PROTO_STATE_LINK_LOST) {
        /* 浠呮帴鍙?FAULT_LINK_LOST 鎴?0x0000 */
        if (fault_code != PROTO_FAULT_LINK_LOST && fault_code != PROTO_FAULT_NONE) {
            send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                      PROTO_DETAIL_BAD_PARAM_FAULT_CODE, 0, true);
            return;
        }
        /* 妫€鏌ュ墠缃潯浠? 宸叉敹鍒版湁鏁?HEARTBEAT (link_recovery_started) */
        if (!ctx->link_recovery_started) {
            send_nack(ctx, req, PROTO_NACK_LINK_NOT_READY,
                      PROTO_DETAIL_LINK_NO_HEARTBEAT, 0, true);
            return;
        }
        /* 妫€鏌ラ摼璺柊椴滃害: 鏈€杩戞帶鍒跺埛鏂颁笉瓒呰繃 200ms */
        uint32_t now = dispatch_now_ms(ctx);
        if (proto_watchdog_is_timeout(&ctx->watchdog, now)) {
            send_nack(ctx, req, PROTO_NACK_LINK_NOT_READY,
                      PROTO_DETAIL_LINK_STALE, 0, true);
            return;
        }
        /* 妫€鏌ユ棤娲诲姩纭欢鏁呴殰 */
        if (ctx->adapter != NULL && ctx->adapter->fault_has_active_hardware != NULL &&
            ctx->adapter->fault_has_active_hardware()) {
            send_nack(ctx, req, PROTO_NACK_FAULT_LATCHED, 0, 0, true);
            return;
        }
        /* 娓呴櫎 LINK_LOST 閿佸瓨 */
        proto_watchdog_clear_link_lost(&ctx->watchdog);
        if (ctx->adapter != NULL && ctx->adapter->fault_clear != NULL) {
            ctx->adapter->fault_clear(fault_code);
        }
        ctx->link_recovery_started = false;
        transition_state(ctx, PROTO_STATE_DISABLED, PROTO_REASON_RECOVERY);
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        /* CLEAR_FAULT 涓嶅埛鏂?watchdog (瑙勮寖: CLEAR_FAULT 涓嶅緱鑷姩鎭㈠杩愯) */
        send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
        return;
    }

    /* --- FAULT 鐘舵€? 娓呴櫎纭欢鏁呴殰 -> DISABLED --- */
    if (ctx->state == PROTO_STATE_FAULT) {
        /* 娲诲姩纭欢鏁呴殰涓嶅彲娓呴櫎 */
        if (ctx->adapter != NULL && ctx->adapter->fault_has_active_hardware != NULL &&
            ctx->adapter->fault_has_active_hardware()) {
            send_nack(ctx, req, PROTO_NACK_FAULT_LATCHED, 0, 0, true);
            return;
        }
        /* 灏濊瘯娓呴櫎 */
        bool cleared = true;
        if (ctx->adapter != NULL && ctx->adapter->fault_clear != NULL) {
            cleared = ctx->adapter->fault_clear(fault_code);
        }
        if (!cleared) {
            send_nack(ctx, req, PROTO_NACK_FAULT_CLEAR_DENIED, 0, 0, true);
            return;
        }
        /* 鍚屾椂娓呴櫎 LINK_LOST 閿佸瓨 (鑻ユ湁) */
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

    /* --- 鍏朵粬鐘舵€?(SAFE / DISABLED / ENABLED / RUNNING) --- */
    /* 鏃犳椿鍔ㄦ晠闅?-> ALREADY_IN_STATE; 鏈夐攣瀛?-> 娓呴櫎 */
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
    /* 鏃犲彲娓呴櫎鏁呴殰 */
    send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
}

/* ========================================================================
 * 鍛戒护澶勭悊: ENABLE 鈥?鍏佽鐢垫満杈撳嚭 (瑙勮寖 搂6.1, 搂7.2)
 * ======================================================================== */

/**
 * @brief 鍚敤鍑芥暟 cmd_enable锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void cmd_enable(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 鏍￠獙 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. 鐘舵€佹鏌? 浠?DISABLED 鍙?ENABLE (搂7.2: DISABLED->ENABLED 涓旀棤閿佸瓨鏁呴殰) */
    if (ctx->state == PROTO_STATE_ENABLED || ctx->state == PROTO_STATE_RUNNING) {
        /* 宸蹭娇鑳?鈥?骞傜瓑 */
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
        return;
    }
    if (ctx->state != PROTO_STATE_DISABLED) {
        /* INIT / SAFE / FAULT / LINK_LOST 鈥?涓嶅厑璁?*/
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 3. 妫€鏌ユ棤閿佸瓨鏁呴殰 */
    if (ctx->watchdog.link_lost_latched) {
        send_nack(ctx, req, PROTO_NACK_FAULT_LATCHED, 0, 0, true);
        return;
    }
    if (ctx->adapter != NULL && ctx->adapter->fault_has_active_hardware != NULL &&
        ctx->adapter->fault_has_active_hardware()) {
        send_nack(ctx, req, PROTO_NACK_FAULT_LATCHED, 0, 0, true);
        return;
    }

    /* 4. 鎵ц: DISABLED -> ENABLED */
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
 * 鍛戒护澶勭悊: RUN 鈥?鎸夊凡璁剧疆鐩爣杩愯 (瑙勮寖 搂6.1, 搂7.2)
 * ======================================================================== */

/**
 * @brief 鎵ц鍑芥暟 cmd_run锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void cmd_run(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 鏍￠獙 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. 鐘舵€佹鏌?*/
    if (ctx->state == PROTO_STATE_RUNNING) {
        /* 宸插湪杩愯 鈥?骞傜瓑 */
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
        return;
    }
    if (ctx->state != PROTO_STATE_ENABLED) {
        /* 闇€瑕?ENABLED 鎵嶈兘 RUN (搂7.2: ENABLED->RUNNING) */
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

    /* 3. 鎵ц: ENABLED -> RUNNING (鐩爣/妯″紡鏈夋晥鎬х敱閫傞厤鍣ㄦ鏌? */
    bool ok = true;
    if (ctx->adapter != NULL && ctx->adapter->motor_run != NULL) {
        ok = ctx->adapter->motor_run();
    }
    if (!ok) {
        /* 閫傞厤鍣ㄦ嫆缁? 鐩爣/妯″紡鏃犳晥 */
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
 * 鍛戒护澶勭悊: SET_TARGET 鈥?璁剧疆鍗曠數鏈虹洰鏍?(瑙勮寖 搂6.1)
 * ======================================================================== */

/**
 * @brief 鑾峰彇鍑芥暟 cmd_set_target锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void cmd_set_target(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 鏍￠獙: 10 瀛楄妭 */
    if (req->payload_len != 10u || req->payload == NULL) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    uint8_t  motor_id     = req->payload[0];
    uint8_t  mode         = req->payload[1];
    int32_t  target_value = proto_get_i32_le(&req->payload[2]);
    int32_t  limit_value  = proto_get_i32_le(&req->payload[6]);

    /* 2. motor_id 鏍￠獙 */
    if (motor_id >= ctx->motor_count) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_MOTOR_ID, 0, true);
        return;
    }

    /* 3. mode 鏋氫妇鏍￠獙 */
    if (mode > PROTO_MODE_ANGLE) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_MODE, 0, true);
        return;
    }

    /* 4. 褰撳墠闃舵浠呮敮鎸?SPEED; POSITION/ANGLE 杩斿洖 UNSUPPORTED (搂6.1) */
    if (mode != PROTO_MODE_SPEED) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0, 0, true);
        return;
    }

    /* 5. SPEED 妯″紡 target_value 鑼冨洿鏍￠獙 (搂6.1: abs(speed) > 100000 鎷掔粷) */
    if (target_value > 100000 || target_value < -100000) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_TARGET, 0, true);
        return;
    }

    /* 6. 鐘舵€佹鏌? 闈?LINK_LOST / INIT */
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 7. 鎵ц: 璁剧疆妯″紡鍜岀洰鏍?(涓嶆敼鍙樼姸鎬? */
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
 * 鍛戒护澶勭悊: SET_MODE 鈥?璁剧疆鎺у埗妯″紡 (瑙勮寖 搂6.1)
 * ======================================================================== */

/**
 * @brief 璁剧疆鍑芥暟 cmd_set_mode锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void cmd_set_mode(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 鏍￠獙: 2 瀛楄妭 */
    if (req->payload_len != 2u || req->payload == NULL) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    uint8_t motor_id = req->payload[0];
    uint8_t mode     = req->payload[1];

    /* 2. motor_id 鏍￠獙 */
    if (motor_id >= ctx->motor_count) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_MOTOR_ID, 0, true);
        return;
    }

    /* 3. mode 鏋氫妇鏍￠獙 */
    if (mode > PROTO_MODE_ANGLE) {
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM,
                  PROTO_DETAIL_BAD_PARAM_MODE, 0, true);
        return;
    }

    /* 4. 褰撳墠闃舵浠呮敮鎸?SPEED; POSITION/ANGLE 杩斿洖 UNSUPPORTED (搂6.1) */
    if (mode != PROTO_MODE_SPEED) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0, 0, true);
        return;
    }

    /* 5. 鐘舵€佹鏌? 闈?LINK_LOST / INIT */
    if (!state_accepts_commands(ctx->state)) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 6. ALREADY_IN_STATE 鍒ゆ柇: 褰撳墠妯″紡 == 璇锋眰妯″紡 (搂6.5.1) */
    if (ctx->adapter != NULL && ctx->adapter->motor_get_mode != NULL) {
        if (ctx->adapter->motor_get_mode(motor_id) == mode) {
            refresh_watchdog(ctx, req->seq);
            send_ack(ctx, req, PROTO_RESULT_ALREADY_IN_STATE, true);
            return;
        }
    }

    /* 7. 鎵ц: 璁剧疆妯″紡 (涓嶆敼鍙樼姸鎬? */
    if (ctx->adapter != NULL && ctx->adapter->motor_set_mode != NULL) {
        ctx->adapter->motor_set_mode(motor_id, mode);
    }
    ctx->queue_generation++;
    ctx->last_command_seq = req->seq;
    refresh_watchdog(ctx, req->seq);
    send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);
}

/* ========================================================================
 * HEARTBEAT 澶勭悊 (瑙勮寖 搂6.2, 搂7.2.1)
 * ======================================================================== */

/**
 * @brief 澶勭悊鍑芥暟 handle_heartbeat锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
static void handle_heartbeat(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* 1. payload 鏍￠獙 */
    if (req->payload_len != PROTO_HEARTBEAT_PAYLOAD_LEN || req->payload == NULL) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 2. flags 绮剧‘鏍￠獙 (瑙勮寖 搂6.2: HEARTBEAT flags 蹇呴』绮剧‘涓?0x30) */
    if (req->flags != PROTO_FLAGS_HEARTBEAT) {
        ctx->bad_heartbeat_count++;
        if (req->flags & PROTO_FLAG_ACK_REQ) {
            /* ACK_REQ=1 鏃惰繑鍥?NACK(BAD_FLAGS) */
            send_nack(ctx, req, PROTO_NACK_BAD_FLAGS,
                      PROTO_DETAIL_BAD_FLAGS_CLASS_MISMATCH, 0, true);
        }
        /* ACK_REQ=0 鏃朵笉鍥炲锛屼笉鍒锋柊 watchdog (搂6.2) */
        return;
    }

    /* 3. requested_mode / requested_run 鏍￠獙 (瑙勮寖 搂6.2) */
    uint8_t requested_mode = req->payload[2];
    uint8_t requested_run  = req->payload[3];
    if (requested_mode > PROTO_MODE_ANGLE || requested_run > 1u) {
        ctx->bad_heartbeat_count++;
        /* 涓嶅埛鏂?watchdog; HEARTBEAT ACK_REQ=0 鏁呬笉鍥炲 NACK (搂6.2) */
        return;
    }

    /* 4. FAULT 鐘舵€? 鎷掔粷 HEARTBEAT锛岃姹傚厛 CLEAR_FAULT */
    if (ctx->state == PROTO_STATE_FAULT) {
        ctx->bad_heartbeat_count++;
        send_nack(ctx, req, PROTO_NACK_FAULT_LATCHED, 0, 0, true);
        return;
    }

    /* 5. INIT 鐘舵€? 鎷掔粷 */
    if (ctx->state == PROTO_STATE_INIT) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0, true);
        return;
    }

    /* 6. LINK_LOST 鐘舵€? 鍙埛鏂?watchdog + 鏍囪鎭㈠宸插紑濮嬶紝涓嶆敼鍙樼姸鎬?(搂7.2.1) */
    if (ctx->state == PROTO_STATE_LINK_LOST) {
        ctx->link_recovery_started = true;
        refresh_watchdog(ctx, req->seq);
        send_ack(ctx, req, PROTO_RESULT_ACCEPTED, true);
        return;
    }

    /* 7. 姝ｅ父鐘舵€?(SAFE / DISABLED / ENABLED / RUNNING): 鍒锋柊 watchdog */
    /* requested_mode / requested_run 浠呮姤鍛?Board B 鎰忓浘锛屼笉鍗曠嫭浣胯兘鎴栬繍琛岀數鏈?(搂6.2) */
    refresh_watchdog(ctx, req->seq);
    send_ack(ctx, req, PROTO_RESULT_ACCEPTED, true);
}

/* ========================================================================
 * QUERY 澶勭悊 鈥?鍙鏌ヨ锛屼笉鍒锋柊 watchdog锛屼笉鏀瑰彉鐘舵€?(瑙勮寖 搂6.3, 搂7.2.1)
 * ======================================================================== */

/**
 * @brief 澶勭悊 QUERY_STATUS: ACK + STATUS_SUMMARY + N脳STATUS_MOTOR + STATUS_SENSOR
 * @note  鎵€鏈夊抚浣跨敤璇锋眰 seq锛屾潵鑷悓涓€浠藉揩鐓э紝status_generation 涓€鑷淬€? *        LINK_LOST 鐘舵€佸厑璁稿搷搴?(鍙)銆? */
static void query_status(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* payload 蹇呴』涓?0 */
    if (req->payload_len != 0u) {
        send_nack(ctx, req, PROTO_NACK_BAD_LENGTH,
                  PROTO_DETAIL_BAD_LENGTH_PAYLOAD, 0, true);
        return;
    }

    /* 妫€鏌?adapter 鏄惁鎻愪緵蹇収鍥炶皟 */
    if (ctx->adapter == NULL ||
        ctx->adapter->status_get_summary == NULL ||
        ctx->adapter->status_get_motor == NULL ||
        ctx->adapter->status_get_sensor == NULL) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0, 0, true);
        return;
    }

    /* 1. 先采集完整 SUMMARY，采集失败时不消耗 generation。 */
    proto_snapshot_summary_t summary;
    if (!collect_summary(ctx, &summary)) {
        send_nack(ctx, req, PROTO_NACK_INTERNAL_ERROR, 0, 0, true);
        return;
    }

    /* 采集成功后提交新的完整快照批次。 */
    ctx->status_generation++;
    summary.status_generation = ctx->status_generation;

    /* 2. 发送 ACK (COMPLETED, 规范 §6.5.1) */
    send_ack(ctx, req, PROTO_RESULT_COMPLETED, true);

    /* 3. 鍙戦€?STATUS_SUMMARY */
    uint8_t pl[PROTO_MAX_PAYLOAD];
    build_summary_payload(&summary, pl);
    send_status_frame(ctx, PROTO_OP_STATUS_SUMMARY,
                      pl, PROTO_STATUS_SUMMARY_LEN,
                      req->seq, 0x00u, req->src);

    /* 4. 鍙戦€?N脳STATUS_MOTOR */
    for (uint8_t i = 0u; i < ctx->motor_count; i++) {
        proto_snapshot_motor_t motor;
        ctx->adapter->status_get_motor(i, &motor);
        motor.motor_id = i;  /* 纭繚 motor_id 涓庣储寮曚竴鑷?*/
        build_motor_payload(&motor, pl);
        send_status_frame(ctx, PROTO_OP_STATUS_MOTOR,
                          pl, PROTO_STATUS_MOTOR_LEN,
                          req->seq, 0x00u, req->src);
    }

    /* 5. 鍙戦€?STATUS_SENSOR */
    proto_snapshot_sensor_t sensor;
    ctx->adapter->status_get_sensor(&sensor);

    /* 澶氱數鏈烘椂 encoder_count 鏃犳晥 (瑙勮寖 搂6.4.1) */
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
 * @brief 澶勭悊 QUERY_INFO: ACK + STATUS_INFO
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

    /* 鍗忚灞傚己鍒跺浐瀹氫綅 */
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
 * @brief 澶勭悊 QUERY_FAULT: ACK + STATUS_FAULT
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

    /* 纭繚 active 涓?fault_flags.bit0 涓€鑷?(瑙勮寖 搂6.4.1) */
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
 * @brief 澶勭悊 QUERY_SENSOR: ACK + STATUS_SENSOR
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

    /* 澶氱數鏈烘椂 encoder_count 鏃犳晥 */
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
 * @brief QUERY 鍒嗗彂璺敱
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
 * 鍛戒护鍒嗗彂 鈥?鎸?opcode 璺敱
 * ======================================================================== */

/**
 * @brief 鍒嗗彂鍑芥暟 dispatch_command锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param req 鍑芥暟鍙傛暟 req銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
/* 执行用户自定义命令。用户命令不刷新安全 watchdog。 */
static void dispatch_user_command(proto_dispatch_ctx_t *ctx,
                                   const proto_frame_view_t *req)
{
    uint16_t detail_code = 0u;
    proto_user_result_t result;

    if (ctx == NULL || req == NULL || ctx->adapter == NULL ||
        ctx->adapter->user_command_execute == NULL) {
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, 0u, 0u, true);
        return;
    }

    /* 自定义业务不得绕过故障、失联和初始化安全状态。 */
    if (ctx->state == PROTO_STATE_FAULT ||
        ctx->state == PROTO_STATE_LINK_LOST ||
        ctx->state == PROTO_STATE_INIT) {
        send_nack(ctx, req, PROTO_NACK_INVALID_STATE,
                  PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED, 0u, true);
        return;
    }

    result = ctx->adapter->user_command_execute(req->opcode,
                                                req->payload,
                                                req->payload_len,
                                                &detail_code);
    switch (result) {
    case PROTO_USER_RESULT_ACCEPTED:
    case PROTO_USER_RESULT_COMPLETED:
    case PROTO_USER_RESULT_ALREADY_IN_STATE:
        ctx->queue_generation++;
        ctx->last_command_seq = req->seq;
        send_ack(ctx, req, (uint8_t)result, true);
        break;
    case PROTO_USER_RESULT_BAD_PARAM:
        send_nack(ctx, req, PROTO_NACK_BAD_PARAM, detail_code, 0u, true);
        break;
    case PROTO_USER_RESULT_BUSY:
        send_nack(ctx, req, PROTO_NACK_BUSY, detail_code, 0u, true);
        break;
    case PROTO_USER_RESULT_UNSUPPORTED:
        send_nack(ctx, req, PROTO_NACK_UNSUPPORTED, detail_code, 0u, true);
        break;
    case PROTO_USER_RESULT_INTERNAL_ERROR:
    default:
        send_nack(ctx, req, PROTO_NACK_INTERNAL_ERROR, detail_code, 0u, true);
        break;
    }
}
static void dispatch_command(proto_dispatch_ctx_t *ctx, const proto_frame_view_t *req)
{
    /* P1 瀹夊叏闂ㄦ帶锛氭湭鍏佽鐨勮繍鍔ㄥ懡浠や笉寰楄繘鍏ュ簲鐢ㄥ眰銆?*/
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
            if (PROTO_IS_USER_COMMAND_OPCODE(req->opcode)) {
                dispatch_user_command(ctx, req);
            } else {
                if (ctx->stats != NULL) { ctx->invalid_enum_count++; }
                send_nack(ctx, req, PROTO_NACK_BAD_OPCODE, 0, 0, true);
            }
            break;
    }
}

/* ========================================================================
 * 甯у鐞嗕富鍏ュ彛 鈥?proto_dispatch_process_frame
 * ======================================================================== */

/**
 * @brief 鍒嗗彂鍑芥暟 proto_dispatch_process_frame锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param decoded 鍑芥暟鍙傛暟 decoded銆? * @param decoded_len 鍑芥暟鍙傛暟 decoded_len銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
/* ========================================================================
 * 下行帧语义校验
 *
 * 地址、长度、版本和 CRC 由 proto_frame_validate() 校验；这里统一校验
 * 当前双板正式链路允许的消息类别、opcode 和 flags 组合。
 * ======================================================================== */
static uint16_t downlink_command_payload_len(uint8_t opcode, bool *known)
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

/*
 * 下行帧语义校验必须在 replay 检查之前完成。
 * 这样长度错误、枚举错误和参数错误不会占用序号，也不会重放旧 ACK。
 */
static bool validate_downlink_semantics(const proto_dispatch_ctx_t *ctx,
                                        const proto_frame_view_t *view,
                                        uint8_t motor_count,
                                        uint8_t *error_code,
                                        uint16_t *detail_code)
{
    if (view == NULL || error_code == NULL || detail_code == NULL) {
        return false;
    }

    *error_code = PROTO_NACK_BAD_CLASS;
    *detail_code = 0u;

    if ((view->flags & PROTO_FLAG_RESERVED_MASK) != 0u) {
        *error_code = PROTO_NACK_BAD_FLAGS;
        *detail_code = PROTO_DETAIL_BAD_FLAGS_RESERVED;
        return false;
    }

    switch (view->msg_class) {
    case PROTO_MSG_COMMAND:
    {
        /* 用户 opcode 的 payload 长度由 Board A 用户命令表决定。 */
        if (PROTO_IS_USER_COMMAND_OPCODE(view->opcode)) {
            uint16_t min_len = 0u;
            uint16_t max_len = PROTO_MAX_PAYLOAD;
            bool idempotent = false;
            bool known_user = (ctx != NULL && ctx->adapter != NULL &&
                               ctx->adapter->user_command_get_info != NULL &&
                               ctx->adapter->user_command_get_info(
                                   view->opcode, &min_len, &max_len, &idempotent));
            uint8_t allowed_flags = PROTO_FLAG_ACK_REQ;

            if (!known_user) {
                *error_code = PROTO_NACK_BAD_OPCODE;
                return false;
            }
            if (idempotent) {
                allowed_flags |= PROTO_FLAG_IS_IDEMPOTENT;
            }
            if ((view->flags & PROTO_FLAG_RESERVED_MASK) != 0u ||
                (view->flags & (uint8_t)~allowed_flags) != 0u ||
                (view->flags & PROTO_FLAG_ACK_REQ) == 0u) {
                *error_code = PROTO_NACK_BAD_FLAGS;
                *detail_code = ((view->flags & PROTO_FLAG_RESERVED_MASK) != 0u) ?
                               PROTO_DETAIL_BAD_FLAGS_RESERVED :
                               PROTO_DETAIL_BAD_FLAGS_CLASS_MISMATCH;
                return false;
            }
            if (view->payload_len < min_len || view->payload_len > max_len ||
                (view->payload_len > 0u && view->payload == NULL)) {
                *error_code = PROTO_NACK_BAD_LENGTH;
                *detail_code = PROTO_DETAIL_BAD_LENGTH_PAYLOAD;
                return false;
            }
            return true;
        }

        bool known_opcode;
        uint16_t expected_len = downlink_command_payload_len(view->opcode,
                                                              &known_opcode);
        uint8_t allowed_flags = PROTO_FLAG_ACK_REQ;
        bool idempotent_command = (view->opcode == PROTO_OP_STOP ||
                                   view->opcode == PROTO_OP_STOP_ALL ||
                                   view->opcode == PROTO_OP_ABORT);

        if (!known_opcode) {
            *error_code = PROTO_NACK_BAD_OPCODE;
            return false;
        }
        if (idempotent_command) {
            allowed_flags |= PROTO_FLAG_IS_IDEMPOTENT;
        }
        /* v1 双板链路要求 COMMAND 必须请求 ACK/NACK。 */
        if ((view->flags & PROTO_FLAG_ACK_REQ) == 0u ||
            (view->flags & (uint8_t)~allowed_flags) != 0u) {
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

        /* 能在协议层确定的参数错误，在 replay 之前直接拦截。 */
        if (view->opcode == PROTO_OP_STOP) {
            if (view->payload[0] >= motor_count) {
                *error_code = PROTO_NACK_BAD_PARAM;
                *detail_code = PROTO_DETAIL_BAD_PARAM_MOTOR_ID;
                return false;
            }
            if (view->payload[1] > PROTO_STOP_TYPE_FAST_SAFETY) {
                *error_code = PROTO_NACK_BAD_PARAM;
                *detail_code = PROTO_DETAIL_BAD_PARAM_STOP_TYPE;
                return false;
            }
        } else if (view->opcode == PROTO_OP_SET_TARGET) {
            if (view->payload[0] >= motor_count) {
                *error_code = PROTO_NACK_BAD_PARAM;
                *detail_code = PROTO_DETAIL_BAD_PARAM_MOTOR_ID;
                return false;
            }
            if (view->payload[1] > PROTO_MODE_ANGLE) {
                *error_code = PROTO_NACK_BAD_PARAM;
                *detail_code = PROTO_DETAIL_BAD_PARAM_MODE;
                return false;
            }
            if (proto_get_i32_le(&view->payload[2]) > 100000 ||
                proto_get_i32_le(&view->payload[2]) < -100000) {
                *error_code = PROTO_NACK_BAD_PARAM;
                *detail_code = PROTO_DETAIL_BAD_PARAM_TARGET;
                return false;
            }
        } else if (view->opcode == PROTO_OP_SET_MODE) {
            if (view->payload[0] >= motor_count) {
                *error_code = PROTO_NACK_BAD_PARAM;
                *detail_code = PROTO_DETAIL_BAD_PARAM_MOTOR_ID;
                return false;
            }
            if (view->payload[1] > PROTO_MODE_ANGLE) {
                *error_code = PROTO_NACK_BAD_PARAM;
                *detail_code = PROTO_DETAIL_BAD_PARAM_MODE;
                return false;
            }
        } else if (view->opcode == PROTO_OP_CLEAR_FAULT) {
            if (proto_get_u16_le(&view->payload[0]) > PROTO_FAULT_OVERTEMP) {
                *error_code = PROTO_NACK_BAD_PARAM;
                *detail_code = PROTO_DETAIL_BAD_PARAM_FAULT_CODE;
                return false;
            }
        }
        return true;
    }

    case PROTO_MSG_QUERY:
        if (view->opcode < PROTO_OP_QUERY_STATUS ||
            view->opcode > PROTO_OP_QUERY_SENSOR) {
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
            *detail_code = 0u;
            return false;
        }
        return true;

    case PROTO_MSG_INVALID:
    default:
        *error_code = PROTO_NACK_BAD_CLASS;
        return false;
    }
}
void proto_dispatch_process_frame(proto_dispatch_ctx_t *ctx,
                                  const uint8_t *decoded, size_t decoded_len)
{
    if (ctx == NULL || decoded == NULL || decoded_len == 0u) {
        return;
    }

    if (ctx->stats != NULL) {
        ctx->stats->rx_frames++;
    }

    /* 1. 先完成地址、长度、版本和 CRC 校验。 */
    proto_frame_view_t view;
    if (!proto_frame_validate(decoded, decoded_len,
                              PROTO_ADDR_GATEWAY, BOARD_A_ADDR, &view)) {
        return;
    }

    /* 2. 再完成消息类别、opcode 和 flags 的完整语义校验。
     *    语义非法的帧不能进入 replay 表，避免错误帧重放旧 ACK/NACK。 */
    uint8_t semantic_error;
    uint16_t semantic_detail;
    if (!validate_downlink_semantics(ctx, &view, ctx->motor_count, &semantic_error, &semantic_detail)) {
        /* HEARTBEAT 语义非法时不得刷新 watchdog；ACK_REQ=0 不回 NACK。 */
        if (view.msg_class == PROTO_MSG_HEARTBEAT) {
            ctx->bad_heartbeat_count++;
            if ((view.flags & PROTO_FLAG_ACK_REQ) != 0u) {
                send_nack(ctx, &view, semantic_error, semantic_detail, 0u, false);
            }
        } else {
            /* 非法语义帧可以回 NACK，但不得进入 replay cache。 */
            send_nack(ctx, &view, semantic_error, semantic_detail, 0u, false);
        }
        return;
    }

    /* 3. 只有语义合法后才检查重复和冲突。 */
    proto_replay_result_t replay = proto_replay_check(&ctx->replay, &view);
    if (replay == PROTO_REPLAY_DUPLICATE) {
        uint8_t cached[PROTO_MAX_DECODED];
        size_t cached_len;
        if (proto_replay_get_result(&ctx->replay, &view,
                                    cached, sizeof(cached), &cached_len)) {
            uint8_t wire[PROTO_MAX_WIRE];
            size_t cobs_len;
            if (proto_cobs_encode(cached, cached_len,
                                  wire, sizeof(wire) - 1u, &cobs_len)) {
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
        send_nack(ctx, &view, PROTO_NACK_DUPLICATE_CONFLICT,
                  0u, 0u, true);
        return;
    }

    /* 4. 分发到 Board A 的三个正式下行入口。 */
    dispatch_enter_critical(ctx);
    switch (view.msg_class) {
    case PROTO_MSG_COMMAND:
        dispatch_command(ctx, &view);
        break;
    case PROTO_MSG_HEARTBEAT:
        handle_heartbeat(ctx, &view);
        break;
    case PROTO_MSG_QUERY:
        dispatch_query(ctx, &view);
        break;
    default:
        /* validate_downlink_semantics() 已拦截其他类别。 */
        break;
    }
    dispatch_exit_critical(ctx);
}

/* ========================================================================
 * 鍛ㄦ湡 tick 鈥?watchdog 瓒呮椂妫€鏌?(瑙勮寖 搂7.3)
 * ======================================================================== */

/**
 * @brief 鎵ц鍛ㄦ湡澶勭悊鍑芥暟 proto_dispatch_tick锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
void proto_dispatch_tick(proto_dispatch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

#if (PRJ_PROTOCOL_WATCHDOG_ENABLE == 0U)
    /* 当前项目取消 HEARTBEAT/watchdog，不因通信间隔自动触发 LINK_LOST。 */
    return;
#else
    uint32_t now = dispatch_now_ms(ctx);

    /* watchdog 未武装（尚未收到有效控制）时不触发。 */
    if (!ctx->watchdog.watchdog_armed) {
        return;
    }

    /* 已经处于 LINK_LOST 时不重复触发。 */
    if (ctx->state == PROTO_STATE_LINK_LOST) {
        return;
    }

    /* FAULT 状态不覆盖为 LINK_LOST。 */
    if (ctx->state == PROTO_STATE_FAULT) {
        return;
    }

    /* INIT / SAFE 状态不因 watchdog 超时改变状态。 */
    if (ctx->state == PROTO_STATE_INIT ||
        ctx->state == PROTO_STATE_SAFE) {
        return;
    }

    if (!proto_watchdog_is_timeout(&ctx->watchdog, now)) {
        return;
    }

    /* 超时后的旧版安全停机流程。 */
    if (ctx->adapter != NULL) {
        if (ctx->adapter->motor_stop_all != NULL) {
            ctx->adapter->motor_stop_all();
        }
        if (ctx->adapter->motor_disable_output != NULL) {
            ctx->adapter->motor_disable_output();
        }
    }

    proto_watchdog_set_link_lost(&ctx->watchdog);
    ctx->link_recovery_started = false;
    transition_state(ctx, PROTO_STATE_LINK_LOST, PROTO_REASON_WATCHDOG_TIMEOUT);
    send_event_link_lost(ctx);
#endif
}
/* ========================================================================
 * 鍏叡 API
 * ======================================================================== */

/**
 * @brief 鍒濆鍖栧嚱鏁?proto_dispatch_init锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param adapter 鍑芥暟鍙傛暟 adapter銆? * @param stats 鍑芥暟鍙傛暟 stats銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
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
    ctx->event_seq   = 1u;   /* 浜嬩欢 seq 浠?1 寮€濮?(0 淇濈暀缁欏垵濮嬪寲) */
    ctx->status_generation = 0u;
    ctx->periodic_status_seq = 1u;
    ctx->last_periodic_status_ms = 0u;

    proto_watchdog_init(&ctx->watchdog);
    proto_replay_init(&ctx->replay);
}

/**
 * @brief 鑾峰彇鍑芥暟 proto_dispatch_get_state锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
proto_state_t proto_dispatch_get_state(const proto_dispatch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return PROTO_STATE_INIT;
    }
    return ctx->state;
}

/**
 * @brief 鍒嗗彂鍑芥暟 proto_dispatch_link_age_ms锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
uint32_t proto_dispatch_link_age_ms(const proto_dispatch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return 0u;
    }
#if (PRJ_PROTOCOL_WATCHDOG_ENABLE != 0U)
    uint32_t now = dispatch_now_ms(ctx);
    return proto_watchdog_age_ms(&ctx->watchdog, now);
#else
    return 0u;
#endif
}

/* ========================================================================
 * 鍏叡 API 鈥?鐘舵€佸揩鐓т唬鏁? * ======================================================================== */

/**
 * @brief 鑾峰彇鍑芥暟 proto_dispatch_get_status_generation锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
uint16_t proto_dispatch_get_status_generation(const proto_dispatch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return 0u;
    }
    return ctx->status_generation;
}

/* ========================================================================
 * 鍏叡 API 鈥?鍛ㄦ湡鐘舵€佸彂甯?(瑙勮寖 搂6.4, 搂9.3)
 * ======================================================================== */

/**
 * @brief 鍒嗗彂鍑芥暟 proto_dispatch_periodic_status锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param now_ms 鍑芥暟鍙傛暟 now_ms銆? * @param period_ms 鍑芥暟鍙傛暟 period_ms銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
void proto_dispatch_periodic_status(proto_dispatch_ctx_t *ctx,
                                    uint32_t now_ms, uint32_t period_ms)
{
    if (ctx == NULL || period_ms == 0u) {
        return;
    }

    /* 鏃堕棿鏈埌 */
    if ((uint32_t)(now_ms - ctx->last_periodic_status_ms) < period_ms) {
        return;
    }
    ctx->last_periodic_status_ms = now_ms;

    /* 妫€鏌?adapter 蹇収鍥炶皟 */
    if (ctx->adapter == NULL ||
        ctx->adapter->status_get_summary == NULL ||
        ctx->adapter->status_get_motor == NULL ||
        ctx->adapter->status_get_sensor == NULL) {
        return;
    }

    uint16_t pseq = ctx->periodic_status_seq++;

    /* 1. 先采集完整 SUMMARY；失败时不消耗 generation。 */
    proto_snapshot_summary_t summary;
    if (!collect_summary(ctx, &summary)) {
        return;
    }

    /* 采集成功后提交新的完整快照批次。 */
    ctx->status_generation++;
    summary.status_generation = ctx->status_generation;

    uint8_t pl[PROTO_MAX_PAYLOAD];
    build_summary_payload(&summary, pl);
    send_status_frame(ctx, PROTO_OP_STATUS_SUMMARY,
                      pl, PROTO_STATUS_SUMMARY_LEN,
                      pseq, PROTO_FLAG_IS_PERIODIC, PROTO_ADDR_GATEWAY);

    /* 2. N脳STATUS_MOTOR */
    for (uint8_t i = 0u; i < ctx->motor_count; i++) {
        proto_snapshot_motor_t motor;
        ctx->adapter->status_get_motor(i, &motor);
        motor.motor_id = i;
        build_motor_payload(&motor, pl);
        send_status_frame(ctx, PROTO_OP_STATUS_MOTOR,
                          pl, PROTO_STATUS_MOTOR_LEN,
                          pseq, PROTO_FLAG_IS_PERIODIC, PROTO_ADDR_GATEWAY);
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
                      pseq, PROTO_FLAG_IS_PERIODIC, PROTO_ADDR_GATEWAY);

}

/* ========================================================================
 * 鍏叡 API 鈥?纭欢鏁呴殰閫氱煡 (瑙勮寖 搂6.6, 搂7.2)
 * ======================================================================== */

/**
 * @brief 鍒嗗彂鍑芥暟 proto_dispatch_notify_fault锛屽畬鎴愬搴旀ā鍧楃殑鍔熻兘澶勭悊銆? * @param ctx 鍑芥暟鍙傛暟 ctx銆? * @param fault_snapshot 鍑芥暟鍙傛暟 fault_snapshot銆? * @return 鍑芥暟鎵ц缁撴灉銆? */
void proto_dispatch_notify_fault(proto_dispatch_ctx_t *ctx,
                                 const proto_snapshot_fault_t *fault_snapshot)
{
    if (ctx == NULL) {
        return;
    }

    /* FAULT 鐘舵€佷笉瑕嗙洊 (浣嗕粛鍙戦€佷簨浠? */
    if (ctx->state != PROTO_STATE_FAULT) {
        /* 瀹夊叏鍋滄満 */
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

    /* 鍙戦€?EVT_FAULT */
    send_event_fault(ctx, fault_snapshot);
}
