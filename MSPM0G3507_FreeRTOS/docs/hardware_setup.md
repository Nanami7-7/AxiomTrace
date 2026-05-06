# 硬件连接与配置 / Hardware Setup & Configuration

## 目录 / Table of Contents

1. [LaunchPad 引脚分配 / Pin Mapping](#1-launchpad-引脚分配--pin-mapping)
2. [电机 PWM 输出 / Motor PWM Output](#2-电机-pwm-输出--motor-pwm-output)
3. [编码器输入 / Encoder Input](#3-编码器输入--encoder-input)
4. [UART 串口 / UART Serial](#4-uart-串口--uart-serial)
5. [驱动板接线 / Motor Driver Wiring](#5-驱动板接线--motor-driver-wiring)
6. [电源要求 / Power Requirements](#6-电源要求--power-requirements)
7. [接线注意事项 / Wiring Notes](#7-接线注意事项--wiring-notes)

---

## 1. LaunchPad 引脚分配 / Pin Mapping

### 系统时钟 / System Clock

| 参数 | 值 |
|---|---|
| CPU 时钟 | 80 MHz |
| PWM 时钟 | 20 MHz |
| 系统定时器 | SysTick HAL 实例 |

---

## 2. 电机 PWM 输出 / Motor PWM Output

使用 TIMA0 生成 4 路 PWM，频率 1 kHz，占空比范围 0~20000。

Uses TIMA0 to generate 4-channel PWM at 1 kHz, duty range 0~20000.

### PWM 通道分配 / PWM Channel Mapping

| 电机 | PWM 通道 | 引脚 | PINCM |
|---|---|---|---|
| A (左前) | CC0 | PA0 | PINCM1 |
| B (左后) | CC1 | PA1 | PINCM2 |
| C (右前) | CC2 | PA15 | PINCM37 |
| D (右后) | CC3 | PA12 | PINCM34 |

### 方向控制引脚 / Direction Control Pins

每路电机使用 TB6612FNG 驱动芯片，需要 2 个 GPIO 控制方向：

| 电机 | IN1 引脚 | IN2 引脚 | 正转 | 反转 | 刹车 | 滑行 |
|---|---|---|---|---|---|---|
| A (左前) | PB20 | PB24 | IN1=1,IN2=0 | IN1=0,IN2=1 | IN1=1,IN2=1 | IN1=0,IN2=0 |
| B (左后) | PA4 | PA7 | IN1=1,IN2=0 | IN1=0,IN2=1 | IN1=1,IN2=1 | IN1=0,IN2=0 |
| C (右前) | PB2 | PB3 | IN1=1,IN2=0 | IN1=0,IN2=1 | IN1=1,IN2=1 | IN1=0,IN2=0 |
| D (右后) | PA8 | PA9 | IN1=1,IN2=0 | IN1=0,IN2=1 | IN1=1,IN2=1 | IN1=0,IN2=0 |

### TB6612FNG 真值表 / TB6612FNG Truth Table

| IN1 | IN2 | PWM | 模式 |
|---|---|---|---|
| H | L | duty | 正转 (CW) |
| L | H | duty | 反转 (CCW) |
| H | H | 0 | 刹车 (Brake) |
| L | L | 0 | 滑行 (Free run) |

### PWM 参数 / PWM Parameters

| 参数 | 值 |
|---|---|
| 定时器 | TIMA0 |
| 时钟频率 | 20 MHz |
| PWM 周期 | 20000 (对应 1 kHz) |
| Duty 范围 | 0 ~ 20000 |
| 分辨率 | 1/20000 = 0.005% |

---

## 3. 编码器输入 / Encoder Input

使用 4 路 Timer Capture 模式采集编码器 A 相脉冲，B 相通过 GPIO 读取方向。

Uses 4 Timer Capture channels for encoder A-phase pulses, B-phase via GPIO for direction.

### 编码器参数 / Encoder Parameters

| 参数 | 值 |
|---|---|
| 单相 PPR | 13 |
| 减速比 | 20:1 |
| 2倍频 | 是 |
| **总 PPR** | 13 × 20 × 2 = **520** |

### Timer 分配 / Timer Assignment

| 编码器 | Timer 实例 | 位置 |
|---|---|---|
| 左前 (LF) | TIMG8 | 电机 A |
| 左后 (LB) | TIMG7 | 电机 B |
| 右前 (RF) | TIMG6 | 电机 C |
| 右后 (RB) | TIMG0 | 电机 D |

### B 相引脚 / B-Phase Pins

| 编码器 | B 相引脚 | PINCM | 方向修正 |
|---|---|---|---|
| 左前 (LF) | PA28 | — | -1 |
| 左后 (LB) | PA31 | — | -1 |
| 右前 (RF) | PA2 | — | -1 |
| 右后 (RB) | PA3 | — | -1 |

**方向修正说明**: `DIR_SIGN = -1` 表示编码器安装方向与电机正转方向相反，软件取反。

---

## 4. UART 串口 / UART Serial

| 参数 | 值 |
|---|---|
| 实例 | UART0 |
| TX 引脚 | PA10 (PINCM21) |
| RX 引脚 | PA11 (PINCM22) |
| 波特率 | 115200 |
| 数据格式 | 8N1 |
| 时钟源 | 40 MHz |

### 连接方式 / Connection

LaunchPad 的 UART0 通过板载 XDS110 调试器桥接到 USB，PC 端可直接通过虚拟串口通信。

LaunchPad's UART0 is bridged to USB via the onboard XDS110 debugger, allowing direct virtual COM port communication on PC.

---

## 5. 驱动板接线 / Motor Driver Wiring

### TB6612FNG 接线示意 / TB6612FNG Wiring Diagram

```
MSPM0G3507 LaunchPad          TB6612FNG           电机
─────────────────          ──────────────         ─────
PA0 (PWM_A)  ──────────── PWMA ──────────────  Motor A
PB20 (IN1_A) ──────────── AIN1
PB24 (IN2_A) ──────────── AIN2
                           AO1 ──────────────  Motor A (+)
                           AO2 ──────────────  Motor A (-)

PA1 (PWM_B)  ──────────── PWMB ──────────────  Motor B
PA4 (IN1_B)  ──────────── BIN1
PA7 (IN2_B)  ──────────── BIN2
                           BO1 ──────────────  Motor B (+)
                           BO2 ──────────────  Motor B (-)

... (C, D 同理)

VM ────────── 电机电源 (6~12V)
VCC ───────── 逻辑电源 (3.3V, 可接 LaunchPad 3.3V)
GND ───────── 共地 (必须与 LaunchPad GND 连接)
STBY ──────── 高电平使能 (接 VCC)
```

### 编码器接线 / Encoder Wiring

```
编码器 A 相 ──────────── Timer Capture 引脚 (见上方分配表)
编码器 B 相 ──────────── GPIO 引脚 (见上方 B 相引脚表)
编码器 VCC ───────────── 3.3V 或 5V (视编码器规格)
编码器 GND ───────────── 共地
```

---

## 6. 电源要求 / Power Requirements

### 电源域 / Power Domains

| 电源域 | 电压 | 来源 | 用途 |
|---|---|---|---|
| MCU 电源 | 3.3V | USB / LaunchPad | MSPM0G3507 + 逻辑电路 |
| 电机驱动电源 | 6~12V | 外部电源 | TB6612FNG VM 引脚 |
| 编码器电源 | 3.3V / 5V | LaunchPad / 外部 | 编码器供电 |

### 注意事项 / Notes

- **共地**：电机驱动电源 GND 必须与 LaunchPad GND 连接
- **独立供电**：建议电机电源独立于 MCU 电源，避免电机启动电流干扰
- **电源滤波**：在 TB6612FNG 的 VM 和 GND 之间并联 100μF 电解电容 + 0.1μF 陶瓷电容
- **STBY 引脚**：必须接高电平（VCC）才能使能驱动芯片

---

## 7. 接线注意事项 / Wiring Notes

### 布线建议 / Layout Tips

1. **短接线**：PWM 和编码器信号线尽量短，减少干扰
2. **屏蔽线**：编码器信号线建议使用屏蔽线或双绞线
3. **远离干扰源**：信号线远离电机电源线
4. **去耦电容**：每个 TB6612FNG 的 VM 引脚附近放置 0.1μF 陶瓷电容

### 常见接线错误 / Common Wiring Errors

| 错误 | 症状 | 解决 |
|---|---|---|
| 未共地 | 编码器读数不稳定，PID 异常 | 连接所有 GND |
| PWM/IN 接反 | 电机不转或单方向 | 检查引脚对应关系 |
| 编码器 A/B 相接反 | 方向判断错误 | 交换 A/B 相接线 |
| 电机电源不足 | 电机无力或不转 | 检查电源电压和电流容量 |
| STBY 未接高 | 驱动芯片不工作 | STBY 接 VCC |

### SysConfig 配置 / SysConfig Configuration

硬件引脚配置通过 `empty.syscfg` 文件管理，使用 TI SysConfig 工具图形化配置：

```
打开方式:
1. 在 Code Composer Studio 中双击 empty.syscfg
2. 或使用独立 SysConfig 工具
```

修改引脚后需重新生成 `ti_msp_dl_config.c/h`，并同步更新 `project_config.h`。

---