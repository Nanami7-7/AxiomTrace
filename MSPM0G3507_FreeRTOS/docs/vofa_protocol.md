# VOFA+ 协议详解 / VOFA+ Protocol Specification

## 目录 / Table of Contents

1. [概述 / Overview](#1-概述--overview)
2. [上行数据格式 / Uplink Data Format](#2-上行数据格式--uplink-data-format)
3. [12 通道分配 / 12-Channel Mapping](#3-12-通道分配--12-channel-mapping)
4. [下行命令规范 / Downlink Command Specification](#4-下行命令规范--downlink-command-specification)
5. [命令解析流程 / Command Parsing Flow](#5-命令解析流程--command-parsing-flow)
6. [安全限制 / Safety Limits](#6-安全限制--safety-limits)
7. [VOFA+ 软件配置 / VOFA+ Software Setup](#7-vofa-软件配置--vofa-software-setup)
8. [带宽分析 / Bandwidth Analysis](#8-带宽分析--bandwidth-analysis)
9. [停机行为设计 / Stop Behavior Design](#9-停机行为设计--stop-behavior-design)
10. [代码结构 / Code Structure](#10-代码结构--code-structure)

---

## 1. 概述 / Overview

VOFA+ 是一款通用的串口数据可视化工具，支持实时波形显示和命令发送。本项目使用 VOFA+ 实现：
- **上行数据**：实时显示 4 路电机的 RPM、目标值、PID 输出
- **下行命令**：远程修改 PID 参数、目标 RPM、启停电机

VOFA+ is a universal serial data visualization tool supporting real-time waveform display and command sending. This project uses VOFA+ for:
- **Uplink data**: Real-time display of 4-motor RPM, target, and PID output
- **Downlink commands**: Remote PID parameter tuning, target RPM setting, motor start/stop

### 支持的帧格式 / Supported Frame Formats

| 格式 | 类型 | 适用场景 | 本项目 |
|---|---|---|---|
| **FireWater** | 文本 (ASCII) | 调试、低速数据 | ✅ 默认使用 |
| **JustFloat** | 二进制 | 高速数据流 | ✅ 已实现（可切换） |

---

## 2. 上行数据格式 / Uplink Data Format

### FireWater 格式 (默认)

```
val0,val1,val2,val3,val4,val5,val6,val7,val8,val9,val10,val11\n
```

**格式规则 / Format Rules**:
- 逗号分隔的浮点数序列
- `\n`（LF, 0x0A）作为帧结束符
- 精度：`%.6f`（6 位小数）
- 无通道名称前缀（纯数值）

**示例 / Example**:
```
150.234567,200.000000,180.123456,0.000000,200.000000,200.000000,200.000000,0.000000,512.345678,700.123456,600.000000,0.000000\n
```

### JustFloat 格式 (备选)

```
[float_0][float_1]...[float_N][0x00][0x00][0x80][0x7F]
```

**格式规则 / Format Rules**:
- IEEE 754 单精度浮点数，小端序
- 帧尾：`0x00 0x00 0x80 0x7F`（即 `0x7F800000` 的小端序，IEEE 754 正无穷大）
- 无文本分隔符

---

## 3. 12 通道分配 / 12-Channel Mapping

### 通道表 / Channel Table

| 通道 | 变量 | 含义 | 范围 |
|---|---|---|---|
| CH0 | `status.rpm[0]` | 电机 A 实际 RPM | ±800 |
| CH1 | `status.rpm[1]` | 电机 B 实际 RPM | ±800 |
| CH2 | `status.rpm[2]` | 电机 C 实际 RPM | ±800 |
| CH3 | `status.rpm[3]` | 电机 D 实际 RPM | ±800 |
| CH4 | `pid[0].setpoint` | 电机 A 目标 RPM | ±800 |
| CH5 | `pid[1].setpoint` | 电机 B 目标 RPM | ±800 |
| CH6 | `pid[2].setpoint` | 电机 C 目标 RPM | ±800 |
| CH7 | `pid[3].setpoint` | 电机 D 目标 RPM | ±800 |
| CH8 | `imu.roll` | 横滚角 (度) | ±180 |
| CH9 | `imu.pitch` | 俯仰角 (度) | ±90 |
| CH10 | `app_cf_get_heading()` | 融合航向角 (度) | ±180 |
| CH11 | `app_cf_get_velocity_x()` | 融合线速度 (m/s) | ±1.0 |

### 分组说明 / Grouping

```
CH0 ──── CH3    实际 RPM (Actual RPM)        [反馈信号]
CH4 ──── CH7    目标 RPM (Target RPM)         [设定值]
CH8 ──── CH9    IMU 姿态 (Roll/Pitch)         [姿态信号]
CH10 ─── CH11   融合数据 (Heading/Velocity)   [融合信号]
```

### VOFA+ 图表建议 / VOFA+ Chart Recommendations

- **波形图 1**：CH0-3（实际 RPM）+ CH4-7（目标 RPM）— 观察跟踪性能
- **波形图 2**：CH8-9（Roll/Pitch）+ CH10（航向角）— 观察姿态变化
- **波形图 3**：CH11（融合速度）— 观察速度估计

---

## 4. 下行命令规范 / Downlink Command Specification

### 命令总览 / Command Overview

| 命令 | 语法 | 说明 | 响应 |
|---|---|---|---|
| `Kp=x` | `Kp=1.5` | 设置比例增益 | `[m] Kp old -> new` |
| `Ki=x` | `Ki=0.3` | 设置积分增益 | `[m] Ki old -> new` |
| `Kd=x` | `Kd=0.1` | 设置微分增益 | `[m] Kd old -> new` |
| `Target=x` | `Target=200` | 设置目标 RPM | `[m] Target old -> new RPM` |
| `Motor=x` | `Motor=0` | 选择电机 (0-3) | `Motor n selected` |
| `Run` | `Run` | 启动当前电机 | `[m] Run (target=x RPM)` |
| `Run=x` | `Run=1` | 启动指定电机 | `[m] Run (target=x RPM)` |
| `Stop` | `Stop` | 停止当前电机 | `[m] Stop` |
| `Stop=x` | `Stop=2` | 停止指定电机 | `[m] Stop` |
| `StopAll` | `StopAll` | 停止所有电机 | `All stopped` |

其中 `m` 为电机索引（0-3），`x` 为参数值。

### 详细说明 / Detailed Description

#### Kp / Ki / Kd — 设置 PID 参数

```
输入: Kp=1.5
输出: [0] Kp 0.80 -> 1.50

输入: Ki=0.5
输出: [0] Ki 0.30 -> 0.50

输入: Kd=0.01
输出: [0] Kd 0.00 -> 0.01
```

- 命令不区分大小写（`kp=1.5` 和 `Kp=1.5` 等效）
- 参数范围：±100.0（超出范围自动限幅）
- NaN/Inf 值被拒绝

#### Target — 设置目标 RPM

```
输入: Target=300
输出: [0] Target 200 -> 300 RPM
```

- 参数范围：±800.0（超出范围自动限幅）
- NaN/Inf 值被拒绝
- 设置后立即生效（PID 下一周期使用新目标）

#### Motor — 选择电机

```
输入: Motor=2
输出: Motor 2 selected
```

- 有效值：0-3（对应电机 A-D）
- 后续命令作用于新选中的电机

#### Run — 启动电机

```
输入: Run
输出: [0] Run (target=200 RPM)

输入: Run=1
输出: [1] Run (target=200 RPM)
```

- 启用 PID 计算和电机输出
- 使用当前 setpoint 作为目标 RPM
- 如果 setpoint=0，电机以 0 RPM 运行（PID 输出为 0）

#### Stop / StopAll — 停止电机

```
输入: Stop
输出: [0] Stop

输入: StopAll
输出: All stopped
```

- 停用 PID 计算
- 执行硬件刹车（BRAKE 模式）
- **保留 setpoint**（不归零），下次 Run 时以原目标恢复

---

## 5. 命令解析流程 / Command Parsing Flow

### 解析函数 / Parsing Function

```
app_vofa_parse_cmd(line, cmd)
  │
  ├── 跳过前导空白
  ├── 跳过空行
  ├── 清零命令结构体
  │
  ├── 匹配 "stopall"    → VOFA_CMD_STOP_ALL    (必须在 "stop" 之前)
  ├── 匹配 "stop"       → VOFA_CMD_STOP
  │     └── 可选: "=x"  → cmd->motor_id = x
  ├── 匹配 "run"        → VOFA_CMD_RUN
  │     └── 可选: "=x"  → cmd->motor_id = x
  ├── 匹配 "motor="     → VOFA_CMD_SET_MOTOR
  │     └── 解析 "=x"   → cmd->motor_id = x
  ├── 匹配 "target="    → VOFA_CMD_SET_TARGET
  │     └── 解析 "=x"   → cmd->value = x
  ├── 匹配 "kp="        → VOFA_CMD_SET_KP
  │     └── 解析 "=x"   → cmd->value = x
  ├── 匹配 "ki="        → VOFA_CMD_SET_KI
  │     └── 解析 "=x"   → cmd->value = x
  ├── 匹配 "kd="        → VOFA_CMD_SET_KD
  │     └── 解析 "=x"   → cmd->value = x
  │
  └── 无匹配 → return false
```

### 应用函数 / Apply Function

```
app_vofa_apply_cmd(cmd, ctx, current_motor)
  │
  ├── 如果 cmd->has_motor → 更新 current_motor
  │
  └── switch (cmd->type)
        ├── SET_KP    → 临界区写入 kp
        ├── SET_KI    → 临界区写入 ki
        ├── SET_KD    → 临界区写入 kd
        ├── SET_TARGET → 临界区写入 setpoint
        ├── SET_MOTOR  → 仅打印确认
        ├── RUN        → 临界区启用 motor_enabled
        ├── STOP       → app_motor_stop()
        └── STOP_ALL   → app_motor_stop_all()
```

### 匹配优先级 / Matching Priority

命令按以下顺序匹配，**顺序很重要**：

1. `"stopall"` — 必须在 `"stop"` 之前，否则 `"stopall"` 会被匹配为 `"stop"` + `"all"`
2. `"stop"` — 前缀匹配
3. `"run"` — 前缀匹配
4. `"motor="` — 精确前缀
5. `"target="` — 精确前缀
6. `"kp="` / `"ki="` / `"kd="` — 精确前缀

### 大小写敏感性 / Case Sensitivity

`strnicmp_prefix()` 实现了忽略大小写的前缀比较，因此以下命令等效：

```
Kp=1.5  =  kp=1.5  =  KP=1.5  =  Kp=1.5
Run     =  run     =  RUN
StopAll =  stopall =  STOPALL
```

---

## 6. 安全限制 / Safety Limits

### PID 参数限制 / PID Parameter Limits

| 参数 | 最小值 | 最大值 | NaN/Inf |
|---|---|---|---|
| Kp | -100.0 | +100.0 | 拒绝 |
| Ki | -100.0 | +100.0 | 拒绝 |
| Kd | -100.0 | +100.0 | 拒绝 |

### 目标 RPM 限制 / Target RPM Limits

| 参数 | 最小值 | 最大值 | NaN/Inf |
|---|---|---|---|
| Target | -800.0 | +800.0 | 拒绝 |

### 限幅行为 / Clamping Behavior

```c
// 超出范围时自动限幅到边界值
float val = cmd->value;
if (val > VOFA_PID_PARAM_MAX) { val = VOFA_PID_PARAM_MAX; }
if (val < -VOFA_PID_PARAM_MAX) { val = -VOFA_PID_PARAM_MAX; }

// NaN/Inf 直接拒绝，不修改参数
if (!isfinite(cmd->value)) {
    printf("[%lu] rejected: NaN/Inf\r\n", mid);
    return;
}
```

### 保护目的 / Protection Purpose

- **PID 参数 ±100**：防止极端参数导致控制不稳定或电机失控
- **Target ±800**：防止 PID 长期饱和运行（M 法测速在高 RPM 时精度足够）
- **NaN/Inf 拒绝**：防止浮点异常传播到 PID 计算

---

## 7. VOFA+ 软件配置 / VOFA+ Software Setup

### 连接设置 / Connection Settings

| 参数 | 值 |
|---|---|
| 协议 | Serial (UART) |
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | None |
| 帧格式 | FireWater |

### 通道配置 / Channel Configuration

在 VOFA+ 中添加 12 个通道：

```
Channel 0: Motor_A_RPM
Channel 1: Motor_B_RPM
Channel 2: Motor_C_RPM
Channel 3: Motor_D_RPM
Channel 4: Target_A
Channel 5: Target_B
Channel 6: Target_C
Channel 7: Target_D
Channel 8: Output_A
Channel 9: Output_B
Channel 10: Output_C
Channel 11: Output_D
```

### 下行命令发送 / Sending Downlink Commands

在 VOFA+ 的命令输入框中输入命令，按回车发送：

```
Target=300    ← 设置目标 RPM
Kp=1.2        ← 调整比例增益
Run           ← 启动电机
Stop          ← 停止电机
```

### 常见配置问题 / Common Setup Issues

1. **无数据显示**：检查波特率是否为 115200，帧格式是否为 FireWater
2. **数据乱码**：确保帧格式为 FireWater（文本模式），不是 JustFloat
3. **通道数值不对**：检查 VOFA+ 是否正确解析逗号分隔的 12 个浮点数
4. **命令发送无效**：确保在 RUNNING 状态下发送命令（菜单按 'd' 进入）

---

## 8. 带宽分析 / Bandwidth Analysis

### UART 带宽 / UART Bandwidth

| 参数 | 值 |
|---|---|
| 波特率 | 115200 baud |
| 有效带宽 | 11520 B/s (8N1, 10 bit/byte) |

### FireWater 帧大小 / FireWater Frame Size

每通道平均字符数：`-123.456789,` = 13 字节

```
12 通道 × 13 字节 = 156 字节
帧尾 '\n' = 1 字节
总计 ≈ 157 字节/帧
```

### 带宽占用 / Bandwidth Usage

| 参数 | 值 |
|---|---|
| 帧大小 | ~157 字节 |
| 输出周期 | 30 ms |
| 数据率 | 157 / 0.03 ≈ 5233 B/s |
| **占用率** | 5233 / 11520 ≈ **45.4%** |
| **剩余带宽** | ~6289 B/s |

### 设计裕量 / Design Margin

- 45% 占用率留有充足裕量
- 剩余带宽可容纳下行命令（通常 <100 B/s）
- 菜单交互（非 RUNNING 状态）时占用率 <1%

### 历史问题 / Historical Issue

早期版本使用 10ms 输出周期，此时带宽占用率高达 135%，导致 UART 输出缓冲区溢出、数据丢失。已修复为 30ms 周期。

---

## 9. 停机行为设计 / Stop Behavior Design

### 设计决策 / Design Decision

**方案 A（旧版）**：Stop 时清零 setpoint
- 问题：用户每次 Stop 后需要重新设置目标 RPM

**方案 B（当前）**：Stop 时保留 setpoint ✅
- 优势：Run 时自动以之前的目标恢复
- 安全性：`motor_enabled=false` 防止意外运动

### 各场景验证 / Scenario Verification

| 场景 | 行为 | 正确性 |
|---|---|---|
| 菜单 'q' → 'd' | setpoint 保留，但 'd' 用 ctx->target_rpm 覆写 | ✅ |
| VOFA Stop → Run | setpoint 保留，Run 使用原目标 | ✅ |
| VOFA StopAll → Run | 所有 setpoint 保留 | ✅ |
| 上电未运行 → Stop | setpoint=0，无影响 | ✅ |
| Stop → Target=x → Run | 新 setpoint 生效 | ✅ |

### VOFA+ 图表行为 / VOFA+ Chart Behavior

修改后，Stop 后 VOFA+ 图表中的"目标 RPM"通道会保持上一次的设定值（而非跳到 0），这提供了更好的可视化体验。

---

## 10. 代码结构 / Code Structure

### 文件列表 / File List

| 文件 | 说明 |
|---|---|
| `app_vofa.h` | VOFA+ 协议接口（类型、常量、函数声明） |
| `app_vofa.c` | VOFA+ 协议实现（发送、解析、应用） |

### API 参考 / API Reference

| 函数 | 说明 |
|---|---|
| `app_vofa_send_firewater()` | 发送 FireWater 格式数据帧 |
| `app_vofa_send_justfloat()` | 发送 JustFloat 格式数据帧 |
| `app_vofa_parse_cmd()` | 解析下行命令字符串 |
| `app_vofa_apply_cmd()` | 应用解析后的命令到共享上下文 |

### 常量定义 / Constants

| 常量 | 值 | 说明 |
|---|---|---|
| `VOFA_JUSTFLOAT_TAIL_*` | 0x00,0x00,0x80,0x7F | JustFloat 帧尾 |
| `VOFA_FIREWATER_CH_MAX_LEN` | 32 | 单通道最大字符串长度 |
| `VOFA_CMD_MAX_LEN` | 64 | 下行命令最大长度 |
| `VOFA_PID_PARAM_MAX` | 100.0 | PID 参数限幅 |
| `VOFA_TARGET_RPM_MAX` | 800.0 | 目标 RPM 限幅 |

---