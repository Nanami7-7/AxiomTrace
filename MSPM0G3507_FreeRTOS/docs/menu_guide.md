# 菜单交互与操作指南 / Menu & Operation Guide

## 目录 / Table of Contents

1. [概述 / Overview](#1-概述--overview)
2. [状态机流程图 / State Machine Diagram](#2-状态机流程图--state-machine-diagram)
3. [各状态详解 / State Details](#3-各状态详解--state-details)
4. [完整操作流程示例 / Step-by-Step Workflow](#4-完整操作流程示例--step-by-step-workflow)
5. [VOFA+ 运行模式 / VOFA+ Runtime Mode](#5-vofa-运行模式--vofa-runtime-mode)
6. [LED 心跳指示 / LED Heartbeat](#6-led-心跳指示--led-heartbeat)
7. [行输入机制 / Line Input Mechanism](#7-行输入机制--line-input-mechanism)

---

## 1. 概述 / Overview

串口菜单是一个基于状态机的交互界面，通过 UART 终端（如 PuTTY、SecureCRT、VOFA+）与系统交互。支持电机选择、RPM 设置、PID 调参、电机启停，以及 VOFA+ 远程调参模式。

The serial menu is a state machine-based interactive interface communicating through a UART terminal (PuTTY, SecureCRT, VOFA+). It supports motor selection, RPM setting, PID tuning, motor start/stop, and VOFA+ remote tuning mode.

### 串口参数 / Serial Parameters

| 参数 | 值 |
|---|---|
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | None |
| 换行 | CR 或 LF 或 CR+LF |

---

## 2. 状态机流程图 / State Machine Diagram

```
                    ┌───────────┐
          ┌────────►│   MAIN    │◄────────────────────┐
          │         │  (主菜单)  │                     │
          │         └─────┬─────┘                     │
          │               │                           │
          │     a/b/c/d/q │                           │
          │               ▼                           │
          │    ┌──────────┬──────────┬──────────┐     │
          │    │          │          │          │     │
          │    ▼          ▼          ▼          ▼     │
          │ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐     │
          │ │MOTOR │ │SET   │ │TUNING│ │RUNNING│    │
          │ │SELECT│ │RPM   │ │PID   │ │       │    │
          │ └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘     │
          │    │q       │q       │q       │q         │
          │    └────────┴────────┴────────┘          │
          │                                          │
          └──────────────────────────────────────────┘
```

---

## 3. 各状态详解 / State Details

### MAIN — 主菜单

**触发**: 上电默认 / 从其他状态按 'q' 返回

**显示内容**:
```
=== Motor: A ===
A: KP=0.80 KI=0.30 KD=0.00
B: KP=0.80 KI=0.30 KD=0.00
C: KP=0.80 KI=0.30 KD=0.00
D: KP=0.80 KI=0.30 KD=0.00
a:Motor b:RPM c:PID d:Run q:Back
```

**按键功能**:

| 按键 | 跳转状态 | 说明 |
|---|---|---|
| `a` | MOTOR_SELECT | 选择目标电机 |
| `b` | SET_RPM | 设置目标 RPM |
| `c` | TUNING_PID | 设置 PID 参数 |
| `d` | RUNNING | 启动当前电机，进入 VOFA+ 运行模式 |
| `q` | (无跳转) | 已在主菜单，无操作 |

**注**: 主菜单显示所有 4 路电机的 PID 参数，读取时在临界区内复制到局部变量再打印，避免数据竞争。

---

### MOTOR_SELECT — 电机选择

**显示**: `Select motor (a-d, q=back):`

**输入**:

| 输入 | 行为 |
|---|---|
| `a`~`d` | 选择电机 A~D (索引 0~3)，返回 MAIN |
| `q` | 返回 MAIN |

**示例**:
```
Select motor (a-d, q=back):
c
Motor C selected
```

---

### SET_RPM — 设置目标 RPM

**显示**: `Set RPM for Motor A (q=back):`

**输入**:

| 输入 | 行为 |
|---|---|
| 整数 (如 `200`) | 设置目标 RPM，返回 MAIN |
| `q` | 返回 MAIN |
| 非法输入 | 打印 `Invalid RPM!`，保持当前状态 |

**示例**:
```
Set RPM for Motor A (q=back):
300
Motor A target: 300 RPM
```

**解析**: 使用 `strtol()` 解析，支持负数（反转），区分 "0" 和非法输入。

---

### TUNING_PID — PID 调参

**显示**: `Set KP KI KD for Motor A (e.g. 1.5 0.3 0.1, q=back):`

**输入**:

| 输入 | 行为 |
|---|---|
| `Kp Ki Kd` (如 `1.5 0.3 0.1`) | 设置 PID 参数，返回 MAIN |
| `q` | 返回 MAIN |
| 非法输入 | 打印 `Invalid! e.g. 1.5 0.3 0.1`，保持当前状态 |

**示例**:
```
Set KP KI KD for Motor A (e.g. 1.5 0.3 0.1, q=back):
1.2 0.4 0.05
Motor A: KP=1.20 KI=0.40 KD=0.05
```

**注意**: PID 参数修改在临界区内执行，防止与控制任务的 PID 计算产生数据竞争。

---

### RUNNING — 运行模式

**显示**: `Motor A running (q=stop):`

**行为**:
1. 每 30ms 输出 12 通道 VOFA+ FireWater 数据
2. 每 30ms 检查一次下行命令输入
3. LED 心跳持续运行

**退出**:

| 输入 | 行为 |
|---|---|
| `q` | 停止当前电机，返回 MAIN |
| VOFA+ 命令 | 执行命令（见 vofa_protocol.md） |
| 未知命令 | 打印 `Unknown cmd: 'xxx'` |

---

## 4. 完整操作流程示例 / Step-by-Step Workflow

### 场景：从上电到运行电机 A

```
步骤 1: 连接串口终端 (115200 baud)
步骤 2: 按 Enter，显示主菜单

=== Motor: A ===
A: KP=0.80 KI=0.30 KD=0.00
B: KP=0.80 KI=0.30 KD=0.00
C: KP=0.80 KI=0.30 KD=0.00
D: KP=0.80 KI=0.30 KD=0.00
a:Motor b:RPM c:PID d:Run q:Back

步骤 3: 按 b 设置 RPM
Set RPM for Motor A (q=back):
300
Motor A target: 300 RPM

步骤 4: 按 c 调整 PID
Set KP KI KD for Motor A (e.g. 1.5 0.3 0.1, q=back):
1.0 0.4 0.05
Motor A: KP=1.00 KI=0.40 KD=0.05

步骤 5: 按 d 启动电机
Motor A running (q=stop):
[VOFA+ 数据开始输出到串口]

步骤 6: 在 VOFA+ 中观察波形，远程微调
发送: Kp=1.2
输出: [0] Kp 1.00 -> 1.20

发送: Target=350
输出: [0] Target 300 -> 350 RPM

步骤 7: 按 q 停止电机
Motor A stopped
[返回主菜单]
```

### 场景：通过 VOFA+ 命令操作

```
[在 RUNNING 状态下]

Motor=2        ← 切换到电机 C
Target=250     ← 设置电机 C 目标 250 RPM
Run            ← 启动电机 C
Kp=0.9         ← 调整电机 C 的 Kp
Ki=0.35        ← 调整电机 C 的 Ki
StopAll        ← 停止所有电机
Run=0          ← 启动电机 A
Run=1          ← 启动电机 B
Stop=0         ← 停止电机 A
```

---

## 5. VOFA+ 运行模式 / VOFA+ Runtime Mode

### 数据输出 / Data Output

- 格式：FireWater（逗号分隔浮点数 + `\n`）
- 周期：30ms
- 通道数：12

### 命令处理 / Command Processing

1. 每个 30ms 周期开始时检查 UART 输入缓冲区
2. 如果有完整行（`\n` 结尾），尝试解析为 VOFA+ 命令
3. 如果解析成功，执行命令并打印响应
4. 如果解析失败（非 VOFA+ 命令），检查是否为 'q'
5. 如果既不是 VOFA+ 命令也不是 'q'，打印 `Unknown cmd`

### 时序 / Timing

```
┌────── 30ms 周期 ──────┐
│                        │
│ 1. 检查命令输入 (~0ms) │
│ 2. 构建 12 通道数据     │
│ 3. 发送 FireWater 帧   │
│ 4. 延时至周期结束       │
│                        │
└────────────────────────┘
```

---

## 6. LED 心跳指示 / LED Heartbeat

LED 以 500ms 周期翻转（250ms 亮 / 250ms 灭），用于指示系统正常运行。

LED toggles at 500ms period (250ms on / 250ms off) to indicate system is running.

### 实现 / Implementation

使用分频计数器实现，不依赖额外定时器：

```c
ctx->led_counter += period;  // period = 当前循环延时 (100ms 或 30ms)
if (ctx->led_counter >= 500U) {
    ctx->led_counter = 0U;
    bsp_led_toggle();
}
```

### 状态指示 / Status Indication

| LED 行为 | 含义 |
|---|---|
| 500ms 周期翻转 | 系统正常运行 |
| LED 熄灭 | 系统卡死（watchdog 未实现） |

---

## 7. 行输入机制 / Line Input Mechanism

### 功能 / Features

- **逐字符读取**：从 UART 环形缓冲区逐字节读取
- **回显**：输入字符实时显示在终端
- **退格支持**：按退格键删除末字符
- **换行检测**：`\r` 或 `\n` 触发行完成
- **缓冲区限制**：最长 63 字符（`MENU_LINE_BUF_SIZE - 1`）

### 输入处理流程 / Input Processing Flow

```
UART RX 中断 → 环形缓冲区 → menu_read_line() → line_buf
                                    │
                  ┌─────────────────┤
                  │                 │
              可打印字符         控制字符
              存入+回显          \r/\n: 行完成
                                BS/DEL: 退格
                                其他: 忽略
```

### 退格处理 / Backspace Handling

支持两种退格字符：
- `0x7F` (DEL) — 终端默认退格
- `0x08` (BS) — 部分终端使用

退格时输出 `\b \b`（退格 + 空格覆盖 + 退格）实现终端字符擦除。

---