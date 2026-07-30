# 外设 API 快速参考 v0.1.0

工程根目录：D:/msp_project/temp2/MSPM0G3507_Project_temp/MSPM0G3507_Project/MSPM0G3507_FreeRTOS

本文只列当前工程中已经存在、适合应用层直接调用的接口。硬件引脚和外设实例以 Config/empty.syscfg 为准，周期、比例、阈值和功能开关以 Config/project_config.h 为准。

## 1. 统一约定

### 1.1 初始化顺序

1. 调用 SYSCFG_DL_init()（由 main/board 启动流程完成）。
2. 初始化 UART、LED、ADC、电机、编码器和 IMU。
3. 创建任务；在控制任务周期内调用运动控制和编码器读取接口。
4. 任何电机动作前调用 bsp_motor_power_enable()；停止时调用 bsp_motor_stop_all() 和 bsp_motor_power_disable()。

### 1.2 返回值

BSP 层返回 bsp_status_t：

| 返回值 | 含义 |
|---|---|
| BSP_OK | 成功 |
| BSP_ERR_NULL_PTR | 指针为空 |
| BSP_ERR_INVALID_PARAM | 参数越界或不合法 |
| BSP_ERR_BUSY | 外设忙，例如 DMA 正在发送 |
| BSP_ERR_TIMEOUT | 超时 |
| BSP_ERR_NOT_INIT | 尚未初始化 |
| BSP_ERR_HW_FAULT | 硬件故障 |
| BSP_ERR_UNSUPPORTED | 当前后端不支持 |
| BSP_ERR_NOT_READY | 已初始化，但前置条件未满足 |

HAL 层返回 hal_status_t：HAL_OK、HAL_ERR_INVALID_PARAM、HAL_ERR_BUSY、HAL_ERR_TIMEOUT、HAL_ERR_HW_FAULT、HAL_ERR_NOT_INIT、HAL_ERR_UNSUPPORTED。

所有时间参数均为无符号毫秒或微秒；带单位的浮点参数必须使用工程规定的 SI 单位。

## 2. GPIO 与 LED

### 2.1 硬件配置

配置文件：

- D:/msp_project/temp2/MSPM0G3507_Project_temp/MSPM0G3507_Project/MSPM0G3507_FreeRTOS/Config/empty.syscfg
- 生成文件：Config/ti_msp_dl_config.h、Config/ti_msp_dl_config.c
- API 头文件：BSP/Peripherals/hal_gpio.h、BSP/Peripherals/bsp_led.h

当前已配置：

| 功能 | 引脚 | 方向/电平 |
|---|---|---|
| 电机电源开关 POWER | PB19 | 输出，active-high |
| KEY_key | PA7 | 输入，上拉，低电平有效 |
| KEY_switch | PB3 | 输入，上拉，低电平有效 |
| LED | PA27 | 输出 |

SysConfig 已配置的引脚不需要再次调用 hal_gpio_init_input() 或 hal_gpio_init_output()；直接读写即可。未在 SysConfig 中配置的引脚才调用初始化函数。

### 2.2 核心 API

HAL GPIO：

| API | 参数 | 合法值/返回值 |
|---|---|---|
| hal_gpio_set_pin(port, pin) | port：HAL_GPIO_PORT_A/B；pin：0~31 的引脚号 | 输出高电平；成功 HAL_OK |
| hal_gpio_clear_pin(port, pin) | 同上 | 输出低电平；成功 HAL_OK |
| hal_gpio_write_pin(port, pin, high) | high：true/false | 写高/低；成功 HAL_OK |
| hal_gpio_read_pin(port, pin) | 同上 | 返回当前输入电平 true/false |
| hal_gpio_read_output_latch(port, pin) | 同上 | 返回输出锁存器，不代表外部实际电压 |
| hal_gpio_toggle_pin(port, pin) | 同上 | 翻转输出；成功 HAL_OK |
| hal_gpio_init_input(cfg, pull) | cfg：引脚配置；pull：NONE/PULL_UP/PULL_DOWN | 仅用于未由 SysConfig 配置的输入 |
| hal_gpio_init_output(cfg) | cfg：引脚配置 | 仅用于未由 SysConfig 配置的输出 |

LED BSP：

| API | 说明 |
|---|---|
| bsp_led_init() | 初始化 LED；返回 BSP_OK 或错误码 |
| bsp_led_on()/bsp_led_off() | 点亮/熄灭 |
| bsp_led_toggle() | 翻转 |
| bsp_led_is_on() | 返回软件记录的 LED 状态 |

### 2.3 使用步骤

1. 在 empty.syscfg 配置端口、引脚、方向和上下拉。
2. 重新生成 SysConfig 文件并确认生成宏未改变。
3. 启动时调用 bsp_led_init()；已配置 GPIO 不重复初始化。
4. 在任务中调用读写 API；不要直接操作 DL_GPIO 寄存器。

### 2.4 示例

~~~c
/* 示例 1：读取 KEY_key，低电平表示按下 */
bool pressed = !hal_gpio_read_pin(HAL_GPIO_PORT_A, KEY_key_PIN);
if (pressed) {
    bsp_led_on();
}
~~~

~~~c
/* 示例 2：直接控制一个已配置的输出 */
hal_gpio_write_pin(HAL_GPIO_PORT_B, POWER_pb19_PIN, true);
bsp_delay_ms(PRJ_DRV8870_POWER_STARTUP_MS);
bool latch = hal_gpio_read_output_latch(HAL_GPIO_PORT_B, POWER_pb19_PIN);
~~~

## 3. UART0、UART1 与 DMA

### 3.1 硬件配置

配置文件：Config/empty.syscfg；API 头文件：BSP/Peripherals/bsp_uart.h、BSP/Peripherals/hal_uart.h。

| 实例 | 引脚 | 当前用途 | 波特率/格式 |
|---|---|---|---|
| UART0 / HAL_UART_DEBUG | PA10 TX、PA11 RX | 上位机、菜单、日志、IMU DMA 输出 | 115200，8N1，TX DMA |
| UART1 / HAL_UART_EXT | PB6 TX、PB7 RX | 外部扩展模块 | 115200，8N1，当前直接用 HAL |

bsp_uart_* 只封装 UART0。UART1 不要调用 bsp_uart_send_dma()，应使用 hal_uart_transmit_dma(HAL_UART_EXT, ...)。

### 3.2 UART0 BSP API

| API | 参数 | 合法值/返回值 |
|---|---|---|
| bsp_uart_init() | 无 | 初始化 UART0；BSP_OK 表示成功 |
| bsp_uart_putc(data) | data：0~255 | 阻塞发送 1 字节 |
| bsp_uart_puts(str) | str：以 0 结尾的字符串 | 阻塞发送；str 不能为 NULL |
| bsp_uart_getc(&data) | data：输出字节指针 | 有字节返回成功，无数据返回 BSP_ERR_BUF_EMPTY |
| bsp_uart_rx_count() | 无 | 返回当前接收缓存字节数 |
| bsp_uart_rx_flush() | 无 | 清空接收缓存 |
| bsp_uart_send_dma(data, len) | data：发送缓存；len：1~512 | 复制到内部 DMA 缓冲区；忙时 BSP_ERR_BUSY |
| bsp_uart_tx_idle() | 无 | true 表示 DMA/发送空闲 |
| bsp_uart_get_tx_diag(&diag) | diag：输出诊断结构体 | 返回发送次数、忙拒绝等统计 |

约束：BSP_UART_DMA_TX_MAX_LEN 为 512 字节；DMA 发送期间不要修改 data 也不要重复提交，收到 BSP_ERR_BUSY 时等待 tx_idle 或下一周期重试。

### 3.3 UART1 HAL API

| API | 参数 | 合法值/返回值 |
|---|---|---|
| hal_uart_enable_irq(id) | id：HAL_UART_DEBUG/EXT | 使能对应中断 |
| hal_uart_disable_irq(id) | 同上 | 禁止对应中断 |
| hal_uart_transmit(id, data) | data：单字节 | 阻塞发送 |
| hal_uart_transmit_buf(id, data, len) | len：按缓冲区实际长度 | 阻塞发送缓冲区 |
| hal_uart_receive(id, &data) | 输出字节指针 | 成功取 1 字节；无数据返回错误 |
| hal_uart_is_busy(id) | 同上 | 返回发送忙状态 |
| hal_uart_get_irq_flag(id) | 同上 | HAL_UART_IRQ_NONE/RX/TX 或组合值 |
| hal_uart_transmit_dma(id, data, len) | len：受底层 DMA 配置限制 | 启动 DMA 发送；忙返回 HAL_ERR_BUSY |
| hal_uart_abort_tx_dma(id) | 同上 | 中止 DMA 发送 |

### 3.4 使用步骤

1. 在 empty.syscfg 设置实例、引脚、波特率和 DMA。
2. 启动时调用 bsp_uart_init() 初始化 UART0。
3. UART0 普通短日志调用 bsp_uart_puts()；周期性或长数据调用 bsp_uart_send_dma()。
4. UART1 业务先调用 HAL 接口；如果需要环形缓冲、协议解析或模块驱动，再在上层封装。

### 3.5 示例

~~~c
/* 示例 1：UART0 DMA 发送一帧 */
static const uint8_t msg[] = "imu ready\r\n";
if (bsp_uart_tx_idle()) {
    bsp_status_t rc = bsp_uart_send_dma(msg, (uint16_t)(sizeof(msg) - 1U));
    if (rc == BSP_ERR_BUSY) {
        /* 下一个周期重试，不要阻塞控制任务 */
    }
}
~~~

~~~c
/* 示例 2：UART1 发送给外部模块 */
static const uint8_t cmd[] = "AT\r\n";
hal_status_t rc = hal_uart_transmit_buf(HAL_UART_EXT, cmd,
                                         (uint16_t)(sizeof(cmd) - 1U));
~~~

~~~c
/* 示例 3：UART0 每 200 ms 发送 10 个 IMU 字段 */
char line[192];
int n = bsp_lsm6dsr_vofa_format(line, sizeof(line), &imu_data);
if (n > 0 && bsp_uart_tx_idle()) {
    (void)bsp_uart_send_dma((const uint8_t *)line, (uint16_t)n);
}
~~~

## 4. 定时器与 PWM

### 4.1 硬件配置

配置文件：Config/empty.syscfg；API 头文件：BSP/Peripherals/hal_timer.h、BSP/Peripherals/bsp_timer.h。

| 功能 | 定时器/通道 | 引脚 |
|---|---|---|
| 电机 PWM CC0~CC3 | TIMA0 | PA8、PA9、PB17、PB2 |
| 编码器捕获 A/M1 | TIMG0 | A 相捕获，B 相 GPIO 判向 |
| 编码器捕获 B/M2 | TIMG6 | 同上 |
| 编码器捕获 C/M3 | TIMG7 | 同上 |
| 编码器捕获 D/M4 | TIMA1 | 同上 |
| 系统节拍 | TIMG8 | 内部 |

PWM 周期计数范围为 0~1000。电机应用层优先使用 bsp_motor_set_speed()，只有示波器工厂测试才直接设置 compare。

### 4.2 核心 API

| API | 参数 | 合法值/说明 |
|---|---|---|
| bsp_timer_init() | 无 | 初始化 BSP 定时基准 |
| bsp_get_us()/bsp_get_ms() | 无 | 返回单调递增微秒/毫秒时间戳 |
| bsp_delay_us(us)/bsp_delay_ms(ms) | us/ms：>=0 | 阻塞延时，不用于控制闭环 |
| hal_timer_start(id) | id：HAL_TIMER_PWM_MOTOR、CAPTURE_*、SYS_TICK | 启动定时器 |
| hal_timer_stop(id) | 同上 | 停止定时器 |
| hal_timer_set_pwm_duty(id, channel, value) | value：0~1000 | 写 PWM 比较值 |
| hal_timer_get_count(id) | 同上 | 读当前计数 |
| hal_timer_get_capture_value(id, channel) | channel：有效 CC 通道 | 读捕获值 |
| hal_timer_get_irq_flag(id) | 同上 | CC0/CC1/LOAD 标志 |
| hal_timer_enable_irq/disable_irq(id) | 同上 | 使能/禁止中断 |
| hal_timer_reset_count(id) | 同上 | 清零计数 |

### 4.3 示例

~~~c
/* 示例 1：读取编码器捕获定时器的当前计数 */
uint32_t now = hal_timer_get_count(HAL_TIMER_CAPTURE_RB);
uint32_t edge = hal_timer_get_capture_value(HAL_TIMER_CAPTURE_RB, 0U);
~~~

~~~c
/* 示例 2：仅工厂示波器测试时设置单路 compare */
if (bsp_drv8870_hw_scope_start() == BSP_OK) {
    (void)bsp_drv8870_hw_scope_set_compare(BSP_DRV8870_A, 550U);
}
~~~

## 5. 电机与 DRV8870/TB6612

### 5.1 硬件配置

配置文件：Config/empty.syscfg、Config/project_config.h；API 头文件：BSP/Peripherals/bsp_motor.h、BSP/Peripherals/bsp_drv8870.h。

当前统一映射：

| 应用编号 | 接口 | PWM | 编码器 |
|---|---|---|---|
| A | M1 | TIMA0 CC0 / PA8 | RB / TIMG0 |
| B | M2 | TIMA0 CC1 / PA9 | RF / TIMG6 |
| C | M3 | TIMA0 CC2 / PB17 | LF / TIMG7 |
| D | M4 | TIMA0 CC3 / PB2 | LB / TIMA1 |

电源门控：PB19，active-high。电机接口全部开放；车体反馈是否使用某一路由 project_config.h 决定。

### 5.2 核心 API

| API | 参数 | 合法值/说明 |
|---|---|---|
| bsp_motor_init() | 无 | 初始化后端并将四路置安全停止 |
| bsp_motor_power_enable() | 无 | 先保持停止，再打开 PB19；成功后等待 PRJ_DRV8870_POWER_STARTUP_MS |
| bsp_motor_power_disable() | 无 | 停止全部电机并关闭电源 |
| bsp_motor_power_is_enabled() | 无 | 返回软件/锁存器状态 |
| bsp_motor_set_speed(motor, command) | motor：A/B/C/D；command：-500~500 | 正值车体前进，负值后退，0 停止 |
| bsp_motor_stop(motor, mode) | mode：COAST/BRAKE | 停止单路；DRV8870 可能统一降级为中性主动阻尼 |
| bsp_motor_stop_all() | 无 | 四路停止 |
| bsp_motor_get_command_max() | 无 | 返回当前最大命令，通常 500 |
| bsp_motor_percent_to_command(percent) | percent：0~100 | 百分比映射为 -500~500 的命令值 |
| bsp_motor_get_driver() | 无 | 返回 DRV8870 或 TB6612 |
| bsp_motor_get_driver_name() | 无 | 返回后端名称 |
| bsp_motor_get_capabilities() | 无 | 返回 POWER_GATE、COAST、BRAKE 等能力位 |

DRV8870 锁相反相规则：

- 命令低于 40%：反转；
- 40%~55%：死区；
- 50%：中性；
- 高于 55%：正转；
- 正常代码不要直接写 200/800 compare，使用有符号 command API。

### 5.3 使用步骤

1. 调用 bsp_motor_init()。
2. 调用 bsp_motor_power_enable()。
3. 按 5 ms 控制周期设置 command 或由速度环输出。
4. 停止时先 bsp_motor_stop_all()，再按系统策略关闭电源。
5. 硬件测试结束调用 bsp_drv8870_hw_scope_stop()，恢复安全中性。

### 5.4 示例

~~~c
/* 示例 1：单路 M1 以正方向运行，并安全停止 */
(void)bsp_motor_init();
if (bsp_motor_power_enable() == BSP_OK) {
    (void)bsp_motor_set_speed(BSP_MOTOR_A, 180);
    bsp_delay_ms(500);
    (void)bsp_motor_stop(BSP_MOTOR_A, BSP_MOTOR_MODE_BRAKE);
}
~~~

~~~c
/* 示例 2：四路差速：左侧前进，右侧反向 */
const int32_t cmd[4] = { 220, -220, 220, -220 };
if (!bsp_motor_power_is_enabled()) {
    (void)bsp_motor_power_enable();
}
for (uint32_t i = 0U; i < 4U; ++i) {
    (void)bsp_motor_set_speed((bsp_motor_id_t)i, cmd[i]);
}
~~~

~~~c
/* 示例 3：只在工厂示波器模式操作 compare */
(void)bsp_drv8870_hw_scope_set_all_compare(500U);
/* 测量结束后恢复并退出 scope */
(void)bsp_drv8870_hw_scope_stop();
~~~

## 6. 编码器与 RPM

### 6.1 硬件和计数口径

配置文件：Config/empty.syscfg、Config/project_config.h；API 头文件：BSP/Peripherals/bsp_encoder.h。

当前参数：

- PPR：13；
- 减速比：28/1；
- 解码倍频：2；
- 输出轴每转计数：13 × 28 × 2 = 728；
- 位置计数采用 CC0 下降沿并读取 B 相判向；CC1 仅用于周期数据；应用层不要把 CC0 与 CC1 事件相加。

编码器枚举顺序不是 A/B/C/D 顺序：

| ID | 物理位置 | 定时器 |
|---|---|---|
| BSP_ENCODER_LF | C/M3 | TIMG7 |
| BSP_ENCODER_LB | D/M4 | TIMA1 |
| BSP_ENCODER_RF | B/M2 | TIMG6 |
| BSP_ENCODER_RB | A/M1 | TIMG0 |

### 6.2 核心 API

| API | 参数 | 合法值/说明 |
|---|---|---|
| bsp_encoder_init(cfg, count, pulses_per_rev) | cfg：配置数组；count：1~4；pulses_per_rev：>0 | 初始化并清零计数 |
| bsp_encoder_get_count(id) | id：LF/LB/RF/RB | 读单路累计计数，不清零 |
| bsp_encoder_get_all_counts(counts) | 数组长度至少 4 | 读四路当前计数，不清零 |
| bsp_encoder_get_all_totals(totals) | 数组长度至少 4 | 读不受测速窗口清零影响的总计数 |
| bsp_encoder_clear_count(id) | 同上 | 清零单路窗口计数 |
| bsp_encoder_clear_all_counts() | 无 | 清零四路窗口计数 |
| bsp_encoder_get_and_clear_count(id) | 同上 | 原子取值并清零 |
| bsp_encoder_get_and_clear_all(deltas) | 数组长度至少 4 | 原子取四路窗口增量并清零 |
| bsp_encoder_get_all_rpm_mt(rpms, had_edge) | rpms/had_edge 长度至少 4 | 当前推荐 RPM 接口；返回 M/T 混合结果 |
| bsp_encoder_get_pulses_per_rev() | 无 | 返回当前每输出轴转计数 |
| bsp_encoder_counts_to_rpm(delta, dt_ms) | delta：有符号计数；dt_ms：>0 | 由窗口增量换算 RPM |
| bsp_encoder_rpm_to_pulse(rpm, dt_ms) | rpm：目标转速；dt_ms：>0 | 计算窗口期望脉冲数 |
| bsp_encoder_get_diag(id, out) | out：诊断结构体 | 读取计数、方向、周期、CC0/CC1 统计 |

RPM 计算的输入必须与 pulses_per_rev 口径一致；不要手动再乘一次减速比或倍频。

### 6.3 示例

~~~c
/* 示例 1：读取四路累计编码数 */
int32_t total[4];
if (bsp_encoder_get_all_totals(total) == BSP_OK) {
    /* 顺序 LF/C、LB/D、RF/B、RB/A */
}
~~~

~~~c
/* 示例 2：每个控制周期读取 RPM */
int32_t rpm[4];
bool edge[4];
if (bsp_encoder_get_all_rpm_mt(rpm, edge) == BSP_OK) {
    int32_t rpm_m1 = rpm[BSP_ENCODER_RB];
    bool m1_valid = edge[BSP_ENCODER_RB];
}
~~~

~~~c
/* 示例 3：工厂诊断单路捕获数据 */
bsp_encoder_diag_t diag;
if (bsp_encoder_get_diag(BSP_ENCODER_RB, &diag) == BSP_OK) {
    printf("cc0=%lu cc1=%lu total=%ld period=%u\n",
           (unsigned long)diag.cc0_event_count,
           (unsigned long)diag.cc1_event_count,
           (long)diag.total,
           (unsigned)diag.period);
}
~~~

## 7. ADC、电流与电池电压

### 7.1 硬件配置

配置文件：Config/empty.syscfg、Config/project_config.h；API 头文件：BSP/Peripherals/bsp_adc.h。

| 通道 | ADC MEM | 引脚 | 含义 |
|---|---|---|---|
| BSP_ADC_CH_M1_CURRENT | MEM0 | PA15 | M1 LMV321 输出 |
| BSP_ADC_CH_M2_CURRENT | MEM1 | PA16 | M2 LMV321 输出 |
| BSP_ADC_CH_M3_CURRENT | MEM2 | PA17 | M3 LMV321 输出 |
| BSP_ADC_CH_M4_CURRENT | MEM3 | PA22 | M4 LMV321 输出 |
| BSP_ADC_CH_BATTERY | MEM4 | PB18 | 电池分压检测点 |

### 7.2 核心 API

| API | 参数 | 合法值/说明 |
|---|---|---|
| bsp_adc_init() | 无 | 配置 ADC1、序列和中断 |
| bsp_adc_start_all() | 无 | 启动一次全序列采样 |
| bsp_adc_read_raw(channel, &raw) | channel：0~4；raw：输出指针 | 读取指定通道原始值 |
| bsp_adc_read_voltage(channel, &voltage) | 同上 | 读取检测点电压，工程实现单位为 mV |
| bsp_adc_start_conversion(channel) | channel：0~4 | 启动单通道转换 |
| bsp_adc_read_sequence(raw_values, count) | 数组；count：至少 5 | 读取序列原始值 |
| bsp_adc_get_last_raw(channel) | channel：0~4 | 返回最近一次原始值 |
| bsp_adc_get_last_voltage_mv(channel) | channel：0~4 | 返回最近检测点电压 mV |
| bsp_adc_get_last_current_raw(motor_idx) | motor_idx：0~3，对应 M1~M4 | 返回最近电流原始值 |
| bsp_adc_get_last_current_ma(motor_idx) | motor_idx：0~3 | 返回校准后的电流 mA |
| bsp_adc_get_all_currents_ma(currents_ma) | 数组长度至少 4 | 输出四路电流 mA |
| bsp_adc_get_bus_voltage_mv() | 无 | 返回电池总线电压 mV |
| bsp_adc_is_conversion_done() | 无 | 返回序列完成标志 |
| bsp_adc_clear_done_flag() | 无 | 清除完成标志 |

原始值转换：

    检测点电压(mV) = raw × PRJ_ADC_VREF_MV / PRJ_ADC_RESOLUTION

当前配置为 VREF=3300 mV、分辨率尺度=4096。代码中统一使用 project_config.h 的宏，不在应用层写死 4095/4096。

电流换算仅适用于四路 LMV321 电流检测：

    I(A) = V_sense(V) / (((90.9 + 10) / 10) × 0.15)
    I(mA) = V_sense(mV) / (((90.9 + 10) / 10) × 0.15)

注意：该公式还需要空载零点和增益校准；PB18 电池通道只能使用分压比换算电池电压，不能套用电流公式。

### 7.3 使用步骤

1. 调用 bsp_adc_init()。
2. 周期任务调用 bsp_adc_start_all() 或等待已有 ADC 序列完成。
3. 用 bsp_adc_get_last_voltage_mv() 读取检测点电压；用 bsp_adc_get_last_current_ma() 读取电流。
4. 校准时分别记录空载值和带已知负载值，保存零点/比例参数到 project_config.h 或校准存储区。

### 7.4 示例

~~~c
/* 示例 1：读取一轮五通道原始值 */
uint16_t raw[5];
if (bsp_adc_read_sequence(raw, 5U) == BSP_OK) {
    uint32_t m1_mv = (uint32_t)raw[0] * PRJ_ADC_VREF_MV / PRJ_ADC_RESOLUTION;
    uint32_t bat_mv = (uint32_t)raw[4] * PRJ_ADC_VREF_MV / PRJ_ADC_RESOLUTION;
}
~~~

~~~c
/* 示例 2：读取四路电流和电池电压 */
float current_ma[4];
bsp_adc_get_all_currents_ma(current_ma);
uint32_t battery_sense_mv = bsp_adc_get_bus_voltage_mv();
~~~

~~~c
/* 示例 3：电流零点校准的最小做法 */
float zero_ma[4];
bsp_adc_get_all_currents_ma(zero_ma);  /* 电机断电/空载时采样并求平均 */
/* 后续实际电流 = measured_ma - zero_ma[motor] */
~~~

## 8. SPI1 与 LSM6DSR IMU

### 8.1 硬件配置

配置文件：Config/empty.syscfg；API 头文件：BSP/IMU/bsp_lsm6dsr.h、BSP/IMU/lsm6dsr.h。

| 信号 | 引脚 |
|---|---|
| SPI1 SCLK | PB16 |
| SPI1 MOSI | PB15 |
| SPI1 MISO | PB14 |
| CS | PA2 |

LSM6DSR 当前输出单位：加速度 m/s²，角速度 dps，俯仰/横滚/偏航 deg，温度 °C。

滤波器合法枚举：FILTER_TYPE_COMPLEMENTARY、FILTER_TYPE_LPF、FILTER_TYPE_EKF、FILTER_TYPE_LKF、FILTER_TYPE_MAHONY、FILTER_TYPE_MADGWICK、FILTER_TYPE_KF。

### 8.2 核心 API

上下文 API 适合新代码：

| API | 参数 | 返回值/说明 |
|---|---|---|
| bsp_lsm6dsr_init_ctx(ctx) | ctx：调用者持有的上下文指针 | 0 成功，非 0 失败 |
| bsp_lsm6dsr_update_ctx(ctx, data) | data：输出数据指针 | 0 成功；每次读取一帧 |
| bsp_lsm6dsr_calibrate_ctx(ctx) | ctx | 0 成功；校准期间必须保持静止 |
| bsp_lsm6dsr_destroy_ctx(ctx) | ctx | 释放/复位上下文 |
| bsp_lsm6dsr_get_bias_ctx(ctx, &bx,&by,&bz) | 输出偏置，单位 dps | 无返回值 |
| bsp_lsm6dsr_is_stationary_ctx(ctx) | ctx | 非 0 表示静止 |
| bsp_lsm6dsr_set_filter_ctx(ctx, type) | type：上述 7 种枚举 | 0 成功 |
| bsp_lsm6dsr_set_filter_param_ctx(ctx, param, value) | param：filter_param_t；value：按参数定义 | 0 成功，非法值失败 |
| bsp_lsm6dsr_vofa_format(buf, size, data) | buf：输出文本；size：缓冲区长度 | 返回写入字符数，失败为负值 |

兼容全局 API：bsp_lsm6dsr_init()、bsp_lsm6dsr_calibrate()、bsp_lsm6dsr_update(data)、bsp_lsm6dsr_get_data()、bsp_lsm6dsr_set_filter(type)、bsp_lsm6dsr_get_filter_type()、bsp_lsm6dsr_get_filter_name()。

### 8.3 使用步骤

1. SPI 和 CS 由 empty.syscfg 配置；启动时执行 SYSCFG_DL_init()。
2. 创建并清零 bsp_lsm6dsr_ctx_t。
3. 调用 init_ctx()，确认 WHO_AM_I 正确。
4. 保持 IMU 静止，调用 calibrate_ctx() 一次。
5. 按固定周期调用 update_ctx()；控制任务只读取最新数据，不在回调中阻塞 SPI。
6. 如需切换算法，调用 set_filter_ctx()；不要同时维护多套滤波状态。

### 8.4 示例

~~~c
/* 示例 1：上下文初始化、校准、读取 */
bsp_lsm6dsr_ctx_t imu;
bsp_lsm6dsr_data_t data;
memset(&imu, 0, sizeof(imu));
if (bsp_lsm6dsr_init_ctx(&imu) == 0) {
    (void)bsp_lsm6dsr_calibrate_ctx(&imu);  /* 校准时保持静止 */
    if (bsp_lsm6dsr_update_ctx(&imu, &data) == 0) {
        printf("roll=%.2f pitch=%.2f yaw=%.2f\n",
               data.roll, data.pitch, data.yaw);
    }
}
~~~

~~~c
/* 示例 2：切换滤波器并输出 VOFA 文本 */
char line[192];
(void)bsp_lsm6dsr_set_filter(FILTER_TYPE_KF);
const bsp_lsm6dsr_data_t *data = bsp_lsm6dsr_get_data();
int n = bsp_lsm6dsr_vofa_format(line, sizeof(line), data);
if (n > 0) {
    (void)bsp_uart_send_dma((const uint8_t *)line, (uint16_t)n);
}
~~~

## 9. 按键

### 9.1 硬件配置

配置文件：Config/empty.syscfg、Config/project_config.h；API 头文件：BSP/Input/key.h、BSP/Peripherals/bsp_key.h。

当前按键：PA7 的 KEY_key 为低电平有效按钮；PB3 的 KEY_switch 为低电平有效开关。扫描周期默认 10 ms，消抖 20 ms，长按 800 ms，卡键上限 5000 ms。

### 9.2 管理器 API

| API | 参数 | 合法值/说明 |
|---|---|---|
| bsp_key_manager_init(manager, instances, configs, count, now_ms) | manager：管理器；instances/configs：连续数组；count：>0；now_ms：当前毫秒 | 初始化全部按键 |
| bsp_key_manager_poll(manager, now_ms) | now_ms：单调递增毫秒 | 每 10 ms 左右调用一次 |
| bsp_key_manager_get(manager, index) | index：0~count-1 | 返回 key_t 指针，越界返回 NULL |

底层单键 API：key_init()、key_reset()、key_poll()、key_enable()、key_disable()、key_get_state()、key_get_raw_level()。按键配置中的 active_level 只能使用高/低有效；debounce_ms、long_press_ms、max_hold_ms 应为非负毫秒值，max_hold_ms=0 表示关闭卡键检测。

应用层回调：

- app_key_short_press_action(key, timestamp_ms, pressed_duration_ms, user_data)
- app_key_long_press_action(key, timestamp_ms, pressed_duration_ms, user_data)
- app_key_stuck_action(key, timestamp_ms, pressed_duration_ms, user_data)

回调只做动作选择或调用运动控制门面；不要在回调中阻塞延时、直接操作 PWM 或执行长时间 SPI/ADC。

### 9.3 使用步骤

1. 在 project_config.h 修改扫描周期和阈值。
2. 准备按键实例数组和配置数组；配置 read_level 回调返回 PA7/PB3 原始电平。
3. 调用 bsp_key_manager_init() 一次。
4. 在按键任务中每 10 ms 调用 bsp_key_manager_poll()。
5. 在短按、长按、stuck 回调中调用 app_motion_* API。

### 9.4 示例

~~~c
/* 示例 1：按键任务扫描 */
void task_key(void *arg)
{
    bsp_key_manager_t manager;
    bsp_key_instance_t instances[2];
    bsp_key_config_t configs[2];
    uint32_t now = bsp_get_ms();
    if (bsp_key_manager_init(&manager, instances, configs, 2U, now) != KEY_STATUS_OK) {
        return;
    }
    for (;;) {
        now = bsp_get_ms();
        (void)bsp_key_manager_poll(&manager, now);
        osal_task_delay_ms(PRJ_KEY_SCAN_PERIOD_MS);
    }
}
~~~

~~~c
/* 示例 2：短按让 M1 维持 +200 RPM，长按相对左转 90 度 */
void app_key_short_press_action(const key_t *key, uint32_t ts,
                                uint32_t duration, void *user_data)
{
    (void)key; (void)ts; (void)duration; (void)user_data;
    (void)app_motion_speed_motor(APP_MOTION_MOTOR_A, 200.0f);
}

void app_key_long_press_action(const key_t *key, uint32_t ts,
                               uint32_t duration, void *user_data)
{
    (void)key; (void)ts; (void)duration; (void)user_data;
    (void)app_motion_angle_start_relative(90.0f, 120.0f, 5000U);
}
~~~

~~~c
/* 示例 3：卡键直接停车 */
void app_key_stuck_action(const key_t *key, uint32_t ts,
                          uint32_t duration, void *user_data)
{
    (void)key; (void)ts; (void)duration; (void)user_data;
    app_motion_stop();
}
~~~

## 10. 运动控制：速度环、位置环、角度环

### 10.1 硬件和配置

API 头文件：Application/Algorithm/app_motion_control.h、Application/Algorithm/app_position_control.h；参数文件：Config/project_config.h。

关键参数：

- 控制周期：PRJ_CONTROL_PERIOD_MS，当前 5 ms；
- 轮径：PRJ_MOTOR_WHEEL_DIAMETER_MM，当前 60 mm；
- 轮距：PRJ_CF_WHEEL_BASE_M，当前 0.19 m；
- 输出轴每转计数：由 PPR、减速比和解码倍频决定，当前 728；
- 位置反馈参与车体计算的配置由 project_config.h 的 M1/M4 反馈开关决定。

### 10.2 门面 API

| API | 参数 | 合法值/说明 |
|---|---|---|
| app_motion_control_init(ctx) | 已初始化的共享上下文 | true 成功；调用一次 |
| app_motion_speed_motor(motor, rpm) | motor：A~D；rpm：在工程最大 RPM 内，正负表示方向 | true 接受；其他电机目标置零 |
| app_motion_speed_all(rpm[4]) | A、B、C、D 四路 RPM | 数组非空、每路在允许范围 |
| app_motion_speed_all_timed(rpm[4], timeout_ms) | timeout_ms：0 或正毫秒 | 超时自动停止 |
| app_motion_position_start(distance_m, cruise_rpm, timeout_ms) | distance_m：正前进/负后退；cruise_rpm：必须 >0；timeout_ms：0 或正毫秒 | 启动位置环 |
| app_motion_angle_start_relative(delta_deg, cruise_rpm, timeout_ms) | delta_deg：正值左转约定；cruise_rpm：>0 | 启动相对角度环 |
| app_motion_angle_start_absolute(target_deg, cruise_rpm, timeout_ms) | target_deg：绝对 yaw；cruise_rpm：>0 | 启动绝对角度环 |
| app_motion_stop() | 无 | 清目标、停四路、复位动作状态 |
| app_motion_control_process(now_ms) | now_ms：单调递增毫秒 | 控制任务每周期调用 |
| app_motion_get_state() | 无 | IDLE/SPEED/POSITION/ANGLE/FAULT |
| app_motion_is_busy() | 无 | true 表示有动作运行 |

不要在多个任务里同时调用 app_motion_control_process()；只保留一个控制任务调用。

### 10.3 位置与角度换算

位置环使用：

    轮周 = π × 直径 = 2π × 半径
    目标计数 = 距离(m) / 轮周(m) × 输出轴每转计数

60 mm 轮、728 counts/output-rev 时，1 m 约对应 3868.7 个输出轴计数。实际代码以 project_config.h 为准。

角度环使用 IMU yaw 和差速轮控制；不在按键回调中自行读取 SPI，也不重复实现 PID。

### 10.4 示例

~~~c
/* 示例 1：四轮同速前进 200 RPM，持续 3 秒 */
const float forward[4] = { 200.0f, 200.0f, 200.0f, 200.0f };
(void)app_motion_speed_all_timed(forward, 3000U);
~~~

~~~c
/* 示例 2：前进 1 m，巡航 120 RPM，最长 8 秒 */
(void)app_motion_position_start(1.0f, 120.0f, 8000U);
~~~

~~~c
/* 示例 3：相对左转 90 度，巡航 100 RPM，最长 5 秒 */
(void)app_motion_angle_start_relative(90.0f, 100.0f, 5000U);
~~~

## 11. PID

### 11.1 配置

头文件：Application/Algorithm/app_pid.h；默认参数：Config/project_config.h。

当前默认值：Kp=0.8、Ki=0.3、Kd=0.0。输出上下限由初始化参数指定，不要在 compute() 后再无边界累加。

### 11.2 核心 API

| API | 参数 | 合法值/说明 |
|---|---|---|
| app_pid_init(pid, kp, ki, kd, mode, out_min, out_max) | pid：对象；kp/ki/kd：有限浮点；mode：位置式/增量式；out_min < out_max | 初始化并复位内部状态 |
| app_pid_set_params(pid, kp, ki, kd) | 有限浮点 | 更新三项参数 |
| app_pid_set_integral_limit(pid, min, max) | min <= max | 限制积分项 |
| app_pid_set_d_filter(pid, coeff) | 通常 0~1 | 设置微分滤波系数 |
| app_pid_set_setpoint(pid, setpoint) | 目标值 | 更新目标 |
| app_pid_compute(pid, feedback, dt_s) | feedback：反馈值；dt_s：>0 秒 | 返回限幅后的控制输出 |
| app_pid_reset(pid) | 无 | 清积分、误差和历史状态 |
| app_pid_get_error(pid) | 无 | 返回最近误差 |
| app_pid_get_integral(pid) | 无 | 返回当前积分项 |

### 11.3 示例

~~~c
/* 示例 1：创建速度 PID，输出限制为 -500~500 */
app_pid_t pid;
app_pid_init(&pid, 0.8f, 0.3f, 0.0f,
             APP_PID_MODE_INCREMENT, -500.0f, 500.0f);
app_pid_set_setpoint(&pid, 200.0f);
~~~

~~~c
/* 示例 2：5 ms 控制周期更新并输出到电机命令 */
float command = app_pid_compute(&pid, measured_rpm, 0.005f);
if (command > 500.0f) command = 500.0f;
if (command < -500.0f) command = -500.0f;
(void)bsp_motor_set_speed(BSP_MOTOR_A, (int32_t)command);
~~~

~~~c
/* 示例 3：换目标或停车时复位积分 */
app_pid_set_setpoint(&pid, 0.0f);
app_pid_reset(&pid);
~~~

## 12. SysConfig、工程配置和 Keil

### 12.1 文件职责

| 文件 | 修改内容 |
|---|---|
| Config/empty.syscfg | 引脚、UART、ADC MEM、PWM、SPI、定时器、中断和 DMA |
| Config/ti_msp_dl_config.h/c | SysConfig 生成文件；不要手工长期修改 |
| Config/project_config.h | 轮径、PPR、减速比、PWM/命令范围、控制周期、PID、ADC VREF、IMU 和功能开关 |
| keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx | 源文件、头文件搜索路径和编译分组 |

### 12.2 修改流程

1. 先修改 empty.syscfg 或 project_config.h。
2. 重新生成 SysConfig 文件。
3. 检查生成宏、定时器实例和引脚没有改变。
4. 在 Keil 执行 Clean/Rebuild。
5. 检查旧删除文件没有继续参与链接。
6. 硬件测试前先确认电机 power gate 为关闭，必要时使用示波器模式。

### 12.3 示例

~~~c
/* 示例 1：改变编码器和轮径参数 */
#define PRJ_MOTOR_ENCODER_PPR            (13U)
#define PRJ_MOTOR_GEAR_RATIO_NUMERATOR   (28U)
#define PRJ_ENCODER_DECODE_MULTIPLIER    (2U)
#define PRJ_MOTOR_WHEEL_DIAMETER_MM      (60.0f)
~~~

~~~c
/* 示例 2：改变 IMU 输出周期和控制周期 */
#define PRJ_CONTROL_PERIOD_MS            (5U)
#define PRJ_IMU_STREAM_PERIOD_MS         (200U)
~~~

## 13. API 速查

| 需求 | 直接调用 |
|---|---|
| 打印短日志 | bsp_uart_puts() |
| UART0 DMA | bsp_uart_send_dma() |
| UART1 发数据 | hal_uart_transmit_buf(HAL_UART_EXT, ...) |
| 打开电机电源 | bsp_motor_power_enable() |
| 设单路电机命令 | bsp_motor_set_speed() |
| 四路停机 | bsp_motor_stop_all() |
| 读编码累计数 | bsp_encoder_get_all_totals() |
| 读 RPM | bsp_encoder_get_all_rpm_mt() |
| 读 ADC 检测电压 | bsp_adc_get_last_voltage_mv() |
| 读四路电流 | bsp_adc_get_all_currents_ma() |
| 读电池总线电压 | bsp_adc_get_bus_voltage_mv() |
| 读 IMU | bsp_lsm6dsr_update_ctx() 或 bsp_lsm6dsr_update() |
| 按键扫描 | bsp_key_manager_poll() |
| 速度环 | app_motion_speed_motor()/app_motion_speed_all() |
| 位置环 | app_motion_position_start() |
| 角度环 | app_motion_angle_start_relative()/absolute() |
| 停止运动 | app_motion_stop() |
| PID 计算 | app_pid_compute() |

## 14. 常见错误

1. 把 UART1 当成 UART0 的 bsp_uart 缓冲区使用：UART1 当前只提供 HAL 接口。
2. DMA 忙时重复提交：先检查 bsp_uart_tx_idle()，或处理 BSP_ERR_BUSY。
3. 把电池 PB18 当电流通道：PB18 只能按电池分压比计算。
4. ADC 应用层固定写 4095：使用 project_config.h 的 PRJ_ADC_RESOLUTION。
5. 编码器 CC0 和 CC1 事件相加：CC0 是位置计数来源，CC1 是周期诊断/测速数据来源。
6. 速度环每路重复乘减速比：pulses_per_rev 已包含当前口径，应用层不要再乘。
7. 直接写 PWM compare 控制业务电机：业务代码调用 bsp_motor_set_speed() 或 app_motion_*。
8. 在按键回调里阻塞延时：回调只触发动作，持续控制由 task_control.c 的 app_motion_control_process() 完成。
9. 多个任务同时运行运动控制：只让一个控制任务调用 app_motion_control_process()。
10. 修改 SysConfig 生成文件后不 Clean/Rebuild：可能残留旧对象文件和错误宏。

## 15. 文件索引

- 硬件配置：D:/msp_project/temp2/MSPM0G3507_Project_temp/MSPM0G3507_Project/MSPM0G3507_FreeRTOS/Config/empty.syscfg
- 工程参数：D:/msp_project/temp2/MSPM0G3507_Project_temp/MSPM0G3507_Project/MSPM0G3507_FreeRTOS/Config/project_config.h
- GPIO/HAL：BSP/Peripherals/hal_gpio.h
- UART：BSP/Peripherals/hal_uart.h、BSP/Peripherals/bsp_uart.h
- 定时器：BSP/Peripherals/hal_timer.h、BSP/Peripherals/bsp_timer.h
- 电机：BSP/Peripherals/bsp_motor.h、BSP/Peripherals/bsp_drv8870.h
- 编码器：BSP/Peripherals/bsp_encoder.h
- ADC：BSP/Peripherals/bsp_adc.h
- IMU：BSP/IMU/bsp_lsm6dsr.h
- 按键：BSP/Input/key.h、BSP/Peripherals/bsp_key.h
- 运动控制：Application/Algorithm/app_motion_control.h、Application/Algorithm/app_position_control.h
- PID：Application/Algorithm/app_pid.h

文档版本：v0.1.0
最后核对日期：2026-07-28
