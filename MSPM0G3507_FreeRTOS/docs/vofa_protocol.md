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
val0,val1,val2,val3,val4,val5,val6,val7,val8,val9,val10\n
```

**格式规则 / Format Rules**:
- 逗号分隔的浮点数序列
- `\n`（LF, 0x0A）作为帧结束符
- 精度：`%.6f`（6 位小数）
- 无通道名称前缀（纯数值）

**示例 / Example**:
```
150.234567,200.000000,180.123456,0.000000,200.000000,200.000000,200.000000,0.000000,512.345678,700.123456,650.000000\n
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

## 3. 11 通道分配 / 11-Channel Mapping

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
| CH8 | `app_ff_compute()` | FF duty（选中电机） | ±1000 |
| CH9 | `status.pid_correction[]` | PID 修正量（选中电机） | ±1000 |
| CH10 | `status.output[]` | 实际 DUTY 输出（选中电机） | ±1000 |

### 分组说明 / Grouping

```
CH0 ──── CH3    实际 RPM (Actual RPM)        [反馈信号]
CH4 ──── CH7    目标 RPM (Target RPM)         [设定值]
CH8 ──────────  FF duty（选中电机）            [前馈信号]
CH9 ──────────  PID 修正量（选中电机）         [PID修正]
CH10 ─────────  实际 DUTY 输出（选中电机）     [执行信号]
```

### VOFA+ 图表建议 / VOFA+ Chart Recommendations

- **波形图 1**：CH0-3（实际 RPM）+ CH4-7（目标 RPM）— 观察跟踪性能
- **波形图 2**：CH8（FF duty）+ CH9（PID 修正）+ CH10（实际 DUTY）— 观察前馈和 PID 协作

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
| `FFe=x` | `FFe=1` | 使能/禁用前馈 | `[m] FF enabled/disabled (restart motor to apply)` |
| `FFk=x` | `FFk=1.34` | 设置前馈斜率 | `[m] FF_k = 1.3400` |
| `FFb=x` | `FFb=63.5` | 设置前馈截距 | `[m] FF_b = 63.5000` |
| `FFKp=x` | `FFKp=0.5` | 设置FF模式比例增益 | `[m] FF_Kp = 0.5000` |
| `FFKi=x` | `FFKi=0.1` | 设置FF模式积分增益 | `[m] FF_Ki = 0.1000` |
| `FFKd=x` | `FFKd=0.0` | 设置FF模式微分增益 | `[m] FF_Kd = 0.0000` |
| `Sweep` | `Sweep` | 自动扫频标定 | 扫频结果 + `k=xxx b=xxx (auto-applied)` |
| `Step` | `Step` | 阶跃响应辨识 | `K=xxx T=xxx fit=xx%` |
| `Step=x` | `Step=300` | 指定 PWM 的阶跃测试 | `K=xxx T=xxx fit=xx%` |
| `Auto` | `Auto` | 自动整定（辨识+极点配置） | PI 参数 + FF 增益 |
| `Auto=x` | `Auto=10` | 指定带宽(Hz)的自动整定 | PI 参数 + FF 增益 |
| `Menu` | `Menu` | 刷新菜单显示 | 显示当前参数 |

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

#### FFe — 使能/禁用前馈

```
输入: FFe=1
输出: [0] FF enabled (restart motor to apply)

输入: FFe=0
输出: [0] FF disabled (restart motor to apply)
```

- 值为 `0` 时禁用前馈，非 `0` 时使能
- 修改后需重新启动电机生效（按 q 停止，再按 d 启动）
- 不区分大小写（`ffe=1` 和 `FFe=1` 等效）

#### FFk / FFb — 设置前馈参数

```
输入: FFk=1.34
输出: [0] FF_k = 1.3400

输入: FFb=63.5
输出: [0] FF_b = 63.5000
```

- 直接修改前馈线性模型的斜率和截距
- 可通过 `Sweep` 命令自动计算，也可手动设置
- 参数立即生效（下次启动电机时应用到 PID）

#### FFKp / FFKi / FFKd — 设置 FF 模式 PID 参数

```
输入: FFKp=0.5
输出: [0] FF_Kp = 0.5000

输入: FFKi=0.1
输出: [0] FF_Ki = 0.1000

输入: FFKd=0.0
输出: [0] FF_Kd = 0.0000
```

- FF 模式使用独立的 PID 参数（位置式 PID，输出修正量）
- 普通模式使用 `Kp/Ki/Kd`（增量式 PID）
- FF 使能时自动切换到 FF PID 参数
- 参数范围：±100.0（超出范围自动限幅）

#### Step — 阶跃响应辨识

```
输入: Step
输出:
  [0] Step test starting (PWM=500)...
  [STEP] K=1.2345 (RPM/PWM)  T=0.156 (s)  omega_ss=617 (RPM)  fit=95.2%
  [STEP] FF_k auto-updated: 0.8104

输入: Step=300
输出:
  [0] Step test starting (PWM=300)...
  [STEP] K=1.2401 (RPM/PWM)  T=0.162 (s)  omega_ss=372 (RPM)  fit=93.8%
  [STEP] FF_k auto-updated: 0.8064
```

- 执行阶跃响应测试，辨识一阶模型 $K$ 和 $T$
- 默认 PWM=500（50% duty），可通过 `Step=x` 自定义
- 电机自动刹车 → 静止确认 → 施加阶跃 → 记录 2 秒 → 停止
- 辨识完成后自动更新前馈增益 $FF\_k = 1/K$
- 总耗时约 2.5 秒
- **不需要先按 d 启动电机**，Step 会自动控制电机
- 可用 Stop 命令中止测试

**拟合质量指标**：
- `fit > 90%`：模型与实测高度吻合 ✅
- `fit 70-90%`：模型可用，可能有摩擦或噪声影响
- `fit < 70%`：模型不准确，增大 PWM 或检查机械系统

#### Auto — 自动 PID 整定

```
输入: Auto
输出:
  [0] Auto-tune starting (PWM=500)...
  [AUTO] K=1.2345  T=0.156s  fit=95.2%
  [AUTO] PI applied: Kp=0.3962  Ki=2.5368  (BW=5.0Hz)
  [AUTO] FF_k=0.8104 (auto-applied)

输入: Auto=10
输出:
  [0] Auto-tune starting (PWM=500)...
  [AUTO] K=1.2345  T=0.156s  fit=95.2%
  [AUTO] PI applied: Kp=0.1981  Ki=1.2684  (BW=10.0Hz)
  [AUTO] FF_k=0.8104 (auto-applied)
```

- **一步完成**模型辨识 + PID 参数计算 + 前馈更新
- 默认闭环带宽 5Hz，可通过 `Auto=x` 指定 (Hz)
- 带宽范围：1~16 Hz（自动限幅）
- 使用零极点对消法（零极点对消 PI），详见 `docs/pid_controller.md`
- 自动更新 FF_k 和 Kp/Ki，**立即生效**
- 完成后再发送 `Run` 启动电机即可验证效果

#### Sweep — 自动扫频标定

```
输入: Sweep
输出:
  [0] Sweep starting (~40s)...
  [SWEEP] RPM=50 -> DUTY=126
  [SWEEP] RPM=100 -> DUTY=195
  [SWEEP] RPM=150 -> DUTY=263
  ...
  [SWEEP] Done
  [SWEEP] k=1.3402 b=63.5437 (auto-applied)
```

- 自动遍历 10 个 RPM 测试点（50→100→150→200→300→400→500→600→700→800）
- 每个点保持 3 秒稳态，采集最后 1 秒的平均 duty
- 完成后自动拟合 k/b 并使能前馈
- 执行期间电机自动运转，总耗时约 40 秒
- **不需要先按 d 启动电机**，Sweep 会自动控制电机
- 扫频结束后电机自动停止

#### Menu — 刷新菜单显示

```
输入: Menu
输出: (刷新菜单显示当前参数)
```

- 重新显示当前电机参数（PID、FF状态、目标RPM等）
- 在空闲状态下发送，无需先按 d 启动电机
- 用于查看当前参数而不需要发送其他命令

---

## 5. 输入验证增强 (v2.4) / Input Validation Enhancement (v2.4)

v2.4 版本使用 `safe_atof()` 替代标准库 `atof()`，提供更严格的输入验证：

| 特性 | `atof()` | `safe_atof()` |
|---|---|---|
| 空字符串 | 返回 0.0 | 返回 false（拒绝） |
| 非数值字符串 | 返回 0.0 | 返回 false（拒绝） |
| 前导空白 | 处理 | 处理 |
| NaN 输入 | 返回 NaN | 返回 false（拒绝） |
| Inf 输入 | 返回 Inf | 返回 false（拒绝） |
| 内部实现 | `strtod` | `strtof` |

```c
// safe_atof() 实现概要
bool safe_atof(const char *str, float *out)
{
    if (str == NULL || *str == '\0') {
        *out = 0.0f;
        return false;  // 空字符串拒绝
    }
    char *endptr;
    *out = strtof(str, &endptr);
    if (endptr == str || *endptr != '\0') {
        return false;  // 非数值或尾部有垃圾字符拒绝
    }
    if (!isfinite(*out)) {
        return false;  // NaN/Inf 拒绝
    }
    return true;
}
```

### PID 参数应用重构 (v2.4) / PID Parameter Apply Refactoring (v2.4)

v2.4 版本新增 `vofa_apply_pid_param()` 函数，统一 PID 参数应用逻辑：

```c
// v2.4 提取的公共函数
static bool vofa_apply_pid_param(app_shared_ctx_t *ctx, uint8_t motor_id,
                                  float value, app_pid_param_type_t type)
{
    if (motor_id >= BSP_MOTOR_COUNT) {
        return false;
    }
    if (!isfinite(value)) {
        printf("[%lu] rejected: NaN/Inf\r\n", (unsigned long)motor_id);
        return false;
    }

    float old_val;
    OSAL_CRITICAL_SECTION {
        switch (type) {
            case PID_PARAM_KP: old_val = ctx->pid[motor_id].kp; ctx->pid[motor_id].kp = value; break;
            case PID_PARAM_KI: old_val = ctx->pid[motor_id].ki; ctx->pid[motor_id].ki = value; break;
            case PID_PARAM_KD: old_val = ctx->pid[motor_id].kd; ctx->pid[motor_id].kd = value; break;
        }
    }
    printf("[%lu] %c%c %.2f -> %.2f\r\n", ...);
    return true;
}
```

**优势**：
- 消除重复的限幅和 NaN/Inf 检查代码
- 统一错误处理和日志输出格式
- 减少约 49 行重复代码

---

## 6. 命令解析流程 / Command Parsing Flow

### 解析函数 / Parsing Function

```
app_vofa_parse_cmd(line, cmd)
  │
  ├── 跳过前导空白
  ├── 跳过空行
  ├── 清零命令结构体
  │
  ├── 匹配 "step"       → VOFA_CMD_STEP
  │     └── 可选: "=x"  → cmd->value = x (PWM值)
  ├── 匹配 "auto"       → VOFA_CMD_AUTOTUNE
  │     └── 可选: "=x"  → cmd->value = x (带宽Hz)
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
  ├── 匹配 "sweep"      → VOFA_CMD_SWEEP
  ├── 匹配 "ffk="       → VOFA_CMD_SET_FF_K
  ├── 匹配 "ffb="       → VOFA_CMD_SET_FF_B
  ├── 匹配 "ffe="       → VOFA_CMD_SET_FF_ENABLE
  ├── 匹配 "ffkp="      → VOFA_CMD_SET_FF_KP
  ├── 匹配 "ffki="      → VOFA_CMD_SET_FF_KI
  ├── 匹配 "ffkd="      → VOFA_CMD_SET_FF_KD
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

## 7. 安全限制 / Safety Limits

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

## 8. VOFA+ 软件配置 / VOFA+ Software Setup

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

在 VOFA+ 中添加 11 个通道：

```
Channel 0:  Motor_A_RPM
Channel 1:  Motor_B_RPM
Channel 2:  Motor_C_RPM
Channel 3:  Motor_D_RPM
Channel 4:  Target_A
Channel 5:  Target_B
Channel 6:  Target_C
Channel 7:  Target_D
Channel 8:  FF_Duty
Channel 9:  PID_Correction
Channel 10: Actual_Duty
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
4. **命令发送无效**：所有命令随时可用（菜单状态或运行状态均可）

---

## 9. 带宽分析 / Bandwidth Analysis

### UART 带宽 / UART Bandwidth

| 参数 | 值 |
|---|---|
| 波特率 | 115200 baud |
| 有效带宽 | 11520 B/s (8N1, 10 bit/byte) |

### FireWater 帧大小 / FireWater Frame Size

每通道平均字符数：`-123.456789,` = 13 字节

```
11 通道 × 13 字节 = 143 字节
帧尾 '\n' = 1 字节
总计 ≈ 144 字节/帧
```

### 带宽占用 / Bandwidth Usage

| 参数 | 值 |
|---|---|
| 帧大小 | ~144 字节 |
| 输出周期 | 30 ms |
| 数据率 | 144 / 0.03 ≈ 4800 B/s |
| **占用率** | 4800 / 11520 ≈ **41.7%** |
| **剩余带宽** | ~6720 B/s |

### 设计裕量 / Design Margin

- 42% 占用率留有充足裕量
- DMA 非阻塞发送，不占用 CPU 时间
- 剩余带宽可容纳下行命令（通常 <100 B/s）
- 菜单交互（非 RUNNING 状态）时占用率 <1%

### 历史问题 / Historical Issue

- 早期版本使用 10ms 输出周期，此时带宽占用率高达 135%，导致 UART 输出缓冲区溢出、数据丢失。已修复为 30ms 周期。
- 早期版本使用阻塞 printf 发送 VOFA+ 数据（11.3ms/帧），占用菜单任务 CPU。已改为 DMA 非阻塞发送，释放 CPU 时间。

---

## 10. 停机行为设计 / Stop Behavior Design

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

## 11. 代码结构 / Code Structure

### 文件列表 / File List

| 文件 | 说明 |
|---|---|
| `app_vofa.h` | VOFA+ 协议接口（类型、常量、函数声明） |
| `app_vofa.c` | VOFA+ 协议实现（发送、解析、应用） |

### API 参考 / API Reference

| 函数 | 说明 |
|---|---|
| `app_vofa_send_firewater()` | 发送 FireWater 格式数据帧（阻塞） |
| `app_vofa_send_justfloat()` | 发送 JustFloat 格式数据帧（阻塞） |
| `app_vofa_parse_cmd()` | 解析下行命令字符串 |
| `app_vofa_apply_cmd()` | 应用解析后的命令到共享上下文 |
| `bsp_uart_send_dma()` | 通过 DMA 发送数据（非阻塞） |
| `bsp_uart_tx_idle()` | 查询 DMA 发送是否空闲 |

### 常量定义 / Constants

| 常量 | 值 | 说明 |
|---|---|---|
| `VOFA_JUSTFLOAT_TAIL_*` | 0x00,0x00,0x80,0x7F | JustFloat 帧尾 |
| `VOFA_FIREWATER_CH_MAX_LEN` | 32 | 单通道最大字符串长度 |
| `VOFA_CMD_MAX_LEN` | 64 | 下行命令最大长度 |
| `VOFA_PID_PARAM_MAX` | 100.0 | PID 参数限幅 |
| `VOFA_TARGET_RPM_MAX` | 800.0 | 目标 RPM 限幅 |

---

## 12. 调参工作流 / Tuning Workflow

### 纯 PID 调参

```
1. 菜单按 a 选择电机
2. 菜单按 b 设置目标 RPM（如 200）
3. 菜单按 d 启动电机
4. VOFA+ 发送 Target=300        ← 调整目标
5. VOFA+ 发送 Kp=1.2            ← 调整比例增益
6. VOFA+ 发送 Ki=0.5            ← 调整积分增益
7. 观察波形，重复调整
8. 按 q 回菜单，查看当前参数
9. 手动将参数复制到代码中
```

### 前馈+PID 调参

```
1. VOFA+ 发送 Sweep             ← 自动扫频（约 40 秒，自动使能前馈）
2. VOFA+ 发送 Run               ← 启动电机
3. VOFA+ 发送 Target=300        ← 调整目标
4. VOFA+ 发送 FFKp=0.5          ← 调整FF模式比例增益
5. VOFA+ 发送 FFKi=0.1          ← 调整FF模式积分增益
6. VOFA+ 发送 FFKd=0.0          ← 调整FF模式微分增益
7. 观察 CH8(FF duty) + CH9(PID修正) 波形
8. 发送 Stop 回菜单，查看当前参数
9. 手动将参数复制到代码中
```

### 一键自动整定 (推荐)

```
1. VOFA+ 发送 Auto              ← 自动辨识 + 极点配置（约 2.5 秒）
   [AUTO] K=1.2345  T=0.156s  fit=95.2%
   [AUTO] PI applied: Kp=0.3962  Ki=2.5368  (BW=5.0Hz)
   [AUTO] FF_k=0.8104 (auto-applied)

2. VOFA+ 发送 Run               ← 启动电机
3. VOFA+ 发送 Target=300        ← 设置目标

4. 观察 CH0-3(实际RPM) 跟踪 CH4-7(目标RPM)
   效果不满意可调整带宽:
   发送 Auto=8                 ← 以 8Hz 带宽重新整定

5. 发送 Stop 回菜单
6. 手动将参数复制到代码中
```

### 参数保存

菜单显示当前所有参数，用户手动复制到代码中：

```
=== Motor: B ===
Target: 200 RPM
A: KP=0.80 KI=0.30 KD=0.00
B: KP=0.80 KI=0.30 KD=0.00
C: KP=0.80 KI=0.30 KD=0.00
D: KP=0.80 KI=0.30 KD=0.00
FF: ON  k=1.340  b=63.5
FF_PID: KP=0.50 KI=0.10 KD=0.00
Motor: stopped
IMU: R=0.1 P=0.2 Y=0.3 H=0.4 V=0.001
a:Motor b:RPM c:PID d:Run q:Back
```

将 `KP/KI/KD`、`FF k/b` 和 `FF_PID KP/KI/KD` 值复制到 `project_config.h` 中的初始参数定义。

---