# 系统架构详解 / System Architecture Deep Dive

## 目录 / Table of Contents

1. [四层架构总览 / Layered Architecture Overview](#1-四层架构总览--layered-architecture-overview)
2. [模块依赖关系 / Module Dependency Graph](#2-模块依赖关系--module-dependency-graph)
3. [共享上下文 / Shared Context](#3-共享上下文--shared-context)
4. [任务模型 / Task Model](#4-任务模型--task-model)
5. [初始化流程 / Initialization Flow](#5-初始化流程--initialization-flow)
6. [编码器测速 / Encoder Speed Measurement](#6-编码器测速--encoder-speed-measurement)
7. [电机驱动 / Motor Driver](#7-电机驱动--motor-driver)
8. [数据流向 / Data Flow](#8-数据流向--data-flow)

---

## 1. 四层架构总览 / Layered Architecture Overview

系统采用严格的四层分层架构，自上而下为：App → BSP → HAL → OSAL。

The system follows a strict four-layer architecture: App → BSP → HAL → OSAL.

```
┌─────────────────────────────────────────────────────────────────┐
│  App Layer / 应用层                                              │
│  业务逻辑：PID 控制、菜单交互、VOFA+ 通信                         │
│  文件: app_main.c/h, app_pid.c/h, app_vofa.c/h                  │
│        Task/task_control.c/h, Task/task_menu.c/h                │
├─────────────────────────────────────────────────────────────────┤
│  BSP Layer / 板级支持包 (Board Support Package)                  │
│  硬件驱动封装：电机 PWM、编码器、UART、LED、ADC                   │
│  文件: bsp_motor.c/h, bsp_encoder.c/h, bsp_uart.c/h             │
│        bsp_led.c/h, bsp_adc.c/h, bsp_common.h                   │
├─────────────────────────────────────────────────────────────────┤
│  HAL Layer / 硬件抽象层 (Hardware Abstraction Layer)              │
│  寄存器级操作：直接操作 MCU 外设寄存器                             │
│  文件: hal_uart.c/h, hal_gpio.c/h, hal_timer.c/h, hal_adc.c/h   │
├─────────────────────────────────────────────────────────────────┤
│  OSAL Layer / 操作系统抽象层 (OS Abstraction Layer)               │
│  RTOS 封装：任务、临界区、延时、互斥锁、信号量、队列              │
│  文件: osal_task.c, osal_critical.c, osal_delay.c                │
│        osal_mutex.c, osal_sem.c, osal_queue.c                    │
├─────────────────────────────────────────────────────────────────┤
│  FreeRTOS Kernel / 内核                                          │
│  RTOS 内核实现                                                    │
├─────────────────────────────────────────────────────────────────┤
│  TI MSPM0 SDK / 芯片驱动库                                      │
│  厂商提供的底层驱动（SysConfig 生成配置）                         │
└─────────────────────────────────────────────────────────────────┘
```

### 各层职责 / Layer Responsibilities

| 层 | 职责 / Responsibility | 依赖 / Depends On |
|---|---|---|
| **App** | 业务逻辑：PID 算法、状态机菜单、VOFA+ 协议、任务管理 | BSP, OSAL |
| **BSP** | 板级硬件驱动：将 HAL 硬件操作封装为语义化 API | HAL, OSAL |
| **HAL** | 寄存器级硬件抽象：直接操作 MCU 外设寄存器 | TI MSPM0 SDK |
| **OSAL** | 操作系统抽象：封装 FreeRTOS API，支持裸机/RTOS 切换 | FreeRTOS |

### 设计原则 / Design Principles

- **单向依赖**：App → BSP → HAL → SDK，禁止反向依赖
- **OSAL 正交**：App 和 BSP 通过 OSAL 访问 RTOS 服务，不直接调用 FreeRTOS API
- **硬件无关**：App 层完全不包含硬件头文件（如 `ti_msp_dl_config.h`）
- **可测试性**：`app_pid.c/h` 纯算法模块，无任何硬件/RTOS 依赖，可独立单元测试

---

## 2. 模块依赖关系 / Module Dependency Graph

```
                    ┌─────────────┐
                    │  task_menu  │
                    │  (菜单任务)  │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │ app_vofa │ │ app_main │ │ app_pid  │
        │ (VOFA+)  │ │ (主入口)  │ │ (PID算法)│
        └────┬─────┘ └────┬─────┘ └────┬─────┘
             │            │            │
             ▼            ▼            │
        ┌──────────┐ ┌──────────┐      │
        │ bsp_uart │ │ bsp_motor│      │
        │ (串口)   │ │ (电机)   │      │
        └──────────┘ └────┬─────┘      │
                          │            │
        ┌─────────────┐   │            │
        │ task_control │───┘            │
        │ (控制任务)    │───────────────┘
        └──────┬──────┘
               │
               ▼
        ┌──────────┐
        │ bsp_encoder│
        │ (编码器)   │
        └──────────┘
```

### 头文件包含规则 / Header Inclusion Rules

| 模块 | 可包含 / May Include | 禁止包含 / Must Not Include |
|---|---|---|
| `task_control.c` | `app_main.h`, `app_pid.h`, `bsp_encoder.h`, `bsp_motor.h`, `osal_api.h` | `project_config.h` (间接), `ti_msp_dl_config.h` |
| `task_menu.c` | `app_main.h`, `app_pid.h`, `app_vofa.h`, `bsp_uart.h`, `osal_api.h` | `hal_*.h`, `ti_msp_dl_config.h` |
| `app_main.c` | 所有 BSP/OSAL 头文件（初始化职责） | — |
| `app_pid.c` | `<math.h>` (仅) | 任何 BSP/HAL/OSAL 头文件 |

---

## 3. 共享上下文 / Shared Context

### 结构体定义 / Structure Definition

控制任务和菜单任务之间通过 `app_shared_ctx_t` 共享数据：

```c
// app_main.h
typedef struct {
    int32_t rpm[BSP_MOTOR_COUNT];      // 4路电机实际RPM
    int32_t output[BSP_MOTOR_COUNT];   // 4路PID输出(duty)
} app_control_status_t;

typedef struct {
    app_pid_t            pid[BSP_MOTOR_COUNT];        // 4路PID实例
    bool                 motor_enabled[BSP_MOTOR_COUNT]; // 4路使能标志
    app_control_status_t status;                      // 控制状态
} app_shared_ctx_t;
```

### 数据所有权 / Data Ownership

| 字段 | 写入方 / Writer | 读取方 / Reader |
|---|---|---|
| `pid[i].kp/ki/kd` | 菜单任务 (tuning_pid / VOFA 命令) | 控制任务 (compute) |
| `pid[i].setpoint` | 菜单任务 (VOFA Target 命令) | 控制任务 (compute) |
| `motor_enabled[i]` | 菜单任务 (Run/Stop 命令) | 控制任务 (guard check) |
| `status.rpm[i]` | 控制任务 (encoder calc) | 菜单任务 (VOFA 输出) |
| `status.output[i]` | 控制任务 (PID compute) | 菜单任务 (VOFA 输出) |

### 同步机制 / Synchronization

所有跨任务共享数据的读写都通过 `OSAL_CRITICAL_SECTION`（映射到 FreeRTOS `taskENTER_CRITICAL`）保护：

```c
// 控制任务写 RPM（临界区内）
OSAL_CRITICAL_SECTION {
    ctx->status.rpm[i] = rpm_local[i];
}

// 菜单任务读 PID 参数（临界区内）
OSAL_CRITICAL_SECTION {
    kp = ctx->shared->pid[i].kp;
    ki = ctx->shared->pid[i].ki;
    kd = ctx->shared->pid[i].kd;
}
```

**为什么用临界区而不用互斥锁？**
- 临界区操作极快（关闭中断 + 恢复），仅保护简单变量读写
- 互斥锁有 FreeRTOS 调度开销，不适合 5ms 控制周期内的高频访问
- `taskENTER_CRITICAL` 是可重入的（嵌套计数），多层临界区嵌套安全

---

## 4. 任务模型 / Task Model

### 任务配置 / Task Configuration

| 属性 | 控制任务 (control) | 菜单任务 (menu) |
|---|---|---|
| **函数** | `app_control_task()` | `app_menu_task()` |
| **优先级** | 5 (最高) | 2 (低) |
| **栈大小** | 256 字 (1024 B) | 512 字 (2048 B) |
| **周期** | 5 ms | 100 ms (菜单) / 30 ms (运行) |
| **实时性** | 硬实时：PID 必须准时计算 | 软实时：菜单延迟可容忍 |

### 优先级设计理由 / Priority Design Rationale

- **控制任务 = 5 (最高)**: PID 闭环需要精确的 5ms 周期，任何抖动都会影响控制质量。如果菜单任务（含 printf、sscanf 等阻塞操作）优先级更高，会抢占控制任务导致控制周期不稳定。
- **菜单任务 = 2 (低)**: 串口交互是人类操作，100ms 的响应延迟完全不可感知。VOFA+ 输出是 30ms 周期，也不需要硬实时。

### 任务时序图 / Task Timing Diagram

```
时间轴 (ms) ─────────────────────────────────────────────►

Control Task (5ms):   |█|   |█|   |█|   |█|   |█|   |█|
                       0    5    10   15   20   25   30

Menu Task (Running, 30ms):
                      |██████████████|                   |██████████████|
                       0                              30               60

VOFA+ 输出周期:        ├──── 30ms ────┤                  ├──── 30ms ────┤
```

### 任务调度行为 / Scheduling Behavior

1. **菜单空闲时**（MAIN/MOTOR_SELECT/SET_RPM/TUNING_PID 状态）：
   - 菜单任务每 100ms 轮询一次 UART 输入
   - 大部分时间在 `osal_task_delay_ms(100)` 中阻塞
   - 控制任务独占 CPU 运行

2. **菜单运行时**（RUNNING 状态）：
   - 菜单任务每 30ms 执行一次 VOFA+ 输出
   - `printf()` 输出 12 通道数据约需 5~8ms（115200 baud）
   - 期间控制任务被延迟，但由于控制任务优先级更高，一旦控制任务就绪会立即抢占

3. **抢占场景**：
   - 当控制任务的 5ms 定时器到期时，即使菜单任务正在 `printf()`，控制任务也会抢占执行
   - `printf()` 的 UART 输出会暂停，控制任务完成后继续
   - 这保证了控制周期的稳定性

---

## 5. 初始化流程 / Initialization Flow

### 启动序列 / Startup Sequence

```
main()
  │
  ├── SYSCFG_DL_init()           ← SysConfig 生成的外设初始化
  │     ├── GPIO 初始化
  │     ├── Timer 初始化 (PWM + Encoder)
  │     ├── UART 初始化
  │     └── ADC 初始化
  │
  ├── board_init()                ← Board 级初始化
  │
  ├── app_main_init()             ← 应用层初始化
  │     ├── bsp_modules_init()    ← BSP 模块初始化
  │     │     ├── bsp_led_init()          → LED GPIO
  │     │     ├── bsp_uart_init()         → UART 环形缓冲区
  │     │     ├── bsp_motor_init()        → PWM Timer + GPIO
  │     │     └── bsp_encoder_init()      → Encoder Timer Capture
  │     │
  │     ├── pid_controllers_init()        → 4路PID参数初始化
  │     │
  │     ├── 清零 motor_enabled[]
  │     │
  │     ├── 串口打印启动信息
  │     │
  │     ├── osal_task_create(control)     → 创建控制任务
  │     └── osal_task_create(menu)        → 创建菜单任务
  │
  └── osal_scheduler_start()      ← 启动 FreeRTOS 调度器
        └── 任务开始并发执行
```

### 关键约束 / Key Constraints

- `app_main_init()` 必须在调度器启动前调用（任务创建依赖调度器未运行）
- BSP 初始化顺序：LED → UART → Motor → Encoder（UART 必须在 Motor 之前，因为 Motor 初始化可能输出调试信息）
- PID 初始化必须在任务创建前完成（任务启动后立即访问 PID 参数）

---

## 6. 编码器测速 / Encoder Speed Measurement

### M 法测速原理 / M-Method Speed Measurement

M 法（测频法）是在固定时间窗口内计数编码器脉冲数，通过脉冲数换算转速。

```
          ┌───────────────────────────┐
          │     dt = 5ms 窗口         │
          │                           │
  脉冲 ──►│ |||||||||                 │──► RPM = delta * 60000 / (PPR * dt_ms)
          │    ↑                      │
          │    delta = 脉冲计数        │
          └───────────────────────────┘
```

### RPM 计算公式 / RPM Formula

```
RPM = delta * 60000 / (PPR * dt_ms)

其中:
  delta    = 窗口内编码器脉冲数 (signed, 含方向)
  PPR      = 编码器每转脉冲数 (520)
  dt_ms    = 窗口时间 (5ms)
  60000    = 60 * 1000 (ms → min 转换)
```

### 数值分析 / Numerical Analysis

| 参数 | 值 |
|---|---|
| PPR | 520 |
| dt_ms | 5 ms |
| **1 RPM 对应脉冲数** | 520 * 5 / 60000 = 0.0433 |
| **1 个脉冲对应 RPM** | 60000 / (520 * 5) = 23.08 RPM |
| **检测下限** | ~23 RPM (1 个脉冲) |
| **中间变量类型** | `int64_t` (防溢出) |

### 低速量化误差 / Low-Speed Quantization Error

M 法在低速时存在量化误差：当转速低于 23 RPM 时，5ms 窗口内可能检测不到脉冲，导致 RPM 读数为 0。这是 M 法的固有限制，可通过以下方式改善：

1. **增大窗口时间**（`dt_ms`）：增加采样时间可降低检测下限，但会降低响应速度
2. **增大 PPR**：使用更高分辨率的编码器
3. **改用 T 法**（测周期法）：测量两个脉冲之间的时间间隔，低速精度更高但高速精度差
4. **M/T 法结合**：高速用 M 法，低速用 T 法（本项目未实现）

### 溢出安全 / Overflow Safety

```c
// bsp_encoder.c
int32_t bsp_encoder_counts_to_rpm(int32_t counts, uint32_t period_ms)
{
    // int64_t 中间变量，即使 counts=INT32_MAX 也不会溢出
    int64_t rpm = (int64_t)counts * 60000LL
                  / ((int64_t)PRJ_ENCODER_PULSES_PER_REV * (int64_t)period_ms);
    return (int32_t)rpm;
}
```

### 脉冲清零时序 / Pulse Clear Timing

```
Control Task (5ms cycle):
  │
  ├── bsp_encoder_get_and_clear_all(deltas)  ← 原子读取+清零
  ├── 计算 RPM
  ├── PID 计算
  ├── 设置电机 PWM
  └── osal_task_delay_ms(5)                  ← 等待下一个 5ms
```

在 5ms 窗口结束时读取并清零脉冲计数，确保：
- 不丢失脉冲（读和清零是原子操作）
- 不重复计数（每个窗口独立）
- 时间窗口一致（固定 5ms）

---

## 7. 电机驱动 / Motor Driver

### PWM 控制 / PWM Control

- **Timer**: 使用 MSPM0G3507 的 Timer 生成 PWM 波形
- **周期**: 由 `PRJ_MOTOR_PWM_PERIOD` 定义（通常 1000 或 10000）
- **Duty**: PID 输出直接映射为 duty 值，范围 `[-duty_max, +duty_max]`
- **方向**: 负 duty 表示反转

### 死区补偿 / Dead-Zone Compensation

电机驱动器存在最低启动电压（死区），低于此电压电机不会转动。`bsp_motor_set_speed()` 内部自动补偿死区：

```c
// 伪代码
if (|duty| > 0 && |duty| < dead_zone) {
    duty = sign(duty) * dead_zone;  // 补偿到死区值
}
```

### 停止模式 / Stop Modes

| 模式 | 行为 | 使用场景 |
|---|---|---|
| `BSP_MOTOR_MODE_BRAKE` | 电机两端短路，电磁制动 | 紧急停止、菜单停机 |
| `BSP_MOTOR_MODE_FREE` | 电机两端开路，自由滑行 | 自然减速 |

### 停机行为设计 / Stop Behavior Design

`app_motor_stop()` 执行以下操作：

1. `motor_enabled = false` — 禁用 PID 计算
2. `app_pid_reset()` — 清零内部状态（积分、历史误差）
3. `bsp_motor_stop(BRAKE)` — 硬件刹车

**注意**：停机时**保留 setpoint**（目标 RPM），这样重新启动时可以立即以之前的目标运行，无需重新设置。这是 v2 版本的设计改进。

---

## 8. 数据流向 / Data Flow

### 控制环路数据流 / Control Loop Data Flow

```
┌──────────┐    delta    ┌──────────┐    rpm     ┌──────────┐
│ Encoder  │───────────►│ M-Method │───────────►│  PID     │
│ (Timer   │            │ RPM Calc │            │ Compute  │
│ Capture) │            │          │            │          │
└──────────┘            └──────────┘            └────┬─────┘
                                                      │ output
                                                      ▼
                                                ┌──────────┐
                                                │  Motor   │
                                                │ PWM Out  │
                                                └──────────┘
```

### VOFA+ 数据流 / VOFA+ Data Flow

```
                          ┌──────────────┐
                          │  VOFA+ 软件  │
                          │  (PC 端)     │
                          └───┬──────┬───┘
                 下行命令 ↓   │      │  ↑ 上行数据
                          ┌───┘      └───┐
                          │              │
                     ┌────┴────┐   ┌─────┴────┐
                     │ UART RX │   │ UART TX  │
                     │ (中断)  │   │ (printf) │
                     └────┬────┘   └─────┬────┘
                          │              │
                     ┌────┴────┐   ┌─────┴────┐
                     │ Ring    │   │ 12ch     │
                     │ Buffer  │   │ FireWater│
                     └────┬────┘   └─────┬────┘
                          │              │
                     ┌────┴──────────────┴────┐
                     │     task_menu          │
                     │  (RUNNING 状态)         │
                     │  - 解析下行命令          │
                     │  - 输出上行数据          │
                     └────────────────────────┘
```

### UART 环形缓冲区 / UART Ring Buffer

串口使用 SPSC（Single Producer Single Consumer）环形缓冲区实现中断安全的数据传输：

- **生产者**: UART RX 中断（写入 head）
- **消费者**: 菜单任务（读取 tail）
- **无锁设计**: `volatile head/tail`，中断写、主循环读，无需互斥锁

```c
// bsp_common.h — SPSC Ring Buffer
typedef struct {
    uint8_t  *buf;        // 缓冲区指针
    uint32_t  size;       // 缓冲区大小 (2的幂)
    volatile uint32_t head; // 写入位置 (ISR 更新)
    volatile uint32_t tail; // 读取位置 (Task 更新)
} ring_buf_t;
```

### 带宽分析 / Bandwidth Analysis

**UART 波特率**: 115200 baud ≈ 11520 B/s

**VOFA+ FireWater 输出**:
- 每通道约 13 字节 (如 `123.456789,`)
- 12 通道 ≈ 156 字节/帧
- 30ms 周期 → 156 / 0.03 ≈ 5200 B/s
- **占用率**: 5200 / 11520 ≈ 45% ✅ 安全

**裕量**: 剩余 55% 带宽可用于下行命令和菜单交互。

---