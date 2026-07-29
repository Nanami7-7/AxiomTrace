/**
 * @file    app_protocol_user.c
 * @brief   Board A 十个通用自定义业务模板实现。
 *
 * 这是用户后续主要修改的文件。
 * 每个模板都是一个独立的业务入口：
 * - 可以自定义 payload 格式；
 * - 可以调用已有控制接口；
 * - 可以只更新目标值，让控制任务异步执行；
 * - 不需要修改 COBS、CRC、UART、ACK/NACK 或协议核心。
 *
 * 默认模板不执行实际动作，只返回 COMPLETED，方便先验证 AB 通信链路。
 */
#include "app_protocol_user.h"
#include "project_config.h"
#include <stddef.h>
#include <stdio.h>

/* 当前保持为 0；改为 1 即同时启用前三个控制示例。 */
#define APP_PROTOCOL_ENABLE_MOTION_EXAMPLES  0U

/*
 * 速度环、位置环、角度环的启用开关。
 *
 * 当前按需求保持为 0：下面的控制示例只作为“可复制的中文模板”，
 * 不参与本次编译，也不会改变现有通信验证行为。
 * 后续准备启用前三个模板时，只需把 APP_PROTOCOL_ENABLE_MOTION_EXAMPLES 改为 1，
 * 具体 payload 长度直接在对应模板函数内检查，不再维护额外配置。
 */
#if APP_PROTOCOL_ENABLE_MOTION_EXAMPLES
#include "Algorithm/app_motion_control.h"
#include "Algorithm/app_position_control.h"
#include "app_main.h"

/* 按小端格式读取一个有符号 32 位整数。 */
static int32_t app_protocol_user_read_i32_le(const uint8_t *data)
{
    uint32_t value;

    value = ((uint32_t)data[0]) |
            ((uint32_t)data[1] << 8) |
            ((uint32_t)data[2] << 16) |
            ((uint32_t)data[3] << 24);
    return (int32_t)value;
}
#endif

/* 每个自定义模板默认允许 0~128 字节 payload。填写业务后可改成精确长度。 */
#define APP_USER_PAYLOAD_MIN_DEFAULT  0U
#define APP_USER_PAYLOAD_MAX_DEFAULT  128U

/* BAD_PARAM 的 detail_code 可按自己的 payload 字段继续扩展。 */
#define APP_USER_DETAIL_BAD_LENGTH    0x0100U
#define APP_USER_DETAIL_BAD_VALUE     0x0101U

/*
 * 自定义模块4只修改运行状态，不在协议任务中执行循迹算法。
 * 这样可以避免协议回调阻塞，并保证循迹算法在控制任务中固定周期运行。
 */
static volatile bool s_line_track_enable = false;
static volatile bool s_line_track_reset_pending = false;

/* ========================================================================
 * 上位机蓝牙发送说明（UART2 透明串口）
 * ========================================================================
 *
 * Board B 不需要为这十个模板增加新的业务解析代码：
 * Board B 只负责把 BLE UART2 收到的固定协议帧透明转发到 Board A UART1。
 *
 * 上位机通过蓝牙发送的是“二进制 COBS 协议帧”，不是 ASCII 文本命令。
 * 直接操作串口时，发送格式如下：
 *
 *   逻辑帧：
 *   [01] [01] [01] [20] [01] [OPCODE] [SEQ_L] [SEQ_H]
 *          [LEN_L] [LEN_H] [PAYLOAD...] [CRC_L] [CRC_H]
 *
 *   字段含义：
 *   version    = 0x01
 *   flags      = 0x01（ACK_REQ）
 *   src        = 0x01（上位机）
 *   dst        = 0x20（Board B 网关；Board B 再转发到 Board A）
 *   msg_class  = 0x01（COMMAND）
 *   opcode     = 0x20~0x29（对应模板01~10）
 *   seq        = 上位机递增事务序号，小端
 *   len        = 自定义 payload 长度，小端，最大128
 *   CRC        = CRC16-CCITT-FALSE，小端
 *
 *   逻辑帧完成后：
 *   1. 对“逻辑帧（含CRC）”做 COBS 编码；
 *   2. 在 COBS 数据末尾追加 0x00 作为帧分隔符；
 *   3. 通过蓝牙串口发送得到的二进制字节。
 *
 * 示例：向模板01（opcode=0x20）发送 payload=11 22 33，seq=1：
 *   逻辑帧：01 01 01 20 01 20 01 00 03 00 11 22 33 41 5B
 *   发送帧：08 01 01 01 20 01 20 01 02 03 06 11 22 33 41 5B 00
 *
 * 注意：示例中的 CRC 只适用于 payload=11 22 33、seq=1。
 * 更换 opcode、seq 或 payload 后，必须重新计算 payload_len、CRC 和 COBS。
 * Board A 处理完成后，会经 Board B 和 BLE 返回 ACK/NACK。
 * ======================================================================== */
/*
 * 调试辅助：Board A 通过 UART0 打印收到的自定义命令。
 * 后续正式运行时如果不需要日志，只删除这个函数和各模板中的调用即可。
 */
static void app_protocol_user_print_received(uint8_t opcode,
                                             const uint8_t *payload,
                                             uint16_t payload_len)
{
    uint16_t i;

    printf("[PROTO A] custom opcode=0x%02X len=%u payload=",
           (unsigned int)opcode, (unsigned int)payload_len);
    for (i = 0U; i < payload_len; ++i) {
        printf("%02X", (unsigned int)payload[i]);
        if ((uint16_t)(i + 1U) < payload_len) {
            printf(" ");
        }
    }
    printf("\r\n");
}
/* ========================================================================
 * 十个通用自定义模板
 * ========================================================================
 *
 * 使用建议：
 * 1. 先在注释中写清楚 payload 字节布局；
 * 2. 检查 payload_len 和字段范围；
 * 3. 调用现有业务接口或更新控制目标；
 * 4. 根据执行结果返回 ACCEPTED、COMPLETED、BAD_PARAM 等结果。
 *
 * 不要在这里长时间阻塞，不要在这里实现完整控制环。
 */

static proto_user_result_t app_custom_template_01(
    const uint8_t *payload, uint16_t payload_len, uint16_t *detail_code)
{
    /* 自定义模板 01：速度环，opcode=0x20。 */
    app_protocol_user_print_received(0x20U, payload, payload_len);
    (void)detail_code;

    /*
     * 【速度环示例，当前关闭】
     *
     * 上位机 payload 固定为 8 字节，小端格式：
     *   payload[0..3]：目标速度，单位 RPM，int32_le，可为负数；
     *   payload[4..7]：速度限幅，单位 RPM，int32_le，必须大于 0。
     *
     * 例如：目标速度=100RPM，限幅=1000RPM：
     *   64 00 00 00 E8 03 00 00
     *
     * Board B 不需要理解这 8 字节，只转发完整协议帧；
     * Board A 在这里解析后，调用现有速度控制门面。
     * 取消注释步骤：
     *   1. 将 APP_PROTOCOL_ENABLE_MOTION_EXAMPLES 改为 1；
     *   2. 本函数中的示例代码会随该宏一起启用；
     *   3. 不需要修改其他配置；长度判断已经写在本模板中；
     *   4. 重新编译并先用小速度验证。
     */
#if APP_PROTOCOL_ENABLE_MOTION_EXAMPLES
    int32_t target_speed_rpm;
    int32_t speed_limit_rpm;
    float target_rpm;
    float rpm_targets[APP_MOTION_MOTOR_COUNT];
    uint32_t i;

    if ((payload == NULL) || (payload_len != 8U)) {
        if (detail_code != NULL) {
            *detail_code = APP_USER_DETAIL_BAD_LENGTH;
        }
        return PROTO_USER_RESULT_BAD_PARAM;
    }

    target_speed_rpm = app_protocol_user_read_i32_le(&payload[0]);
    speed_limit_rpm = app_protocol_user_read_i32_le(&payload[4]);
    if (speed_limit_rpm <= 0) {
        return PROTO_USER_RESULT_BAD_PARAM;
    }

    /* 先限幅，再把相同目标速度发送给四个电机。 */
    if (target_speed_rpm > speed_limit_rpm) {
        target_speed_rpm = speed_limit_rpm;
    } else if (target_speed_rpm < -speed_limit_rpm) {
        target_speed_rpm = -speed_limit_rpm;
    }

    target_rpm = (float)target_speed_rpm;
    for (i = 0U; i < APP_MOTION_MOTOR_COUNT; ++i) {
        rpm_targets[i] = target_rpm;
    }

    return app_motion_speed_all(rpm_targets)
               ? PROTO_USER_RESULT_COMPLETED
               : PROTO_USER_RESULT_INTERNAL_ERROR;
#endif

    /* 当前仍保持模板默认行为：只验证通信，不驱动电机。 */
    return PROTO_USER_RESULT_COMPLETED;
}

static proto_user_result_t app_custom_template_02(
    const uint8_t *payload, uint16_t payload_len, uint16_t *detail_code)
{
    /* 自定义模板 02：位置环，opcode=0x21。 */
    app_protocol_user_print_received(0x21U, payload, payload_len);
    (void)detail_code;

    /*
     * 【位置环示例，当前关闭】
     *
     * 上位机 payload 固定为 12 字节，小端格式：
     *   payload[0..3]：目标位移，单位 mm，int32_le，可为负数；
     *   payload[4..7]：最大速度，单位 RPM，int32_le，必须大于 0；
     *   payload[8..11]：最大加速度，单位 RPM/s，int32_le，必须大于 0。
     *
     * 例如：移动 500mm，最大速度 200RPM，加速度 1000RPM/s：
     *   F4 01 00 00 C8 00 00 00 E8 03 00 00
     *
     * 位置控制门面会把米、RPM 和编码器参数转换交给现有控制任务；
     * 这里不要等待动作完成，也不要在回调中实现 PID。
     * 取消注释步骤：
     *   1. 将 APP_PROTOCOL_ENABLE_MOTION_EXAMPLES 改为 1；
     *   2. 本函数中的示例代码会随该宏一起启用；
     *   3. 不需要修改其他配置；长度判断已经写在本模板中；
     *   4. 重新编译后先使用较小位移验证。
     */
#if APP_PROTOCOL_ENABLE_MOTION_EXAMPLES
    int32_t target_position_mm;
    int32_t max_speed_rpm;
    int32_t max_accel_rpm_s;
    app_shared_ctx_t *ctx;

    if ((payload == NULL) || (payload_len != 12U)) {
        if (detail_code != NULL) {
            *detail_code = APP_USER_DETAIL_BAD_LENGTH;
        }
        return PROTO_USER_RESULT_BAD_PARAM;
    }

    target_position_mm = app_protocol_user_read_i32_le(&payload[0]);
    max_speed_rpm = app_protocol_user_read_i32_le(&payload[4]);
    max_accel_rpm_s = app_protocol_user_read_i32_le(&payload[8]);
    if ((max_speed_rpm <= 0) || (max_accel_rpm_s <= 0)) {
        return PROTO_USER_RESULT_BAD_PARAM;
    }

    ctx = app_protocol_get_context();
    if (ctx == NULL) {
        return PROTO_USER_RESULT_INTERNAL_ERROR;
    }

    app_posctrl_set_accel(&ctx->posctrl, (float)max_accel_rpm_s);
    return app_motion_position_start(
               (float)target_position_mm / 1000.0f,
               (float)max_speed_rpm,
               0U)
               ? PROTO_USER_RESULT_ACCEPTED
               : PROTO_USER_RESULT_INTERNAL_ERROR;
#endif

    /* 当前仍保持模板默认行为：只验证通信，不驱动电机。 */
    return PROTO_USER_RESULT_COMPLETED;
}

static proto_user_result_t app_custom_template_03(
    const uint8_t *payload, uint16_t payload_len, uint16_t *detail_code)
{
    /* 自定义模板 03：角度环，opcode=0x22。 */
    app_protocol_user_print_received(0x22U, payload, payload_len);
    (void)detail_code;

    /*
     * 【角度环示例，当前关闭】
     *
     * 上位机 payload 固定为 8 字节，小端格式：
     *   payload[0..3]：目标相对角度，单位 0.01°，int32_le，可为负数；
     *   payload[4..7]：角速度限幅，单位 RPM，int32_le，必须大于 0。
     *
     * 例如：相对旋转 90.00°，角速度限幅 100RPM：
     *   28 23 00 00 64 00 00 00
     *
     * 这里使用“相对角度”接口，正负方向沿现有 IMU yaw 约定；
     * 如果以后需要绝对角度，把接口替换为
     * app_motion_angle_start_absolute() 即可。
     * 取消注释步骤：
     *   1. 将 APP_PROTOCOL_ENABLE_MOTION_EXAMPLES 改为 1；
     *   2. 本函数中的示例代码会随该宏一起启用；
     *   3. 不需要修改其他配置；长度判断已经写在本模板中；
     *   4. 确认 IMU yaw 已正常，再从小角度开始验证。
     */
#if APP_PROTOCOL_ENABLE_MOTION_EXAMPLES
    int32_t angle_x100;
    int32_t speed_limit_rpm;

    if ((payload == NULL) || (payload_len != 8U)) {
        if (detail_code != NULL) {
            *detail_code = APP_USER_DETAIL_BAD_LENGTH;
        }
        return PROTO_USER_RESULT_BAD_PARAM;
    }

    angle_x100 = app_protocol_user_read_i32_le(&payload[0]);
    speed_limit_rpm = app_protocol_user_read_i32_le(&payload[4]);
    if (speed_limit_rpm <= 0) {
        return PROTO_USER_RESULT_BAD_PARAM;
    }

    return app_motion_angle_start_relative(
               (float)angle_x100 / 100.0f,
               (float)speed_limit_rpm,
               0U)
               ? PROTO_USER_RESULT_ACCEPTED
               : PROTO_USER_RESULT_INTERNAL_ERROR;
#endif

    /* 当前仍保持模板默认行为：只验证通信，不驱动电机。 */
    return PROTO_USER_RESULT_COMPLETED;
}

static proto_user_result_t app_custom_template_04(
    const uint8_t *payload, uint16_t payload_len, uint16_t *detail_code)
{
    /*
     * 自定义模块4：循迹控制命令，opcode=0x23。
     * payload[0]：0x00停止，0x01启动，0x02复位。
     */
    if ((payload == NULL) || (payload_len != 1U)) {
        if (detail_code != NULL) {
            *detail_code = APP_USER_DETAIL_BAD_LENGTH;
        }
        return PROTO_USER_RESULT_BAD_PARAM;
    }

    app_protocol_user_print_received(0x23U, payload, payload_len);

    switch (payload[0]) {
    case APP_USER_LINE_TRACK_STOP_RESET:
        /* 停止循迹，复位动作由控制任务执行，避免跨任务直接改算法状态。 */
        s_line_track_enable = false;
        s_line_track_reset_pending = true;
        return PROTO_USER_RESULT_COMPLETED;

    case APP_USER_LINE_TRACK_START:
        /* 启动前先复位一次，避免沿用上一次丢线方向记忆。 */
        app_protocol_user_line_track_request_start();
        return PROTO_USER_RESULT_ACCEPTED;

    case APP_USER_LINE_TRACK_RESET:
        /* 只复位算法，不改变当前启停状态。 */
        s_line_track_reset_pending = true;
        return PROTO_USER_RESULT_COMPLETED;

    default:
        if (detail_code != NULL) {
            *detail_code = APP_USER_DETAIL_BAD_VALUE;
        }
        return PROTO_USER_RESULT_BAD_PARAM;
    }
}

bool app_protocol_user_line_track_is_enabled(void)
{
    return s_line_track_enable;
}

void app_protocol_user_line_track_request_start(void)
{
    /* 与协议启动命令保持同一语义：启动前由控制任务复位算法状态。 */
    s_line_track_enable = true;
    s_line_track_reset_pending = true;
}

bool app_protocol_user_line_track_take_reset_request(void)
{
    bool pending = s_line_track_reset_pending;
    s_line_track_reset_pending = false;
    return pending;
}

void app_protocol_user_line_track_force_stop(void)
{
    /* 安全故障后禁止循迹按原请求自动重启，必须由上位机重新发送启动命令。 */
    s_line_track_enable = false;
    s_line_track_reset_pending = true;
}
static proto_user_result_t app_custom_template_05(
    const uint8_t *payload, uint16_t payload_len, uint16_t *detail_code)
{
    /* 自定义模板 05：上位机发送 COMMAND，opcode=0x24；在这里定义 payload 和业务。 */
    app_protocol_user_print_received(0x24U, payload, payload_len);
    (void)detail_code;

    /* 在这里填写你的第 5 个自定义控制命令。 */
    return PROTO_USER_RESULT_COMPLETED;
}

static proto_user_result_t app_custom_template_06(
    const uint8_t *payload, uint16_t payload_len, uint16_t *detail_code)
{
    /* 自定义模板 06：上位机发送 COMMAND，opcode=0x25；在这里定义 payload 和业务。 */
    app_protocol_user_print_received(0x25U, payload, payload_len);
    (void)detail_code;

    /* 在这里填写你的第 6 个自定义控制命令。 */
    return PROTO_USER_RESULT_COMPLETED;
}

static proto_user_result_t app_custom_template_07(
    const uint8_t *payload, uint16_t payload_len, uint16_t *detail_code)
{
    /* 自定义模板 07：上位机发送 COMMAND，opcode=0x26；在这里定义 payload 和业务。 */
    app_protocol_user_print_received(0x26U, payload, payload_len);
    (void)detail_code;

    /* 在这里填写你的第 7 个自定义控制命令。 */
    return PROTO_USER_RESULT_COMPLETED;
}

static proto_user_result_t app_custom_template_08(
    const uint8_t *payload, uint16_t payload_len, uint16_t *detail_code)
{
    /* 自定义模板 08：上位机发送 COMMAND，opcode=0x27；在这里定义 payload 和业务。 */
    app_protocol_user_print_received(0x27U, payload, payload_len);
    (void)detail_code;

    /* 在这里填写你的第 8 个自定义控制命令。 */
    return PROTO_USER_RESULT_COMPLETED;
}

static proto_user_result_t app_custom_template_09(
    const uint8_t *payload, uint16_t payload_len, uint16_t *detail_code)
{
    /* 自定义模板 09：上位机发送 COMMAND，opcode=0x28；在这里定义 payload 和业务。 */
    app_protocol_user_print_received(0x28U, payload, payload_len);
    (void)detail_code;

    /* 在这里填写你的第 9 个自定义控制命令。 */
    return PROTO_USER_RESULT_COMPLETED;
}

static proto_user_result_t app_custom_template_10(
    const uint8_t *payload, uint16_t payload_len, uint16_t *detail_code)
{
    /* 自定义模板 10：上位机发送 COMMAND，opcode=0x29；在这里定义 payload 和业务。 */
    app_protocol_user_print_received(0x29U, payload, payload_len);
    (void)detail_code;

    /* 在这里填写你的第 10 个自定义控制命令。 */
    return PROTO_USER_RESULT_COMPLETED;
}

/*
 * 自定义业务直接分发。
 *
 * 这里故意不使用配置表：本工程是一次性用途，后续增加功能时尽量少改文件。
 * 新增一个业务通常只需要：
 *   1. 写一个 app_custom_template_xx() 函数；
 *   2. 在下面的 switch 中增加一个 case。
 *
 * get_info() 统一返回自定义区的通用范围；具体 payload 长度由各业务函数自己检查。
 */
bool app_protocol_user_get_info(uint8_t opcode,
                                uint16_t *min_payload_len,
                                uint16_t *max_payload_len,
                                bool *idempotent)
{
    /* 0x20~0x3F 预留给本工程自定义业务。 */
    if ((opcode < 0x20U) || (opcode > 0x3FU)) {
        return false;
    }

    if (min_payload_len != NULL) {
        *min_payload_len = APP_USER_PAYLOAD_MIN_DEFAULT;
    }
    if (max_payload_len != NULL) {
        *max_payload_len = APP_USER_PAYLOAD_MAX_DEFAULT;
    }
    if (idempotent != NULL) {
        /* 循迹启停和复位都是幂等操作，其余模板默认按非幂等处理。 */
        *idempotent = (opcode == APP_USER_OP_CUSTOM_04);
    }
    return true;
}

proto_user_result_t app_protocol_user_execute(uint8_t opcode,
                                              const uint8_t *payload,
                                              uint16_t payload_len,
                                              uint16_t *detail_code)
{
    if (detail_code != NULL) {
        *detail_code = 0U;
    }

    /* 统一检查协议允许的最大 payload；固定长度由各模板自己检查。 */
    if ((payload_len > APP_USER_PAYLOAD_MAX_DEFAULT) ||
        ((payload_len > 0U) && (payload == NULL))) {
        if (detail_code != NULL) {
            *detail_code = APP_USER_DETAIL_BAD_LENGTH;
        }
        return PROTO_USER_RESULT_BAD_PARAM;
    }

    switch (opcode) {
    case APP_USER_OP_CUSTOM_01:
        return app_custom_template_01(payload, payload_len, detail_code);
    case APP_USER_OP_CUSTOM_02:
        return app_custom_template_02(payload, payload_len, detail_code);
    case APP_USER_OP_CUSTOM_03:
        return app_custom_template_03(payload, payload_len, detail_code);
    case APP_USER_OP_CUSTOM_04:
        return app_custom_template_04(payload, payload_len, detail_code);
    case APP_USER_OP_CUSTOM_05:
        return app_custom_template_05(payload, payload_len, detail_code);
    case APP_USER_OP_CUSTOM_06:
        return app_custom_template_06(payload, payload_len, detail_code);
    case APP_USER_OP_CUSTOM_07:
        return app_custom_template_07(payload, payload_len, detail_code);
    case APP_USER_OP_CUSTOM_08:
        return app_custom_template_08(payload, payload_len, detail_code);
    case APP_USER_OP_CUSTOM_09:
        return app_custom_template_09(payload, payload_len, detail_code);
    case APP_USER_OP_CUSTOM_10:
        return app_custom_template_10(payload, payload_len, detail_code);
    default:
        /* 0x2A~0x3F 可以以后直接新增 case；未实现的返回 UNSUPPORTED。 */
        return PROTO_USER_RESULT_UNSUPPORTED;
    }
}
