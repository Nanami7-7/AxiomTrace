/**
 * @file    proto_types.h
 * @brief   MSPM0 双板 COBS 控制协议 v1.0 — 类型与常量定义
 * @note    冻结规范 Implementation Baseline，对应 protocol_implementation_spec.md v1.0.0
 *          严禁自行修改字段值、字节序或枚举顺序；如需变更必须修订协议版本。
 */
#ifndef PROTO_TYPES_H
#define PROTO_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * §3.3 帧常量
 * ======================================================================== */
#define PROTO_VERSION            0x01u   /**< 协议版本，固定 0x01 */
#define PROTO_HEADER_LEN         10u     /**< 固定头字段长度 (version..payload_len)，不含 CRC */
#define PROTO_CRC_LEN            2u      /**< CRC-16 长度 */
#define PROTO_FIXED_OVERHEAD     12u     /**< 固定开销 = HEADER_LEN + CRC_LEN = 10 + 2 */
#define PROTO_MAX_PAYLOAD        128u    /**< 最大 payload */
#define PROTO_MAX_DECODED        140u    /**< 最大逻辑帧 = FIXED_OVERHEAD + MAX_PAYLOAD = 12 + 128 */
#define PROTO_MAX_ENCODED        141u    /**< COBS 编码最坏长度 */
#define PROTO_MAX_WIRE           142u    /**< 最大线上帧 = 141 + 1 分隔符 */

/* §3.2 帧分隔符 */
#define PROTO_DELIMITER          0x00u   /**< 唯一帧分隔符；0xA5 等不重置同步 */

/* ========================================================================
 * §3.4 flags 位定义
 * ======================================================================== */
#define PROTO_FLAG_ACK_REQ       0x01u   /**< bit0: 要求 ACK/NACK */
#define PROTO_FLAG_IS_ACK        0x02u   /**< bit1: 当前帧是 ACK */
#define PROTO_FLAG_IS_NACK       0x04u   /**< bit2: 当前帧是 NACK */
#define PROTO_FLAG_IS_EVENT      0x08u   /**< bit3: 当前帧是异步事件 */
#define PROTO_FLAG_IS_PERIODIC   0x10u   /**< bit4: 周期状态/心跳类 */
#define PROTO_FLAG_IS_IDEMPOTENT 0x20u   /**< bit5: 幂等命令 */
#define PROTO_FLAG_RESERVED_MASK 0xC0u   /**< bit6-7: 必须为 0 */

/* HEARTBEAT 固定 flags */
#define PROTO_FLAGS_HEARTBEAT    0x30u   /**< IS_PERIODIC | IS_IDEMPOTENT */

/* ========================================================================
 * §3.5 地址定义
 * ======================================================================== */
#define PROTO_ADDR_HOST          0x01u   /**< 上位机逻辑节点 */
#define PROTO_ADDR_CONTROLLER    0x10u   /**< Board A (电机控制板) */
#define PROTO_ADDR_GATEWAY       0x20u   /**< Board B (BLE/OLED 网关) */
#define PROTO_ADDR_BROADCAST     0xFFu   /**< 广播 (当前点对点不使用) */

/* ========================================================================
 * §3.6 消息类别
 * ======================================================================== */
typedef enum {
    PROTO_MSG_INVALID    = 0x00,  /**< 非法 */
    PROTO_MSG_COMMAND    = 0x01,  /**< 下行命令 */
    PROTO_MSG_QUERY      = 0x02,  /**< 下行只读查询 */
    PROTO_MSG_STATUS     = 0x03,  /**< 上行状态快照 */
    PROTO_MSG_ACK        = 0x04,  /**< 上行成功接受/完成 */
    PROTO_MSG_NACK       = 0x05,  /**< 上行拒绝或失败 */
    PROTO_MSG_EVENT      = 0x06,  /**< 上行异步故障/状态变化 */
    PROTO_MSG_HEARTBEAT  = 0x07,  /**< 下行控制刷新 */
} proto_msg_class_t;

/* ========================================================================
 * §6.1 COMMAND opcodes (msg_class=0x01)
 * ======================================================================== */
typedef enum {
    PROTO_OP_STOP        = 0x01,  /**< 停止一个电机 (payload=2) */
    PROTO_OP_STOP_ALL    = 0x02,  /**< 停止全部电机 (payload=0) */
    PROTO_OP_ABORT       = 0x03,  /**< 终止位置/角度动作 (payload=0) */
    PROTO_OP_ENABLE      = 0x04,  /**< 允许电机输出 (payload=0) */
    PROTO_OP_DISABLE     = 0x05,  /**< 禁止电机输出 (payload=0) */
    PROTO_OP_RUN         = 0x06,  /**< 按已设置目标运行 (payload=0) */
    PROTO_OP_SET_TARGET  = 0x07,  /**< 设置单电机目标 (payload=10) */
    PROTO_OP_SET_MODE    = 0x08,  /**< 设置控制模式 (payload=2) */
    PROTO_OP_CLEAR_FAULT = 0x09,  /**< 清除可清除故障锁存 (payload=2) */
} proto_cmd_opcode_t;
/* ========================================================================
 * 用户自定义 COMMAND opcode 范围
 *
 * 0x01~0x09 为标准控制命令；0x20~0x3F 由项目业务自行定义。
 * 0x20~0x3F 仍然使用固定协议外壳，Board B 只转发，不解析业务 payload。
 * ======================================================================== */
#define PROTO_USER_OP_MIN        0x20u
#define PROTO_USER_OP_MAX        0x3Fu
#define PROTO_IS_USER_COMMAND_OPCODE(opcode) \
    ((uint8_t)(opcode) >= PROTO_USER_OP_MIN && (uint8_t)(opcode) <= PROTO_USER_OP_MAX)

/* ========================================================================
 * §6.3 QUERY opcodes (msg_class=0x02)
 * ======================================================================== */
typedef enum {
    PROTO_OP_QUERY_STATUS = 0x01,  /**< ACK + STATUS_SUMMARY + N×STATUS_MOTOR + STATUS_SENSOR */
    PROTO_OP_QUERY_INFO   = 0x02,  /**< ACK + STATUS_INFO */
    PROTO_OP_QUERY_FAULT  = 0x03,  /**< ACK + STATUS_FAULT */
    PROTO_OP_QUERY_SENSOR = 0x04,  /**< ACK + STATUS_SENSOR */
} proto_query_opcode_t;

/* ========================================================================
 * §6.4 STATUS opcodes (msg_class=0x03)
 * ======================================================================== */
typedef enum {
    PROTO_OP_STATUS_SUMMARY = 0x01,  /**< 20 字节 */
    PROTO_OP_STATUS_MOTOR   = 0x02,  /**< 26 字节 */
    PROTO_OP_STATUS_SENSOR  = 0x03,  /**< 46 字节 */
    PROTO_OP_STATUS_FAULT   = 0x04,  /**< 16 字节 */
    PROTO_OP_STATUS_INFO    = 0x05,  /**< 16 字节 */
} proto_status_opcode_t;

/* ========================================================================
 * §6.2/§6.5/§6.6 其他 opcodes
 * ======================================================================== */
#define PROTO_OP_ACK            0x01u   /**< ACK/NACK opcode 固定 0x01 */
#define PROTO_OP_HEARTBEAT      0x01u   /**< HEARTBEAT opcode 固定 0x01 */

typedef enum {
    PROTO_OP_EVT_FAULT        = 0x01,  /**< payload 使用 STATUS_FAULT 格式 */
    PROTO_OP_EVT_LINK_LOST    = 0x02,  /**< payload 8 字节 */
    PROTO_OP_EVT_STATE_CHANGED = 0x03, /**< payload 4 字节 */
} proto_event_opcode_t;

/* ========================================================================
 * §6.5 NACK error_code
 * ======================================================================== */
typedef enum {
    PROTO_NACK_BAD_VERSION       = 0x01,
    PROTO_NACK_BAD_FLAGS         = 0x02,
    PROTO_NACK_BAD_ADDRESS       = 0x03,
    PROTO_NACK_BAD_CLASS         = 0x04,
    PROTO_NACK_BAD_OPCODE        = 0x05,
    PROTO_NACK_BAD_LENGTH        = 0x06,
    PROTO_NACK_BAD_PARAM         = 0x07,
    PROTO_NACK_CRC_ERROR         = 0x08,
    PROTO_NACK_INVALID_STATE     = 0x09,
    PROTO_NACK_FAULT_LATCHED     = 0x0A,
    PROTO_NACK_LINK_NOT_READY    = 0x0B,
    PROTO_NACK_QUEUE_FULL        = 0x0C,
    PROTO_NACK_DUPLICATE_CONFLICT = 0x0D,
    PROTO_NACK_UNSUPPORTED       = 0x0E,
    PROTO_NACK_INTERNAL_ERROR    = 0x0F,
    PROTO_NACK_FAULT_CLEAR_DENIED = 0x10,
    PROTO_NACK_SEQ_REQUIRED      = 0x11,
    PROTO_NACK_BUSY              = 0x12,
} proto_nack_code_t;

/* ========================================================================
 * §6.5 ACK result_code
 * ======================================================================== */
typedef enum {
    PROTO_RESULT_ACCEPTED        = 0x00,  /**< 命令已接受并入队 */
    PROTO_RESULT_COMPLETED       = 0x01,  /**< 命令已同步完成 */
    PROTO_RESULT_ALREADY_IN_STATE = 0x02, /**< 幂等命令，目标状态已满足 */
} proto_ack_result_t;

/* ========================================================================
 * §6.5.2 NACK detail_code (按 error_code 分组)
 * ======================================================================== */
/* BAD_VERSION */
#define PROTO_DETAIL_BAD_VERSION_UNSUPPORTED   0x0001u
/* BAD_FLAGS */
#define PROTO_DETAIL_BAD_FLAGS_RESERVED        0x0001u
#define PROTO_DETAIL_BAD_FLAGS_CLASS_MISMATCH  0x0002u
/* BAD_LENGTH */
#define PROTO_DETAIL_BAD_LENGTH_PAYLOAD        0x0001u
#define PROTO_DETAIL_BAD_LENGTH_LOGICAL        0x0002u
/* BAD_PARAM */
#define PROTO_DETAIL_BAD_PARAM_MOTOR_ID        0x0001u
#define PROTO_DETAIL_BAD_PARAM_MODE            0x0002u
#define PROTO_DETAIL_BAD_PARAM_TARGET          0x0003u
#define PROTO_DETAIL_BAD_PARAM_LIMIT           0x0004u
#define PROTO_DETAIL_BAD_PARAM_STOP_TYPE       0x0005u
#define PROTO_DETAIL_BAD_PARAM_FAULT_CODE      0x0006u
/* INVALID_STATE */
#define PROTO_DETAIL_INVALID_STATE_NOT_ALLOWED 0x0001u
#define PROTO_DETAIL_INVALID_STATE_NOT_ENABLED 0x0002u
#define PROTO_DETAIL_INVALID_STATE_FAULT       0x0003u
/* LINK_NOT_READY */
#define PROTO_DETAIL_LINK_NO_HEARTBEAT         0x0001u
#define PROTO_DETAIL_LINK_STALE                0x0002u

/* ========================================================================
 * §6.1 控制模式
 * ======================================================================== */
typedef enum {
    PROTO_MODE_SPEED    = 0x00,
    PROTO_MODE_POSITION = 0x01,
    PROTO_MODE_ANGLE    = 0x02,
} proto_ctrl_mode_t;

/* §6.1 STOP stop_type */
#define PROTO_STOP_TYPE_CONTROLLED   0x00u
#define PROTO_STOP_TYPE_FAST_SAFETY  0x01u

/* ========================================================================
 * §6.1.1 fault_code
 * ======================================================================== */
typedef enum {
    PROTO_FAULT_NONE        = 0x0000,
    PROTO_FAULT_LINK_LOST   = 0x0001,
    PROTO_FAULT_OVERCURRENT = 0x0002,
    PROTO_FAULT_ENCODER     = 0x0003,
    PROTO_FAULT_IMU         = 0x0004,
    PROTO_FAULT_ADC         = 0x0005,
    PROTO_FAULT_OVERTEMP    = 0x0006,
} proto_fault_code_t;

/* ========================================================================
 * §7.1 Board A 安全状态机
 * ======================================================================== */
typedef enum {
    PROTO_STATE_INIT      = 0x00,
    PROTO_STATE_SAFE      = 0x01,
    PROTO_STATE_DISABLED  = 0x02,
    PROTO_STATE_ENABLED   = 0x03,
    PROTO_STATE_RUNNING   = 0x04,
    PROTO_STATE_FAULT     = 0x05,
    PROTO_STATE_LINK_LOST = 0x06,
} proto_state_t;

/* ========================================================================
 * §6.4.1 STATUS_MOTOR.motor_state
 * ======================================================================== */
typedef enum {
    PROTO_MOTOR_STOPPED  = 0x00,
    PROTO_MOTOR_READY    = 0x01,
    PROTO_MOTOR_RUNNING  = 0x02,
    PROTO_MOTOR_BRAKING  = 0x03,
    PROTO_MOTOR_FAULT    = 0x04,
    PROTO_MOTOR_DISABLED = 0x05,
} proto_motor_state_t;

/* ========================================================================
 * §6.4 STATUS_INFO device_type / board_role
 * ======================================================================== */
#define PROTO_DEVICE_MOTOR_CONTROLLER  0x01u
#define PROTO_DEVICE_GATEWAY           0x02u
#define PROTO_BOARD_ROLE_CONTROLLER    0x01u
#define PROTO_BOARD_ROLE_GATEWAY       0x02u

/* ========================================================================
 * §6.6 EVENT STATE_CHANGED reason
 * ======================================================================== */
typedef enum {
    PROTO_REASON_UNSPECIFIED  = 0x00,
    PROTO_REASON_COMMAND      = 0x01,
    PROTO_REASON_WATCHDOG_TIMEOUT = 0x02,
    PROTO_REASON_WATCHDOG     = PROTO_REASON_WATCHDOG_TIMEOUT, /* 旧名称兼容别名 */
    PROTO_REASON_FAULT        = 0x03,
    PROTO_REASON_RECOVERY     = 0x04,
    PROTO_REASON_INIT         = 0x05,
} proto_state_reason_t;

/* ========================================================================
 * §6.4 capability_flags 位定义
 * ======================================================================== */
#define PROTO_CAP_SPEED_CONTROL      (1u << 0)
#define PROTO_CAP_POSITION_CONTROL   (1u << 1)
#define PROTO_CAP_ANGLE_CONTROL      (1u << 2)
#define PROTO_CAP_MULTI_MOTOR        (1u << 3)
#define PROTO_CAP_IMU                (1u << 4)
#define PROTO_CAP_ADC                (1u << 5)
#define PROTO_CAP_ENCODER            (1u << 6)
#define PROTO_CAP_MOTOR_POWER_CTRL   (1u << 7)
#define PROTO_CAP_BLE                (1u << 8)
#define PROTO_CAP_OLED               (1u << 9)
#define PROTO_CAP_GATEWAY            (1u << 10)

/* ========================================================================
 * §6.4.1 sensor_flags 位定义
 * ======================================================================== */
#define PROTO_SENSOR_GYRO_VALID      (1u << 0)
#define PROTO_SENSOR_ACCEL_VALID     (1u << 1)
#define PROTO_SENSOR_ENCODER_VALID   (1u << 2)
#define PROTO_SENSOR_ADC0_VALID      (1u << 3)
#define PROTO_SENSOR_ADC1_VALID      (1u << 4)
#define PROTO_SENSOR_ADC2_VALID      (1u << 5)
#define PROTO_SENSOR_BUS_V_VALID     (1u << 6)
#define PROTO_SENSOR_MOTOR_I_VALID   (1u << 7)
#define PROTO_SENSOR_TEMP_VALID      (1u << 8)
#define PROTO_SENSOR_SAMPLE_T_VALID  (1u << 9)

/* ========================================================================
 * §6.4.1 safety_flags 位定义
 * ======================================================================== */
#define PROTO_SAFETY_MOTOR_OUT_EN    (1u << 0)
#define PROTO_SAFETY_ESTOP_ACTIVE    (1u << 1)
#define PROTO_SAFETY_WATCHDOG_ARMED  (1u << 2)
#define PROTO_SAFETY_LINK_LOST_LATCH (1u << 3)
#define PROTO_SAFETY_HW_FAULT        (1u << 4)
#define PROTO_SAFETY_SENSOR_FAULT    (1u << 5)
#define PROTO_SAFETY_OVERCURRENT     (1u << 6)
#define PROTO_SAFETY_OVERTEMP        (1u << 7)

/* ========================================================================
 * §6.4.1 fault_flags 位定义
 * ======================================================================== */
#define PROTO_FAULT_FLAG_ACTIVE       (1u << 0)
#define PROTO_FAULT_FLAG_LATCHED      (1u << 1)
#define PROTO_FAULT_FLAG_SINCE_BOOT   (1u << 2)
#define PROTO_FAULT_FLAG_POWER_CYCLE  (1u << 3)

/* ========================================================================
 * §5.1 帧视图 — 解码后的逻辑帧字段映射 (不拥有 payload 内存)
 * ======================================================================== */
typedef struct {
    uint8_t         version;
    uint8_t         flags;
    uint8_t         src;
    uint8_t         dst;
    uint8_t         msg_class;
    uint8_t         opcode;
    uint16_t        seq;
    uint16_t        payload_len;
    const uint8_t  *payload;    /**< 指向解码缓冲区内的 payload，不拥有内存 */
    uint16_t        crc16;      /**< 帧中携带的 CRC 值 */
} proto_frame_view_t;

/* ========================================================================
 * §5.1 协议统计
 * ======================================================================== */
typedef struct {
    uint32_t rx_frames;
    uint32_t rx_empty_frames;
    uint32_t rx_cobs_errors;
    uint32_t rx_crc_errors;
    uint32_t rx_length_errors;
    uint32_t rx_version_errors;
    uint32_t rx_address_errors;
    uint32_t rx_replay_hits;
    uint32_t rx_overflows;
    uint32_t tx_frames;
    uint32_t tx_dropped;
} proto_stats_t;

/* ========================================================================
 * §5.3 推荐的命令队列元素
 * ======================================================================== */
typedef struct {
    uint8_t  msg_class;
    uint8_t  opcode;
    uint8_t  src;
    uint8_t  dst;
    uint8_t  flags;
    uint16_t seq;
    uint16_t payload_len;
    uint8_t  payload[PROTO_MAX_PAYLOAD];
    uint32_t received_at_ms;
} proto_message_t;

/* ========================================================================
 * §6.5 ACK payload (6 字节)
 * ======================================================================== */
typedef struct {
    uint16_t original_seq;       /**< LE */
    uint8_t  original_opcode;
    uint8_t  result_code;        /**< proto_ack_result_t */
    uint16_t queue_generation;   /**< LE */
} proto_ack_payload_t;
#define PROTO_ACK_PAYLOAD_LEN  6u

/* ========================================================================
 * §6.5 NACK payload (8 字节)
 * ======================================================================== */
typedef struct {
    uint16_t original_seq;       /**< LE */
    uint8_t  original_opcode;
    uint8_t  error_code;         /**< proto_nack_code_t */
    uint16_t detail_code;        /**< LE */
    uint16_t retry_after_ms;     /**< LE */
} proto_nack_payload_t;
#define PROTO_NACK_PAYLOAD_LEN 8u

/* ========================================================================
 * §6.2 HEARTBEAT payload (4 字节)
 * ======================================================================== */
typedef struct {
    uint16_t control_epoch;      /**< LE */
    uint8_t  requested_mode;
    uint8_t  requested_run;
} proto_heartbeat_payload_t;
#define PROTO_HEARTBEAT_PAYLOAD_LEN 4u

/* ========================================================================
 * §8.2 Board B 事务表项
 * ======================================================================== */
typedef enum {
    PROTO_TXN_FREE             = 0,
    PROTO_TXN_WAIT_BOARD_ACK   = 1,
    PROTO_TXN_WAIT_BOARD_STATUS = 2,
    PROTO_TXN_DONE             = 3,
    PROTO_TXN_FAILED           = 4,
} proto_txn_state_t;

typedef struct {
    bool     used;
    uint16_t host_seq;
    uint16_t board_seq;
    uint8_t  host_msg_class;
    uint8_t  board_msg_class;
    uint8_t  host_opcode;
    uint8_t  board_opcode;
    uint32_t sent_ms;
    uint8_t  retry_count;
    uint8_t  state;              /**< proto_txn_state_t */
    uint16_t payload_len;
    uint8_t  payload[PROTO_MAX_PAYLOAD];
} gateway_transaction_t;

/* ========================================================================
 * §6.4 STATUS payload 长度常量
 * ======================================================================== */
#define PROTO_STATUS_INFO_LEN     16u   /**< STATUS_INFO payload 固定 16 字节 */
#define PROTO_STATUS_SUMMARY_LEN  20u   /**< STATUS_SUMMARY payload 固定 20 字节 */
#define PROTO_STATUS_MOTOR_LEN    26u   /**< STATUS_MOTOR payload 固定 26 字节 */
#define PROTO_STATUS_SENSOR_LEN   46u   /**< STATUS_SENSOR payload 固定 46 字节 */
#define PROTO_STATUS_FAULT_LEN    16u   /**< STATUS_FAULT payload 固定 16 字节 */

/* §6.6 EVENT payload 长度常量 */
#define PROTO_EVT_FAULT_LEN       16u   /**< EVT_FAULT payload = STATUS_FAULT 格式 */
#define PROTO_EVT_LINK_LOST_LEN    8u   /**< EVT_LINK_LOST payload 8 字节 */
#define PROTO_EVT_STATE_CHANGED_LEN 4u  /**< EVT_STATE_CHANGED payload 4 字节 */

/* ========================================================================
 * §6.4 状态快照结构体 (内存中使用，线上传输时必须通过显式 LE 读写)
 * @note  严禁直接序列化这些结构体；必须通过 proto_frame.h 的 LE 读写函数。
 * ======================================================================== */

/** STATUS_SUMMARY 快照 (规范 §6.4) */
typedef struct {
    uint8_t  state;               /**< proto_state_t */
    uint8_t  motor_count;
    uint16_t active_motor_mask;
    uint16_t fault_code;          /**< proto_fault_code_t */
    uint16_t safety_flags;
    uint16_t last_command_seq;
    uint16_t last_control_seq;
    uint32_t uptime_ms;
    uint16_t link_age_ms;         /**< 饱和到 0xFFFF */
    uint16_t status_generation;
} proto_snapshot_summary_t;

/** STATUS_MOTOR 快照 (规范 §6.4) */
typedef struct {
    uint8_t  motor_id;
    uint8_t  motor_state;         /**< proto_motor_state_t */
    uint8_t  mode;                /**< proto_ctrl_mode_t */
    uint8_t  power_enabled;       /**< 0x00 或 0x01 */
    int32_t  target_value;
    int32_t  speed_x100_rpm;
    int32_t  position_count;
    int32_t  current_ma;
    uint16_t voltage_mv;
    int16_t  temperature_x100_c;
    uint16_t fault_code;
} proto_snapshot_motor_t;

/** STATUS_SENSOR 快照 (规范 §6.4) */
typedef struct {
    uint32_t sample_time_ms;
    uint16_t sensor_flags;
    int32_t  gyro_x_x1000_dps;
    int32_t  gyro_y_x1000_dps;
    int32_t  gyro_z_x1000_dps;
    int32_t  accel_x_x1000_mg;
    int32_t  accel_y_x1000_mg;
    int32_t  accel_z_x1000_mg;
    int32_t  encoder_count;       /**< motor_count=1 时有效，多电机填 0 */
    uint16_t adc0_mv;
    uint16_t adc1_mv;
    uint16_t adc2_mv;
    uint16_t bus_voltage_mv;
    uint16_t motor_current_ma;    /**< 所有电机电流总和 */
    int16_t  temperature_x100_c;
} proto_snapshot_sensor_t;

/** STATUS_FAULT 快照 (规范 §6.4) */
typedef struct {
    uint16_t fault_code;          /**< proto_fault_code_t */
    uint16_t fault_flags;         /**< PROTO_FAULT_FLAG_* 位组合 */
    uint32_t first_seen_ms;
    uint32_t last_seen_ms;
    uint16_t repeat_count;
    uint8_t  active;              /**< 0x00 或 0x01，必须与 fault_flags.bit0 一致 */
    uint8_t  reserved;
} proto_snapshot_fault_t;

/** STATUS_INFO 快照 (规范 §6.4) */
typedef struct {
    uint8_t  device_type;         /**< PROTO_DEVICE_* */
    uint8_t  board_role;          /**< PROTO_BOARD_ROLE_* */
    uint8_t  protocol_version;    /**< PROTO_VERSION */
    uint8_t  hardware_revision;
    uint8_t  firmware_major;
    uint8_t  firmware_minor;
    uint8_t  firmware_patch;
    uint8_t  reserved;
    uint32_t capability_flags;    /**< PROTO_CAP_* 位组合 */
    uint32_t serial_number;
} proto_snapshot_info_t;

/* ========================================================================
 * §7.3 watchdog 参数
 * ======================================================================== */
#define PROTO_WATCHDOG_TIMEOUT_MS     200u
#define PROTO_HEARTBEAT_PERIOD_MS     50u
#define PROTO_HEARTBEAT_JITTER_MS     20u

/* §8.3 网关超时参数 */
#define PROTO_GATEWAY_ACK_TIMEOUT_MS  40u
#define PROTO_GATEWAY_MAX_RETRIES     2u
#define PROTO_GATEWAY_HOST_TIMEOUT_MS 250u
#define PROTO_HOST_CONTROL_LEASE_MS   300u
#define PROTO_GATEWAY_MAX_RETRY_DELAY_MS 1000u

/* §8.3 事务表/重放缓存最小容量 */
#define PROTO_GATEWAY_TXN_TABLE_SIZE  8u
#define PROTO_REPLAY_CACHE_SIZE       8u
#define PROTO_BOARD_A_CMD_QUEUE_SIZE  8u

#ifdef __cplusplus
}
#endif
#endif /* PROTO_TYPES_H */
