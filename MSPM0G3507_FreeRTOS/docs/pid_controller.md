# PID 控制器算法详解 / PID Algorithm Deep Dive

## 目录 / Table of Contents

1. [PID 控制基础 / PID Control Basics](#1-pid-控制基础--pid-control-basics)
2. [位置式 PID / Position-Form PID](#2-位置式-pid--position-form-pid)
3. [增量式 PID / Incremental PID](#3-增量式-pid--incremental-pid)
4. [本项目采用的算法 / Algorithm Used in This Project](#4-本项目采用的算法--algorithm-used-in-this-project)
5. [抗积分饱和 / Anti-Windup](#5-抗积分饱和--anti-windup)
6. [首次运行处理 / First-Run Handling](#6-首次运行处理--first-run-handling)
7. [微分滤波 / Derivative Filtering](#7-微分滤波--derivative-filtering)
8. [NaN/Inf 保护 / NaN/Inf Protection](#8-naninf-保护--naninf-protection)
9. [多实例设计 / Multi-Instance Design](#9-多实例设计--multi-instance-design)
10. [参数整定指南 / Parameter Tuning Guide](#10-参数整定指南--parameter-tuning-guide)
11. [代码结构 / Code Structure](#11-代码结构--code-structure)

---

## 1. PID 控制基础 / PID Control Basics

PID (Proportional-Integral-Derivative) 控制器是工业控制中最广泛使用的反馈控制器。它通过计算设定值（setpoint）与实际值（feedback）之间的误差（error），生成控制输出。

PID (Proportional-Integral-Derivative) controller is the most widely used feedback controller in industrial control. It generates control output by computing the error between the setpoint and the actual feedback value.

### 三个分量 / Three Components

| 分量 | 公式 | 作用 / Purpose |
|---|---|---|
| **P (比例)** | `Kp × e` | 对当前误差做出响应，误差越大输出越大 |
| **I (积分)** | `Ki × ∫e·dt` | 消除稳态误差，积累历史误差 |
| **D (微分)** | `Kd × de/dt` | 预测误差趋势，抑制超调 |

### 误差定义 / Error Definition

```
e(k) = setpoint - feedback

其中:
  setpoint  = 目标值 (目标RPM)
  feedback  = 反馈值 (编码器实际RPM)
```

---

## 2. 位置式 PID / Position-Form PID

### 公式 / Formula

```
u(k) = Kp × e(k) + Ki × ∫e·dt + Kd × de/dt
```

离散化（梯形积分）/ Discretized (Trapezoidal Integration):

```
u(k) = Kp × e(k) + Ki × Σ[(e(i)+e(i-1))×dt/2] + Kd × [e(k)-e(k-1)]/dt
```

### 优缺点 / Pros & Cons

| 优点 / Pros | 缺点 / Cons |
|---|---|
| 简单直观 | 积分项持续累积，容易饱和 |
| 适合大多数场景 | 大幅设定值变化时输出跳变 |
| | 需要额外的抗饱和处理 |

### 本项目支持但未使用 / Supported but Not Used

本项目的 `app_pid.c` 完整实现了位置式 PID，但实际运行使用增量式 PID（`APP_PID_MODE_INCREMENT`）。位置式 PID 保留在代码中供参考和未来扩展。

---

## 3. 增量式 PID / Incremental PID

### 公式 / Formula

增量式 PID 计算的是**输出增量** Δu，而非绝对输出值：

```
Δu(k) = Kp × [e(k) - e(k-1)] + Ki × e(k) + Kd × [e(k) - 2×e(k-1) + e(k-2)]

u(k) = u(k-1) + Δu(k)
```

其中：
- `Δe = e(k) - e(k-1)` — 一阶差分（偏差增量）
- `Δ²e = e(k) - 2×e(k-1) + e(k-2)` — 二阶差分

### 推导过程 / Derivation

从位置式 PID 出发：

```
u(k)   = Kp×e(k) + Ki×∫e(k) + Kd×de(k)/dt
u(k-1) = Kp×e(k-1) + Ki×∫e(k-1) + Kd×de(k-1)/dt
```

相减得：

```
Δu = u(k) - u(k-1)
   = Kp×[e(k)-e(k-1)] + Ki×e(k)×dt + Kd×[e(k)-2e(k-1)+e(k-2)]/dt
```

当 dt 归一化为 1 时，即得到标准增量式公式。

### 优缺点 / Pros & Cons

| 优点 / Pros | 缺点 / Cons |
|---|---|
| 输出增量小，不会跳变 | 需要存储更多历史误差 |
| 天然抗饱和：增量有限 | 首次运行无历史数据 |
| 异常安全：故障时输出保持 | 算法稍复杂 |
| 计算量小 | — |

### 为什么选择增量式？ / Why Incremental?

1. **天然抗饱和**：增量式只计算变化量，即使误差很大，输出也不会突变
2. **故障安全**：如果某次计算异常（如编码器故障），输出保持在上次值，不会归零或满幅
3. **适合电机控制**：电机 PWM duty 突变会造成电流冲击，增量式输出平滑
4. **重启友好**：`app_pid_reset()` 清零 `integral`（即 u(k-1)），重启时从 0 开始爬升

---

## 4. 本项目采用的算法 / Algorithm Used in This Project

### 增量式 PID 实现细节 / Incremental PID Implementation Details

```c
// app_pid.c — 增量式 PID 核心计算

if (pid->is_first_run) {
    // 首次运行: 无历史偏差, 用 P+I 估算
    output = pid->kp * error + pid->ki * error;
} else {
    // 计算偏差增量
    float delta_error = error - pid->last_error;
    
    // 计算二阶差分
    float delta2_error = error - 2.0f * pid->last_error 
                         + pid->last_last_error;
    
    // 计算输出增量
    float delta_out = pid->kp * delta_error
                    + pid->ki * error
                    + pid->kd * delta2_error;
    
    // 累加到上次输出 (先限幅再累加)
    pid->integral = clamp_f(
        pid->integral + delta_out,
        pid->integral_min, pid->integral_max);
    
    output = clamp_f(pid->integral,
        pid->out_min, pid->out_max);
}

// 保存本次输出为下次的 u(k-1)
pid->integral = output;
```

### 关键设计点 / Key Design Points

1. **`integral` 的双重含义**：在增量式 PID 中，`integral` 变量存储的是"上次输出 u(k-1)"，而非传统位置式 PID 中的"积分累积和"。因此 `integral_min/max` 的限幅实际约束的是输出幅度。

2. **输出限幅顺序**：
   ```
   Δu → clamp(integral + Δu, integral_min, integral_max) → output
   → clamp(output, out_min, out_max) → 最终输出
   ```
   两级限幅确保输出不会超过 PWM duty 范围。

3. **`dt_s` 参数**：当前实现中 `dt_s` 仅用于增量式 PID 的 `delta2_error` 计算（当 `dt_s <= 0` 时 `delta2_error = 0`），不影响主要增量计算。

---

## 5. 抗积分饱和 / Anti-Windup

### 饱和问题 / Saturation Problem

当电机达到最大 PWM duty 仍无法达到目标 RPM 时，积分项会持续累积（饱和）。当目标 RPM 降低后，积分项需要很长时间才能释放，导致超调。

When the motor reaches maximum PWM duty but still cannot reach the target RPM, the integral term accumulates (winds up). When the target RPM decreases, it takes a long time for the integral to unwind, causing overshoot.

### 本项目的抗饱和策略 / Anti-Windup Strategy

本项目使用**积分限幅**（Integral Clamping）：

```c
pid->integral = clamp_f(pid->integral, 
    pid->integral_min,   // = -duty_max
    pid->integral_max);  // = +duty_max
```

**配置**：`integral_min/max = out_min/max = ±duty_max`

这意味着积分项（即增量式 PID 中的累计输出）永远不会超过 PWM duty 的物理极限。

### 其他抗饱和方法（未使用） / Other Anti-Windup Methods (Not Used)

| 方法 | 原理 | 优缺点 |
|---|---|---|
| **积分限幅** ✅ | 限制积分项最大值 | 简单有效，本项目采用 |
| **条件积分** | 输出饱和时停止积分 | 效果更好但实现复杂 |
| **Back-Calculation** | 用实际输出与理想输出之差修正积分 | 最优但计算复杂 |
| **积分衰减** | 每周期将积分项乘以 <1 的系数 | 会降低稳态精度 |

---

## 6. 首次运行处理 / First-Run Handling

### 问题 / Problem

首次运行时，没有历史误差 `last_error` 和 `last_last_error`，直接使用增量式公式会导致 `delta_error` 和 `delta2_error` 仅基于当前误差计算，产生不正确的初始输出。

On first run, there are no historical errors `last_error` and `last_last_error`. Using the incremental formula directly would compute `delta_error` and `delta2_error` based only on current error, producing an incorrect initial output.

### 解决方案 / Solution

使用 P+I 近似估算初始输出：

```c
if (pid->is_first_run) {
    output = pid->kp * error + pid->ki * error;
}
```

这等效于位置式 PID 的第一拍输出（无积分累积、无微分项），提供合理的启动响应。

### `app_pid_reset()` 的作用 / Role of `app_pid_reset()`

```c
void app_pid_reset(app_pid_t *pid)
{
    pid->last_error      = 0.0f;
    pid->last_last_error = 0.0f;
    pid->integral        = 0.0f;
    pid->last_derivative = 0.0f;
    pid->is_first_run    = true;  // 标记下次运行为首次
}
```

`app_motor_stop()` 调用 `app_pid_reset()` 清零 PID 内部状态，下次启动时重新进入首次运行逻辑。**注意：`app_pid_reset()` 不改变 setpoint 和 kp/ki/kd 参数。**

---

## 7. 微分滤波 / Derivative Filtering

### 问题 / Problem

编码器测速存在量化噪声（尤其低速时），直接对误差求微分会放大高频噪声。

Encoder speed measurement has quantization noise (especially at low speed), and directly differentiating the error amplifies high-frequency noise.

### 滤波方案 / Filtering Solution

一阶低通滤波器 / First-Order Low-Pass Filter:

```c
if (pid->d_filter_coeff > 0.0f) {
    derivative = pid->last_derivative
        + (1.0f - pid->d_filter_coeff)
        * (derivative - pid->last_derivative);
}
pid->last_derivative = derivative;
```

**参数 / Parameter**:
- `d_filter_coeff`: 范围 [0, 1]
  - `0.0` = 无滤波（默认）
  - `0.5` = 中等滤波
  - `0.9` = 强滤波（响应慢）

### 本项目配置 / Project Configuration

本项目默认 `d_filter_coeff = 0.0`（无滤波），因为增量式 PID 本身对微分噪声不太敏感（二阶差分会部分抵消噪声）。如果实际使用中发现 PID 输出抖动，可以启用微分滤波：

```c
app_pid_set_d_filter(&ctx->pid[i], 0.5f);  // 启用中等滤波
```

---

## 8. NaN/Inf 保护 / NaN/Inf Protection

### 双层防护 / Two-Layer Protection

#### 第 1 层：`app_pid_compute()` 内部

```c
// app_pid.c — 计算完成后检查
if (!isfinite(output) || !isfinite(pid->integral) ||
    !isfinite(pid->last_error)) {
    app_pid_reset(pid);
    output = 0.0f;
}
```

如果 PID 内部状态出现 NaN/Inf（如编码器返回异常值），自动重置 PID 并返回 0。

#### 第 2 层：`task_control.c` 调用后检查

```c
// task_control.c — 调用 PID 后再次检查
OSAL_CRITICAL_SECTION {
    output = app_pid_compute(&ctx->pid[i], feedback, dt_s);
    
    if (!isfinite(output)) {
        // 输出警告日志
        AX_LOG_WARN("Motor %lu NaN/Inf! set=%.0f fb=%.0f", ...);
        output = 0.0f;
        app_pid_reset(&ctx->pid[i]);
    }
    
    ctx->status.output[i] = (int32_t)output;
}
```

即使第 1 层防护被绕过（理论上不会），第 2 层仍能捕获异常并保护电机。

### NaN/Inf 可能来源 / Possible NaN/Inf Sources

1. 编码器硬件故障导致脉冲计数异常
2. 浮点运算精度溢出（极端 PID 参数）
3. `dt_s` 为 0 时除以零（已通过 `dt_s > 0` 检查防护）

---

## 9. 多实例设计 / Multi-Instance Design

### 设计 / Design

`app_pid_t` 结构体包含 PID 控制器的全部状态，无任何全局/静态变量：

```c
typedef struct {
    float kp, ki, kd;           // 参数
    app_pid_mode_t mode;        // 模式
    float out_min, out_max;     // 输出限幅
    float integral_min, integral_max;  // 积分限幅
    float d_filter_coeff;       // 微分滤波系数
    float setpoint;             // 目标值
    float last_error;           // 上次偏差
    float last_last_error;      // 上上次偏差
    float integral;             // 积分/累计输出
    float last_derivative;      // 上次微分
    bool  is_first_run;         // 首次运行标志
} app_pid_t;
```

### 使用方式 / Usage

```c
// 初始化 4 路独立 PID
app_pid_t pid[4];
for (int i = 0; i < 4; i++) {
    app_pid_init(&pid[i], 0.8f, 0.3f, 0.0f,
        APP_PID_MODE_INCREMENT, -duty_max, duty_max);
}

// 每路独立计算
for (int i = 0; i < 4; i++) {
    float output = app_pid_compute(&pid[i], rpm[i], dt_s);
    bsp_motor_set_speed(i, (int32_t)output);
}
```

### 线程安全 / Thread Safety

`app_pid_t` 本身**不是**线程安全的。跨任务访问时必须使用 `OSAL_CRITICAL_SECTION`：

```c
// 正确 ✅：控制任务在临界区内计算
OSAL_CRITICAL_SECTION {
    output = app_pid_compute(&ctx->pid[i], feedback, dt_s);
}

// 正确 ✅：菜单任务在临界区内修改参数
OSAL_CRITICAL_SECTION {
    app_pid_set_params(&ctx->pid[i], kp, ki, kd);
}

// 错误 ❌：无保护的跨任务访问
ctx->pid[i].kp = new_kp;  // 可能与控制任务的 compute 竞争！
```

---

## 10. 参数整定指南 / Parameter Tuning Guide

### 默认参数 / Default Parameters

| 参数 | 默认值 | 说明 |
|---|---|---|
| Kp | 0.8 | 比例增益 |
| Ki | 0.3 | 积分增益 |
| Kd | 0.0 | 微分增益（未使用） |

### 整定步骤 / Tuning Procedure

#### Step 1: 纯 P 控制（Ki=0, Kd=0）

```
Kp: 0.1 → 0.3 → 0.5 → 0.8 → ...
Ki: 0
Kd: 0
```

逐步增大 Kp，直到：
- ✅ 电机能跟踪目标转速
- ❌ 开始出现持续振荡 → 取该值的 50~70%

#### Step 2: 加入积分（逐步增大 Ki）

```
Kp: (Step 1 确定值)
Ki: 0.05 → 0.1 → 0.2 → 0.3 → ...
Kd: 0
```

逐步增大 Ki，直到：
- ✅ 稳态误差消除
- ❌ 出现超调或积分饱和 → 取该值的 50~70%

#### Step 3: 微分（可选）

```
Kp: (Step 1 确定值)
Ki: (Step 2 确定值)
Kd: 0.01 → 0.05 → 0.1 → ...
```

增大 Kd 可以：
- ✅ 减少超调
- ❌ 过大会导致输出抖动

### 调参技巧 / Tuning Tips

1. **先 P 后 I 再 D**：按顺序调整，每次只改一个参数
2. **观察 VOFA+ 图表**：在 VOFA+ 中观察目标 RPM 和实际 RPM 的跟踪曲线
3. **从小到大**：参数从小值开始逐步增大，避免一开始就设过大导致震荡
4. **记录每次修改**：使用 VOFA+ 命令 `Kp=x` / `Ki=x` / `Kd=x` 修改，并记录效果
5. **注意量化误差**：低于 23 RPM 时 M 法测速失效，PID 无法精确控制

### VOFA+ 远程调参流程 / VOFA+ Remote Tuning Workflow

```
1. 菜单按 'd' 启动电机
2. VOFA+ 发送 Target=200     ← 设置目标 RPM
3. 观察图表，逐步调整：
   发送 Kp=1.0               ← 调整比例
   发送 Ki=0.5               ← 调整积分
   发送 Kd=0.01              ← 调整微分
4. 观察响应曲线，重复调整
5. 发送 Stop 或菜单按 'q'    ← 停止电机
```

---

## 11. 代码结构 / Code Structure

### 文件列表 / File List

| 文件 | 说明 |
|---|---|
| `app_pid.h` | PID 控制器接口定义（类型、函数声明） |
| `app_pid.c` | PID 控制器实现（纯算法，无硬件依赖） |

### API 参考 / API Reference

| 函数 | 说明 |
|---|---|
| `app_pid_init()` | 初始化 PID 控制器（参数、模式、限幅） |
| `app_pid_set_params()` | 运行时修改 Kp/Ki/Kd |
| `app_pid_set_setpoint()` | 设置目标值 |
| `app_pid_set_integral_limit()` | 设置积分限幅 |
| `app_pid_set_d_filter()` | 设置微分滤波系数 |
| `app_pid_compute()` | 执行一次 PID 计算，返回输出 |
| `app_pid_reset()` | 重置内部状态（保留参数和限幅） |
| `app_pid_get_error()` | 获取上次偏差 |
| `app_pid_get_integral()` | 获取积分累计值 |

### 调用关系 / Call Graph

```
task_control (5ms)
  │
  ├── bsp_encoder_get_and_clear_all()    → 读取脉冲
  ├── bsp_encoder_counts_to_rpm()        → 计算 RPM
  │
  ├── OSAL_CRITICAL_SECTION {
  │     ├── ctx->status.rpm[i] = rpm     → 写入共享状态
  │     │
  │     ├── if (motor_enabled[i]) {
  │     │     ├── app_pid_compute()       → PID 计算
  │     │     │     ├── clamp_f()
  │     │     │     └── isfinite()        → NaN 检查
  │     │     │
  │     │     ├── isfinite(output)        → 二次检查
  │     │     └── ctx->status.output[i]   → 写入输出
  │     │   }
  │     └── }
  │
  └── bsp_motor_set_speed()              → 设置 PWM
```

---