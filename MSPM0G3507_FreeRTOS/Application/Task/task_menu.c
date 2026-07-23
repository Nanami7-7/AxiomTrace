/**
 * @file    task_menu.c
 * @brief   鑿滃崟浠诲姟瀹炵幇(CLI妯″紡)
 * @note    CLI妯″紡: 鏄剧ず鐘舵€?鈫?绛夊緟鍛戒护 鈫?鎵ц 鈫?鍒锋柊
 *          Run鍛戒护杩涘叆鏁版嵁杈撳嚭妯″紡(30ms VOFA+鏁版嵁)
 *          Stop鍛戒护閫€鍑烘暟鎹緭鍑烘ā寮? */
#include "task_menu.h"
#include "app_main.h"
#include "app_pid.h"
#include "app_feedforward.h"
#include "app_vofa.h"
#include "app_complementary_filter.h"
#include "osal_api.h"
#include "bsp_led.h"
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "bsp_uart.h"
#include "app_debug.h"
#include "project_config.h"
#if (PRJ_BLE_MENU_ENABLE != 0U)
#include "app_ble_service.h"
#endif
#include "app_test_runner.h"
#include "axiomtrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================== 绉佹湁甯搁噺 ======================== */

/** 鐢垫満鍚嶇О鏌ユ壘琛?*/
static const char *s_motor_names[BSP_MOTOR_COUNT] = {
    "A", "B", "C", "D"
};

/** LED蹇冭烦鍛ㄦ湡(ms) */
#define MENU_LED_PERIOD_MS  (100U)

/** LED缈昏浆闃堝€?ms) */
#define LED_TOGGLE_THRESH   (500U)

/* ======================== 绉佹湁鍑芥暟: 琛岃緭鍏?======================== */

/**
 * @brief  闈為樆濉炶杈撳叆
 * @param  line_buf  琛岀紦鍐插尯
 * @param  buf_size  缂撳啿鍖哄ぇ灏? * @param  line_pos  褰撳墠鍐欏叆浣嶇疆鎸囬拡(璇诲啓)
 * @retval true  涓€琛岃緭鍏ュ畬鎴? * @retval false 灏氭湭瀹屾垚
 */
typedef enum {
    MENU_DRVSCOPE_START = 0,
    MENU_DRVSCOPE_SET_ONE,
    MENU_DRVSCOPE_SET_ALL,
    MENU_DRVSCOPE_STATUS,
    MENU_DRVSCOPE_OFF,
} menu_drvscope_action_t;

typedef struct {
    menu_drvscope_action_t action;
    uint32_t motor_id;
    uint32_t duty_permille; /* 0..1000 = 0.0..100.0% */
} menu_drvscope_cmd_t;

static char menu_ascii_lower(char ch);

#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U)
static bool menu_read_ble_line(char *line_buf, uint32_t buf_size,
                               uint32_t *line_pos, bool *discard_line);
#endif

/** Parse: encdiag cap [A|B|C|D|ALL] [window_ms]. */
static bool menu_parse_encdiag_capture(const char *line,
                                       uint32_t *motor_id,
                                       uint32_t *duration_ms)
{
    if ((line == NULL) || (motor_id == NULL) || (duration_ms == NULL)) {
        return false;
    }

    if (strcmp(line, "encdiag cap") == 0) {
        *motor_id = BSP_ENCODER_COUNT;
        *duration_ms = 1000U;
        return true;
    }

    char target[8] = {0};
    unsigned long window = 1000UL;
    int parsed = sscanf(line, "encdiag cap %7s %lu", target, &window);
    if (parsed < 1) {
        return false;
    }

    for (uint32_t i = 0U; target[i] != '\0'; i++) {
        target[i] = menu_ascii_lower(target[i]);
    }

    if (strcmp(target, "all") == 0) {
        *motor_id = BSP_ENCODER_COUNT;
    } else if ((target[0] >= 'a') && (target[0] <= 'd') &&
               (target[1] == '\0')) {
        *motor_id = (uint32_t)(target[0] - 'a');
    } else {
        return false;
    }

    if (parsed >= 2) {
        if (window < 100UL) {
            window = 100UL;
        }
        if (window > 10000UL) {
            window = 10000UL;
        }
    }
    *duration_ms = (uint32_t)window;
    return true;
}

static void menu_print_encdiag_capture_usage(void)
{
    (void)printf("Usage: encdiag cap [A|B|C|D|ALL] [100..10000_ms]\r\n");
    (void)printf("Example: encdiag cap A 1000\r\n");
}

static char menu_ascii_lower(char ch)
{
    if ((ch >= 'A') && (ch <= 'Z')) {
        return (char)(ch + ('a' - 'A'));
    }
    return ch;
}

static bool menu_token_equal_ci(const char *lhs, const char *rhs)
{
    if ((lhs == NULL) || (rhs == NULL)) {
        return false;
    }
    while ((*lhs != '\0') && (*rhs != '\0')) {
        if (menu_ascii_lower(*lhs) != menu_ascii_lower(*rhs)) {
            return false;
        }
        lhs++;
        rhs++;
    }
    return (*lhs == '\0') && (*rhs == '\0');
}

static bool menu_next_token(const char **cursor, char *token,
                            uint32_t token_size)
{
    const char *p;
    uint32_t length = 0U;

    if ((cursor == NULL) || (*cursor == NULL) ||
        (token == NULL) || (token_size < 2U)) {
        return false;
    }
    p = *cursor;
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0') {
        *cursor = p;
        return false;
    }
    while ((*p != '\0') && (*p != ' ')) {
        if (length >= (token_size - 1U)) {
            return false;
        }
        token[length++] = *p++;
    }
    token[length] = '\0';
    *cursor = p;
    return true;
}

static bool menu_parse_duty_permille(const char *token,
                                      uint32_t *duty_permille)
{
    uint32_t whole = 0U;
    uint32_t decimal = 0U;
    const char *p = token;

    if ((token == NULL) || (duty_permille == NULL) ||
        (*p < '0') || (*p > '9')) {
        return false;
    }
    while ((*p >= '0') && (*p <= '9')) {
        whole = (whole * 10U) + (uint32_t)(*p - '0');
        if (whole > 100U) {
            return false;
        }
        p++;
    }
    if (*p == '.') {
        p++;
        if ((*p < '0') || (*p > '9')) {
            return false;
        }
        decimal = (uint32_t)(*p - '0');
        p++;
    }
    if ((*p != '\0') || ((whole == 100U) && (decimal != 0U))) {
        return false;
    }

    *duty_permille = (whole * 10U) + decimal;
    return true;
}

static bool menu_parse_drvscope(const char *line_buf,
                                menu_drvscope_cmd_t *cmd)
{
    const char *cursor = line_buf;
    char command[12];
    char arg1[12];
    char arg2[12];
    char extra[MENU_LINE_BUF_SIZE];

    if ((line_buf == NULL) || (cmd == NULL) ||
        !menu_next_token(&cursor, command, sizeof(command)) ||
        !menu_token_equal_ci(command, "drvscope") ||
        !menu_next_token(&cursor, arg1, sizeof(arg1))) {
        return false;
    }

    if (menu_token_equal_ci(arg1, "start") ||
        menu_token_equal_ci(arg1, "status") ||
        menu_token_equal_ci(arg1, "off") ||
        menu_token_equal_ci(arg1, "stop")) {
        if (menu_next_token(&cursor, extra, sizeof(extra))) {
            return false;
        }
        if (menu_token_equal_ci(arg1, "start")) {
            cmd->action = MENU_DRVSCOPE_START;
        } else if (menu_token_equal_ci(arg1, "status")) {
            cmd->action = MENU_DRVSCOPE_STATUS;
        } else {
            cmd->action = MENU_DRVSCOPE_OFF;
        }
        return true;
    }

    if (!menu_next_token(&cursor, arg2, sizeof(arg2)) ||
        menu_next_token(&cursor, extra, sizeof(extra)) ||
        !menu_parse_duty_permille(arg2, &cmd->duty_permille)) {
        return false;
    }
    if (menu_token_equal_ci(arg1, "all")) {
        cmd->action = MENU_DRVSCOPE_SET_ALL;
        return true;
    }
    if ((arg1[1] == '\0') &&
        (menu_ascii_lower(arg1[0]) >= 'a') &&
        (menu_ascii_lower(arg1[0]) <= 'd')) {
        cmd->action = MENU_DRVSCOPE_SET_ONE;
        cmd->motor_id = (uint32_t)(menu_ascii_lower(arg1[0]) - 'a');
        return true;
    }
    return false;
}

static void menu_print_drvscope_usage(void)
{
    (void)printf("Usage:\r\n");
    (void)printf("  drvscope start       - all PWM=50.0%%, then PB19 ON (persistent)\r\n");
    (void)printf("  drvscope A 80        - A/M1 direct PWM duty=80.0%%\r\n");
    (void)printf("  drvscope B 20.5      - B/M2 direct PWM duty=20.5%%\r\n");
    (void)printf("  drvscope all 50      - set all four channels to 50.0%%\r\n");
    (void)printf("  drvscope status      - print PB19 and all compare values\r\n");
    (void)printf("  drvscope off|stop    - all PWM neutral, PB19 OFF, release lock\r\n");
}

static bool menu_read_line(char *line_buf, uint32_t buf_size,
                             uint32_t *line_pos)
{
    uint8_t ch;

    while (bsp_uart_getc(&ch) == BSP_OK) {
        if (ch == '\r' || ch == '\n') {
            line_buf[*line_pos] = '\0';
            (void)printf("\r\n");
            *line_pos = 0U;
            return true;
        } else if (ch == 0x7FU || ch == 0x08U) {
            if (*line_pos > 0U) {
                (*line_pos)--;
                (void)printf("\b \b");
            }
        } else if (ch >= 0x20U && ch < 0x7FU) {
            if (*line_pos < (buf_size - 1U)) {
                line_buf[*line_pos] = (char)ch;
                (*line_pos)++;
                (void)bsp_uart_putc(ch);
            }
        }
    }

    return false;
}

/* ======================== 绉佹湁鍑芥暟: 鐘舵€佹樉绀?======================== */

/**
 * @brief  鎵撳嵃褰撳墠鐘舵€?鐢垫満鍙傛暟+FF鐘舵€?IMU鏁版嵁)
 */
static void menu_print_status(const app_shared_ctx_t *ctx,
                               uint32_t motor)
{
    (void)printf("\r\n=== Motor: %s ===\r\n", s_motor_names[motor]);

    /* 褰撳墠鐩爣RPM */
    {
        float sp;
        OSAL_CRITICAL_SECTION {
            sp = ctx->pid[motor].setpoint;
        }
        (void)printf("Target: %.0f RPM\r\n", (double)sp);
    }

    /* 鍚勭數鏈篜ID鍙傛暟 */
    for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
        float kp, ki, kd;
        OSAL_CRITICAL_SECTION {
            kp = ctx->pid[i].kp;
            ki = ctx->pid[i].ki;
            kd = ctx->pid[i].kd;
        }
        (void)printf("%c: KP=%.2f KI=%.2f KD=%.2f\r\n",
            'A' + (int)i,
            (double)kp, (double)ki, (double)kd);
    }

    /* FF鐘舵€?*/
    {
        float ff_k, ff_b, ff_kp, ff_ki, ff_kd;
        bool ff_en;
        OSAL_CRITICAL_SECTION {
            ff_k = ctx->ff[motor].k;
            ff_b = ctx->ff[motor].b;
            ff_en = ctx->ff[motor].enabled;
            ff_kp = ctx->pid[motor].ff_kp;
            ff_ki = ctx->pid[motor].ff_ki;
            ff_kd = ctx->pid[motor].ff_kd;
        }
        if (ff_en) {
            (void)printf("FF: ON  k=%.3f  b=%.1f\r\n",
                (double)ff_k, (double)ff_b);
            (void)printf("FF_PID: KP=%.2f KI=%.2f KD=%.2f\r\n",
                (double)ff_kp, (double)ff_ki, (double)ff_kd);
        } else {
            (void)printf("FF: OFF\r\n");
        }
    }

    /* 鐢垫満杩愯鐘舵€?*/
    {
        bool en;
        int32_t rpm;
        OSAL_CRITICAL_SECTION {
            en = ctx->motor_enabled[motor];
            rpm = ctx->status.rpm[motor];
        }
        if (en) {
            (void)printf("Motor: running (%ld RPM)\r\n", (long)rpm);
        } else {
            (void)printf("Motor: stopped\r\n");
        }
    }

    /* IMU鏁版嵁 */
    {
        float roll, pitch, yaw, heading, vx;
        OSAL_CRITICAL_SECTION {
            roll  = ctx->imu.roll;
            pitch = ctx->imu.pitch;
            yaw   = ctx->imu.yaw;
        }
        heading = app_cf_get_heading();
        vx      = app_cf_get_velocity_x();
        (void)printf("IMU: R=%.1f P=%.1f Y=%.1f H=%.1f V=%.3f\r\n",
            (double)roll, (double)pitch, (double)yaw,
            (double)heading, (double)vx);
    }
}

/* ======================== 绉佹湁鍑芥暟: 鏁版嵁杈撳嚭寰幆 ======================== */

/**
 * @brief  VOFA+鏁版嵁杈撳嚭妯″紡(Run鍚庤繘鍏? Stop閫€鍑?
 * @param  ctx          鍏变韩涓婁笅鏂? * @param  motor        褰撳墠鐢垫満绱㈠紩鎸囬拡
 * @param  need_refresh 鍒锋柊鏍囧織鎸囬拡
 */
static void menu_data_output_loop(app_shared_ctx_t *ctx,
                                   uint32_t *motor,
                                   bool *need_refresh)
{
    char line_buf[MENU_LINE_BUF_SIZE];
    uint32_t line_pos = 0U;
#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U)
    uint32_t ble_line_pos = 0U;
    bool ble_line_discard = false;
#endif
    uint32_t led_cnt = 0U;

    (void)printf("[DATA] VOFA+ output started. Send Stop to exit.\r\n");

    for (;;) {
        /* 闈為樆濉炴鏌ュ懡浠?*/
#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U)
        if (menu_read_line(line_buf, MENU_LINE_BUF_SIZE, &line_pos) ||
            menu_read_ble_line(line_buf, MENU_LINE_BUF_SIZE,
                               &ble_line_pos, &ble_line_discard)) {
            (void)printf("[BLE/UART MENU] %s\r\n", line_buf);
#else
        if (menu_read_line(line_buf, MENU_LINE_BUF_SIZE, &line_pos)) {
#endif
            vofa_cmd_t cmd;
            if (app_vofa_parse_cmd(line_buf, &cmd)) {
                app_vofa_apply_cmd(&cmd, ctx, motor, need_refresh);

                /* Stop閫€鍑烘暟鎹緭鍑烘ā寮?*/
                if (cmd.type == VOFA_CMD_STOP ||
                    cmd.type == VOFA_CMD_STOP_ALL ||
                    cmd.type == VOFA_CMD_STREAM_OFF) {
                    (void)printf("[DATA] VOFA+ output stopped.\r\n");
                    return;
                }
                /* 闈濻top鍛戒护: 鍙墦鍗板弽棣? 涓嶅埛鏂拌彍鍗?*/
            }
        }

        /* 杈撳嚭VOFA+鏁版嵁(11閫氶亾, DMA闈為樆濉? */
        {
            float channels[VOFA_TELEMETRY_CHANNEL_COUNT];
            OSAL_CRITICAL_SECTION {
                for (uint32_t i = 0U; i < BSP_MOTOR_COUNT; i++) {
                    channels[i] = (float)ctx->status.rpm[i];
                    channels[4 + i] = ctx->pid[i].setpoint;
                }
                /* CH8: FF duty(閫変腑鐢垫満) */
                if (ctx->ff[*motor].enabled) {
                    channels[8] = app_ff_compute(
                        &ctx->ff[*motor], ctx->pid[*motor].setpoint);
                } else {
                    channels[8] = 0.0f;
                }
                /* CH9: PID淇閲?閫変腑鐢垫満) */
                channels[9] = ctx->status.pid_correction[*motor];
                /* CH10: 瀹為檯DUTY杈撳嚭(閫変腑鐢垫満) */
                channels[10] = (float)ctx->status.output[*motor];
            }

            /* 鏍煎紡鍖栧埌涓存椂缂撳啿鍖?*/
            char tx_buf[180];
            int len = 0;
            for (uint32_t i = 0U; i < VOFA_TELEMETRY_CHANNEL_COUNT; i++) {
                if (i > 0U) {
                    tx_buf[len] = ',';
                    len++;
                }
                int ret = snprintf(&tx_buf[len],
                    sizeof(tx_buf) - (uint32_t)len,
                    "%.6f", (double)channels[i]);
                if (ret < 0 || (uint32_t)ret >= sizeof(tx_buf) - (uint32_t)len) {
                    len = 0;  /* 缂撳啿鍖轰笉瓒?鏀惧純鏈抚 */
                    break;
                }
                len += ret;
            }
            if (len > 0 && len < (int)sizeof(tx_buf)) {
                tx_buf[len] = '\n';
                len++;
                /* 闈為樆濉濪MA鍙戦€侊細妫€鏌ユ爣蹇椾綅锛屽繖鍒欒烦杩囨湰甯?*/
                if (bsp_uart_tx_idle()) {
                    (void)bsp_uart_send_dma((uint8_t *)tx_buf, (uint16_t)len);
                }
            }
        }

        /* LED蹇冭烦 */
        led_cnt += APP_RPM_OUTPUT_PERIOD_MS;
        if (led_cnt >= LED_TOGGLE_THRESH) {
            led_cnt = 0U;
            bsp_led_toggle();
        }

        osal_task_delay_ms(APP_RPM_OUTPUT_PERIOD_MS);
    }
}

/* ======================== 鍏叡鍑芥暟瀹炵幇 ======================== */

#if (PRJ_BLE_MENU_ENABLE != 0U)

/* ======================== JDY-23 BLE console helpers ======================== */

#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U)
/**
 * @brief 从 JDY-23 透明通道组装一条菜单命令。
 * @details 手机端必须发送 ASCII 文本并以 CR/LF 或 LF 结束。该函数不回显
 * 到 BLE，也不把 BLE 数据直接交给电机层；完整行仍复用现有菜单分发逻辑。
 * 超长行会被丢弃到行尾，避免执行截断命令。
 */
static bool menu_read_ble_line(char *line_buf, uint32_t buf_size,
                               uint32_t *line_pos, bool *discard_line)
{
    uint8_t ch;
    uint16_t received;
    jdy23_status_t status;

    if ((line_buf == NULL) || (buf_size <= 1U) ||
        (line_pos == NULL) || (discard_line == NULL)) {
        return false;
    }

    /* 每次最多取 64 字节；遇到完整行立即返回，剩余数据留给下次循环。 */
    for (uint32_t i = 0U; i < 64U; i++) {
        received = 0U;
        status = app_ble_receive(&ch, 1U, &received);
        if ((status != JDY23_OK) || (received == 0U)) {
            return false;
        }

        if ((ch == '\r') || (ch == '\n')) {
            if (*discard_line) {
                *line_pos = 0U;
                *discard_line = false;
                continue;
            }
            if (*line_pos == 0U) {
                continue;
            }
            line_buf[*line_pos] = '\0';
            *line_pos = 0U;
            return true;
        }

        if ((ch == 0x7FU) || (ch == 0x08U)) {
            if (!*discard_line && (*line_pos > 0U)) {
                (*line_pos)--;
            }
            continue;
        }

        if ((ch >= 0x20U) && (ch < 0x7FU)) {
            if (*discard_line) {
                continue;
            }
            if (*line_pos < (buf_size - 1U)) {
                line_buf[*line_pos] = (char)ch;
                (*line_pos)++;
            } else {
                *line_pos = 0U;
                *discard_line = true;
            }
        }
    }

    return false;
}
#endif /* PRJ_BLE_MENU_CONSOLE_ENABLE */


/**
 * @brief 将 JDY-23 状态码转换为串口可读名称。
 * @param status JDY-23 驱动状态码。
 * @return 静态字符串，不需要调用者释放。
 */
static const char *menu_ble_status_name(jdy23_status_t status)
{
    switch (status) {
    case JDY23_OK:                    return "OK";
    case JDY23_ERR_INVALID_PARAM:     return "INVALID_PARAM";
    case JDY23_ERR_NOT_INIT:          return "NOT_INIT";
    case JDY23_ERR_IO:                return "IO_ERROR";
    case JDY23_ERR_TIMEOUT:           return "TIMEOUT";
    case JDY23_ERR_RESPONSE_TOO_LONG: return "RESPONSE_TOO_LONG";
    case JDY23_ERR_UNEXPECTED_RESPONSE: return "UNEXPECTED_RESPONSE";
    default:                          return "UNKNOWN";
    }
}

/**
 * @brief 打印 UART0 控制台支持的 JDY-23 BLE 命令帮助。
 * @details 命令只用于调试和模块配置；BLE 接收数据不会进入电机控制解析器。
 */
static void menu_print_ble_usage(void)
{
    (void)printf("\r\nJDY-23 BLE commands (UART1: PB6 TX, PB7 RX, 9600 8N1):\r\n");
    (void)printf("  ble status                    Show transport/module status.\r\n");
    (void)printf("  ble probe                     Probe with AT+VER\\r\\n, then AT fallback.\r\n");
    (void)printf("  ble query <key>               Run one wrapped read-only AT query.\r\n");
    (void)printf("  ble inspect                   Query every known read-only item.\r\n");
    (void)printf("  ble action <key> CONFIRM      Run RST/DISC/SLEEP explicitly.\r\n");
    (void)printf("  ble at <command>              Send arbitrary AT plus CRLF.\r\n");
    (void)printf("  ble atraw <command>           Compatibility mode: no line ending.\r\n");
    (void)printf("  ble send <text>               Send transparent data to BLE peer.\r\n");
    (void)printf("  ble rx                        Drain received BLE bytes once.\r\n");
    (void)printf("  ble monitor on|off            Print received BLE bytes each menu cycle.\r\n");
    (void)printf("  ble flush                     Discard buffered BLE RX bytes.\r\n");
    (void)printf("Queries: VER STAT MAC BAUD NAME STARTEN ADVIN HOSTEN IBUUID MAJOR MINOR.\r\n");
    (void)printf("Actions: RST DISC SLEEP (confirmation is mandatory).\r\n");
    (void)printf("Safety: BLE RX is NOT forwarded to the motor/VOFA command parser.\r\n");
    (void)printf("Disconnect the BLE peer before AT commands; all wrapped commands add CRLF.\r\n");
}

/**
 * @brief 以可见转义格式打印 BLE 原始字节。
 * @param data 待打印数据。
 * @param len 数据长度，单位为字节。
 */
static void menu_print_ble_bytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0U; i < len; i++) {
        uint8_t ch = data[i];
        if (ch == (uint8_t)'\r') {
            (void)printf("\\r");
        } else if (ch == (uint8_t)'\n') {
            (void)printf("\\n");
        } else if ((ch >= 0x20U) && (ch <= 0x7EU)) {
            (void)printf("%c", (int)ch);
        } else {
            (void)printf("\\x%02X", (unsigned int)ch);
        }
    }
}

/**
 * @brief 从应用服务读取并打印一批已缓存 BLE 接收数据。
 * @return 实际读取到数据返回 true；无数据或读取失败返回 false。
 */
static bool menu_ble_drain_rx(void)
{
    uint8_t data[64];
    uint16_t received = 0U;
    jdy23_status_t status = app_ble_receive(data, (uint16_t)sizeof(data),
                                            &received);

    if (status != JDY23_OK) {
        (void)printf("[BLE RX] error=%s (%d)\r\n",
                     menu_ble_status_name(status), (int)status);
        return false;
    }
    if (received == 0U) {
        return false;
    }

    (void)printf("[BLE RX %u] ", (unsigned int)received);
    menu_print_ble_bytes(data, received);
    (void)printf("\r\n");
    return true;
}

/**
 * @brief 获取 BLE 控制台命令前缀后的参数部分。
 * @param line 完整命令行。
 * @param prefix_len 前缀长度，单位为字符。
 * @return 跳过空格后的参数指针，指向原始命令行内部。
 */
static const char *menu_ble_argument(const char *line, uint32_t prefix_len)
{
    const char *arg = &line[prefix_len];
    while (*arg == ' ') {
        arg++;
    }
    return arg;
}

/**
 * @brief 判断命令行是否匹配一个完整 BLE 命令前缀。
 * @param line 待匹配命令行。
 * @param prefix 命令前缀。
 * @return 完全匹配或后接空格时返回 true。
 */
static bool menu_ble_prefix_matches(const char *line, const char *prefix)
{
    size_t prefix_len = strlen(prefix);

    return (strncmp(line, prefix, prefix_len) == 0) &&
           ((line[prefix_len] == '\0') || (line[prefix_len] == ' '));
}

/**
 * @brief 将应用层响应格式转换为诊断输出名称。
 * @param info 命令元数据，可为 NULL。
 * @param result 应用层命令结果，可为 NULL。
 * @return 静态格式名称字符串。
 */
static const char *menu_ble_response_format_name(
    const jdy23_command_info_t *info, const app_ble_command_result_t *result)
{
    if (result == NULL) {
        return "NONE";
    }
    if (result->format == APP_BLE_RESPONSE_FORMAT_RAW_FALLBACK) {
        return "RAW_FALLBACK";
    }
    if (result->format == APP_BLE_RESPONSE_FORMAT_PREFIX_MATCHED) {
        return ((info != NULL) && info->response_prefix_verified) ?
               "PREFIX_MATCHED/VERIFIED" :
               "PREFIX_MATCHED/UNVERIFIED";
    }
    return "NONE";
}

/**
 * @brief 打印一条 JDY-23 内置命令的完整诊断结果。
 * @param operation 当前操作名称，例如 query、inspect 或 action。
 * @param command 已执行的命令索引。
 * @param result 传输和解析结果。
 */
static void menu_print_ble_command_result(
    const char *operation, jdy23_command_t command,
    const app_ble_command_result_t *result)
{
    const jdy23_command_info_t *info = app_ble_get_command_info(command);

    if ((operation == NULL) || (info == NULL) || (result == NULL)) {
        (void)printf("BLE command result: invalid internal parameter.\r\n");
        return;
    }

    (void)printf("\r\nBLE %s %s [%s]: %s (%d)\r\n",
                 operation, info->name, info->at_command,
                 menu_ble_status_name(result->transfer_status),
                 (int)result->transfer_status);
    (void)printf("  raw    : ");
    if (result->raw_response[0] != '\0') {
        menu_print_ble_bytes((const uint8_t *)result->raw_response,
                             (uint16_t)strlen(result->raw_response));
    } else {
        (void)printf("<none>");
    }
    (void)printf("\r\n");

    (void)printf("  value  : ");
    if (result->parse_status == JDY23_OK) {
        menu_print_ble_bytes((const uint8_t *)result->value,
                             (uint16_t)strlen(result->value));
    } else {
        (void)printf("<not parsed: %s (%d)>",
                     menu_ble_status_name(result->parse_status),
                     (int)result->parse_status);
    }
    (void)printf("\r\n");
    (void)printf("  format : %s\r\n",
                 menu_ble_response_format_name(info, result));
    if (info->response_prefix != NULL) {
        (void)printf("  prefix : %s (%s)\r\n", info->response_prefix,
                     info->response_prefix_verified ?
                     "hardware verified" : "assumed; collect raw reply");
    } else {
        (void)printf("  prefix : <unknown/action response>\r\n");
    }
}

/**
 * @brief 执行内置命令并立即打印结果。
 * @param operation 输出中的操作名称。
 * @param command 命令索引。
 * @param timeout_ms 命令超时时间，单位为毫秒。
 * @return 应用服务返回的传输状态。
 */
static jdy23_status_t menu_ble_execute_and_print(
    const char *operation, jdy23_command_t command, uint32_t timeout_ms)
{
    app_ble_command_result_t result;
    jdy23_status_t status = app_ble_execute_command(command, &result,
                                                    timeout_ms);
    menu_print_ble_command_result(operation, command, &result);
    return status;
}

/**
 * @brief 依次执行所有内置只读 AT 查询并打印结果。
 * @details 不执行 RST、DISC、SLEEP 等动作命令，适合工厂和现场只读诊断。
 */
static void menu_ble_inspect_all(void)
{
    uint32_t i;

    (void)printf("\r\n===== JDY-23 READ-ONLY AT INSPECTION =====\r\n");
    (void)printf("All commands append \\r\\n. Raw replies are retained.\r\n");
    (void)printf("Expected prefixes not yet hardware-verified are marked UNVERIFIED.\r\n");

    for (i = 0U; i < (uint32_t)JDY23_COMMAND_COUNT; i++) {
        jdy23_command_t command = (jdy23_command_t)i;
        const jdy23_command_info_t *info =
            app_ble_get_command_info(command);
        if ((info != NULL) && (info->kind == JDY23_COMMAND_KIND_QUERY)) {
            (void)menu_ble_execute_and_print("inspect", command, 800U);
        }
    }
    (void)printf("===== JDY-23 INSPECTION COMPLETE =====\r\n");
}

/**
 * @brief 解析并处理一条 UART0 控制台 BLE 命令。
 * @param line 已去除行尾的命令字符串。
 * @param[in,out] monitor_enabled BLE 接收监视开关。
 * @return 该命令属于 BLE 命令并已处理返回 true，否则返回 false。
 */
static bool menu_handle_ble_command(const char *line,
                                    bool *monitor_enabled)
{
    char response[APP_BLE_RESPONSE_MAX];
    char key[16];
    char token[16];
    char extra[2];
    jdy23_status_t status;
    jdy23_command_t command;
    const jdy23_command_info_t *info;
    const char *arg;
    int parsed;

    if ((line == NULL) || (monitor_enabled == NULL)) {
        return false;
    }
    if ((strcmp(line, "ble") == 0) || (strcmp(line, "ble help") == 0)) {
        menu_print_ble_usage();
        return true;
    }
    if (strcmp(line, "ble status") == 0) {
        app_ble_status_t ble;
        app_ble_get_status(&ble);
        (void)printf("\r\nBLE/JDY-23 status:\r\n");
        (void)printf("  UART1 mapping : TX=PB6, RX=PB7, %lu 8N1\r\n",
                     (unsigned long)ble.baud_rate);
        (void)printf("  initialized   : %s\r\n",
                     ble.initialized ? "YES" : "NO");
        (void)printf("  AT detected   : %s\r\n",
                     ble.detected ? "YES" : "NO/not probed");
        (void)printf("  monitor       : %s\r\n",
                     *monitor_enabled ? "ON" : "OFF");
        (void)printf("  RX pending    : %lu\r\n",
                     (unsigned long)ble.rx_pending);
        (void)printf("  RX bytes/overflow: %lu/%lu\r\n",
                     (unsigned long)ble.uart_diag.rx_bytes,
                     (unsigned long)ble.uart_diag.rx_overflow);
        (void)printf("  IRQ/ignored   : %lu/%lu\r\n",
                     (unsigned long)ble.uart_diag.irq_count,
                     (unsigned long)ble.uart_diag.ignored_irq_count);
        return true;
    }
    if (strcmp(line, "ble probe") == 0) {
        status = app_ble_probe(response, (uint16_t)sizeof(response), 500U);
        (void)printf("BLE probe: %s (%d)", menu_ble_status_name(status),
                     (int)status);
        if (response[0] != '\0') {
            (void)printf("; response=");
            menu_print_ble_bytes((const uint8_t *)response,
                                 (uint16_t)strlen(response));
        }
        (void)printf("\r\n");
        return true;
    }
    if (menu_ble_prefix_matches(line, "ble query")) {
        arg = menu_ble_argument(line, 9U);
        parsed = sscanf(arg, "%15s %1s", key, extra);
        if ((parsed != 1) || !app_ble_find_command(key, &command)) {
            (void)printf("Usage: ble query <VER|STAT|MAC|BAUD|NAME|STARTEN|ADVIN|HOSTEN|IBUUID|MAJOR|MINOR>\r\n");
            return true;
        }
        info = app_ble_get_command_info(command);
        if ((info == NULL) || (info->kind != JDY23_COMMAND_KIND_QUERY)) {
            (void)printf("BLE %s is an action; use: ble action %s CONFIRM\r\n",
                         key, key);
            return true;
        }
        (void)menu_ble_execute_and_print("query", command, 800U);
        return true;
    }
    if (strcmp(line, "ble inspect") == 0) {
        menu_ble_inspect_all();
        return true;
    }
    if (menu_ble_prefix_matches(line, "ble action")) {
        arg = menu_ble_argument(line, 10U);
        parsed = sscanf(arg, "%15s %15s %1s", key, token, extra);
        if ((parsed != 2) || (strcmp(token, "CONFIRM") != 0) ||
            !app_ble_find_command(key, &command)) {
            (void)printf("Usage: ble action <RST|DISC|SLEEP> CONFIRM\r\n");
            return true;
        }
        info = app_ble_get_command_info(command);
        if ((info == NULL) || (info->kind != JDY23_COMMAND_KIND_ACTION)) {
            (void)printf("BLE %s is read-only; use: ble query %s\r\n",
                         key, key);
            return true;
        }
        status = menu_ble_execute_and_print("action", command, 1000U);
        if ((status == JDY23_ERR_TIMEOUT) &&
            ((command == JDY23_COMMAND_RST) ||
             (command == JDY23_COMMAND_DISC) ||
             (command == JDY23_COMMAND_SLEEP))) {
            (void)printf("Note: timeout may be expected if the module reset, disconnected, or slept before replying.\r\n");
        }
        return true;
    }
    if (menu_ble_prefix_matches(line, "ble atraw")) {
        arg = menu_ble_argument(line, 9U);
        if (*arg == '\0') {
            menu_print_ble_usage();
            return true;
        }
        status = app_ble_send_at(arg, JDY23_LINE_END_NONE, response,
                                 (uint16_t)sizeof(response), 800U);
        (void)printf("BLE AT(raw): %s (%d)", menu_ble_status_name(status),
                     (int)status);
        if (response[0] != '\0') {
            (void)printf("; response=");
            menu_print_ble_bytes((const uint8_t *)response,
                                 (uint16_t)strlen(response));
        }
        (void)printf("\r\n");
        return true;
    }
    if (menu_ble_prefix_matches(line, "ble at")) {
        arg = menu_ble_argument(line, 6U);
        if (*arg == '\0') {
            menu_print_ble_usage();
            return true;
        }
        status = app_ble_send_at(arg, JDY23_LINE_END_CRLF, response,
                                 (uint16_t)sizeof(response), 800U);
        (void)printf("BLE AT: %s (%d)", menu_ble_status_name(status),
                     (int)status);
        if (response[0] != '\0') {
            (void)printf("; response=");
            menu_print_ble_bytes((const uint8_t *)response,
                                 (uint16_t)strlen(response));
        }
        (void)printf("\r\n");
        return true;
    }
    if (menu_ble_prefix_matches(line, "ble send")) {
        arg = menu_ble_argument(line, 8U);
        if (*arg == '\0') {
            menu_print_ble_usage();
            return true;
        }
        status = app_ble_send((const uint8_t *)arg, (uint16_t)strlen(arg));
        (void)printf("BLE TX: %s (%d), %u byte(s)\r\n",
                     menu_ble_status_name(status), (int)status,
                     (unsigned int)strlen(arg));
        return true;
    }
    if (strcmp(line, "ble rx") == 0) {
#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U)
        (void)printf("BLE RX command is unavailable while menu console is enabled.\r\n");
#else
        if (!menu_ble_drain_rx()) {
            (void)printf("[BLE RX] no buffered data.\r\n");
        }
#endif
        return true;
    }
    if (strcmp(line, "ble monitor on") == 0) {
#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U)
        (void)printf("BLE RX monitor is unavailable while menu console is enabled.\r\n");
#else
        *monitor_enabled = true;
        (void)printf("BLE RX monitor: ON\r\n");
#endif
        return true;
    }
    if (strcmp(line, "ble monitor off") == 0) {
        *monitor_enabled = false;
        (void)printf("BLE RX monitor: OFF\r\n");
        return true;
    }
    if (strcmp(line, "ble flush") == 0) {
#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U)
        (void)printf("BLE RX flush is unavailable while menu console is enabled.\r\n");
#else
        app_ble_flush_rx();
        (void)printf("BLE RX buffer flushed.\r\n");
#endif
        return true;
    }
    if (menu_ble_prefix_matches(line, "ble")) {
        menu_print_ble_usage();
        return true;
    }
    return false;
}

#endif /* PRJ_BLE_MENU_ENABLE */

void app_menu_task(void *param)
{
    app_shared_ctx_t *ctx = (app_shared_ctx_t *)param;
    uint32_t selected_motor = 0U;
    char line_buf[MENU_LINE_BUF_SIZE];
    uint32_t line_pos = 0U;
    uint32_t led_cnt = 0U;
    bool need_refresh = true;
#if (PRJ_BLE_MENU_ENABLE != 0U)
    bool ble_monitor_enabled = false;
#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U)
    uint32_t ble_line_pos = 0U;
    bool ble_line_discard = false;
#endif
#endif

    (void)printf("\r\n=== MSPM0G3507 Motor Control ===\r\n");
    (void)printf("Send VOFA+ commands to control.\r\n");
    (void)printf("Type 'bench' to run MATHACL benchmark.\r\n");
#if (PRJ_BLE_MENU_ENABLE != 0U)
    (void)printf("Type 'ble help' for JDY-23 UART1/BLE commands.\r\n");
#endif
    (void)printf("Type 'encdiag' for one read-only hardware/encoder snapshot.\r\n");
#if (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0U)
    (void)printf("Type 'encdiag cap [A|B|C|D|ALL] [ms]' for capture-edge diagnostics.\r\n");
#endif
    (void)printf("Type 'drvscope start' for the persistent DRV8870 oscilloscope session.\r\n");
    (void)printf("Type 'drvscope status' for scope command help/status.\r\n");
    (void)printf("Type 'mathdiag' to run MATHACL hardware diagnostic.\r\n");
    (void)printf("Type 'zutptest N' to run N-sec ZUPT drift test.\r\n");
    (void)printf("  - Outputs: yaw drift/std/range, bias_z tracking, fault detect, verdict\r\n");
    (void)printf("Type 'turndtest ANG [T]' to start turn test (e.g. turndtest 90).\r\n");
    (void)printf("  - Outputs: angle error, turn/stable phase stats, bias_z gating, verdict\r\n");
    (void)printf("Type 'turnend' to finish turn (after reaching target angle).\r\n");
    (void)printf("Type 'kftune N' to run KF parameter sweep (N sec/set, 9 sets).\r\n");
    (void)printf("  - Sweeps Q_angle x Q_bias, outputs drift/std/jump, finds best params\r\n");

    for (;;) {
        /* 鍒锋柊鑿滃崟 */
        if (need_refresh) {
            menu_print_status(ctx, selected_motor);
            need_refresh = false;
        }

        /* 妫€鏌ュ懡浠?*/
#if (PRJ_BLE_MENU_CONSOLE_ENABLE != 0U)
        if (menu_read_line(line_buf, MENU_LINE_BUF_SIZE, &line_pos) ||
            menu_read_ble_line(line_buf, MENU_LINE_BUF_SIZE,
                               &ble_line_pos, &ble_line_discard)) {
            (void)printf("[BLE/UART MENU] %s\r\n", line_buf);
#else
        if (menu_read_line(line_buf, MENU_LINE_BUF_SIZE, &line_pos)) {
#endif
            menu_drvscope_cmd_t scope_cmd;

#if (PRJ_BLE_MENU_ENABLE != 0U)
            if (menu_handle_ble_command(line_buf, &ble_monitor_enabled)) {
                need_refresh = true;
            } else
#endif
            if (menu_parse_drvscope(line_buf, &scope_cmd)) {
                switch (scope_cmd.action) {
                case MENU_DRVSCOPE_START:
                    app_debug_drv8870_scope_start(ctx);
                    break;
                case MENU_DRVSCOPE_SET_ONE:
                    app_debug_drv8870_scope_set(scope_cmd.motor_id,
                                                  scope_cmd.duty_permille);
                    break;
                case MENU_DRVSCOPE_SET_ALL:
                    app_debug_drv8870_scope_set_all(scope_cmd.duty_permille);
                    break;
                case MENU_DRVSCOPE_STATUS:
                    app_debug_drv8870_scope_status();
                    menu_print_drvscope_usage();
                    break;
                case MENU_DRVSCOPE_OFF:
                    app_debug_drv8870_scope_stop();
                    break;
                default:
                    menu_print_drvscope_usage();
                    break;
                }
                need_refresh = true;
            } else if (strncmp(line_buf, "drvscope", 8U) == 0) {
                menu_print_drvscope_usage();
                need_refresh = true;
            } else if (strcmp(line_buf, "encdiag") == 0) {
                app_debug_hwmap_snapshot();
                need_refresh = true;
            } else if (strncmp(line_buf, "encdiag cap", 11U) == 0) {
                uint32_t capture_motor = BSP_ENCODER_COUNT;
                uint32_t capture_window = 1000U;
                if (menu_parse_encdiag_capture(line_buf,
                                               &capture_motor,
                                               &capture_window)) {
                    app_debug_encoder_capture_diag(capture_motor,
                                                   capture_window);
                } else {
                    menu_print_encdiag_capture_usage();
                }
                need_refresh = true;
            } else if (strcmp(line_buf, "enc") == 0) {
                app_debug_encoder_stream(50);
                need_refresh = true;
            } else if (strcmp(line_buf, "diag") == 0) {
                app_debug_encoder_diag(ctx, selected_motor);
                need_refresh = true;
            } else if (strcmp(line_buf, "adc") == 0) {
                app_debug_adc_test();
                need_refresh = true;
            } else if (strncmp(line_buf, "zutptest", 8) == 0) {
                /* zutptest N: 鍚姩 N 绉?ZUPT 娴嬭瘯 */
                uint32_t dur = 60;
                if (strlen(line_buf) > 9) {
                    dur = (uint32_t)atoi(&line_buf[9]);
                    if (dur == 0 || dur > 86400) dur = 60;
                }
                app_test_runner_start(dur);
            } else if (strncmp(line_buf, "turndtest", 9) == 0) {
                /* turndtest ANGLE [TIMEOUT]: 鍔ㄦ€佽浆鍔ㄧ簿搴︽祴璇?*/
                float target = 90.0f;
                uint32_t timeout = 60;
                /* 瑙ｆ瀽: "turndtest 90" 鎴?"turndtest 90 30" */
                char *p = &line_buf[9];
                while (*p == ' ') p++;
                if (*p != '\0') {
                    target = (float)atof(p);
                    /* 鏌ユ壘绗簩涓弬鏁?*/
                    while (*p != '\0' && *p != ' ') p++;
                    while (*p == ' ') p++;
                    if (*p != '\0') {
                        int t = atoi(p);
                        if (t > 0 && t <= 86400) timeout = (uint32_t)t;
                    }
                }
                if (target == 0.0f) target = 90.0f;
                app_test_runner_start_turn(target, timeout);
            } else if (strcmp(line_buf, "turnend") == 0) {
                /* turnend: 鎵嬪姩纭杞姩缁撴潫 */
                app_test_runner_end_turn();
            } else if (strncmp(line_buf, "kftune", 6) == 0) {
                /* kftune N: KF 鍙傛暟鎵弿, 姣忕粍 N 绉?(榛樿 10) */
                uint32_t dur = 10;
                if (strlen(line_buf) > 7) {
                    dur = (uint32_t)atoi(&line_buf[7]);
                    if (dur == 0 || dur > 600) dur = 10;
                }
                app_test_runner_start_kftune(dur);
            } else {
                vofa_cmd_t cmd;
                if (app_vofa_parse_cmd(line_buf, &cmd)) {
                    app_vofa_apply_cmd(&cmd, ctx, &selected_motor,
                                       &need_refresh);

                    /* Run鍛戒护杩涘叆鏁版嵁杈撳嚭妯″紡 */
                    if (cmd.type == VOFA_CMD_RUN ||
                        cmd.type == VOFA_CMD_STREAM_ON) {
                        menu_data_output_loop(ctx, &selected_motor,
                                              &need_refresh);
                        /* 閫€鍑哄悗鍒锋柊鑿滃崟 */
                        need_refresh = true;
                    }
                }
            }
        }

#if (PRJ_BLE_MENU_ENABLE != 0U) && (PRJ_BLE_MENU_CONSOLE_ENABLE == 0U)
        if (ble_monitor_enabled) {
            (void)menu_ble_drain_rx();
        }
#endif

        /* LED蹇冭烦 */
        led_cnt += MENU_LED_PERIOD_MS;
        if (led_cnt >= LED_TOGGLE_THRESH) {
            led_cnt = 0U;
            bsp_led_toggle();
        }

        osal_task_delay_ms(MENU_LED_PERIOD_MS);
    }
}
