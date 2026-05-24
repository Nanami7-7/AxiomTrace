# 系统架构详解 / System Architecture Deep Dive

## 目录 / Table of Contents

1. [四层架构总览 / Layered Architecture Overview](#1-四层架构总览--layered-architecture-overview)
2. [模块依赖关系 / Module Dependency Graph](#2-模块依赖关系--module-dependency-graph)
3. [共享上下文 / Shared Context](#3-共享上下文--shared-context)
4. [任务模型 / Task Model](#4-任务模型--task-model)
5. [初始化流程 / Initialization Flow](#5-初始化流程--initialization-flow)
6. [编码器测速 / Encoder Speed Measurement](#6-编码器测速--encoder-speed-measurement)
7. [电机驱动 / Motor Driver](#7-电机驱动--motor-driver)
8. [模型参数辨识 / Model Parameter Identification](#8-模型参数辨识--model-parameter-identification)
9. [数据流向 / Data Flow](#9-数据流向--data-flow)

---

## 1. 四层架构总览 / Layered Architecture Overview

系统采用严格的四层分层架构，自上而下为：App → BSP → HAL → OSAL。

The system follows a strict four-layer architecture: App → BSP → HAL → OSAL.

```
┌─────────────────────────────────────────────────────────────────┐
│  App Layer / 应用层                                              │
│  业务逻辑：PID 控制、菜单交互、VOFA+ 通信、互补滤波               │
│  文件: app_main.c/h, app_pid.c/h, app_vofa.c/h                  │
│        app_feedforward.c/h, app_model_id.c/h                     │
│        app_complementary_filter.c/h                              │
│        Task/task_control.c/h, Task/task_imu.c/h, Task/task_menu.c│
├─────────────────────────────────────────────────────────────────┤
│  BSP Layer / 板级支持包 (Board Support Package)                  │
│  硬件驱动封装：电机 PWM、编码器、UART、LED、ADC、MPU6050 I2C     │
│  文件: bsp_motor.c/h, bsp_encoder.c/h, bsp_uart.c/h            │
│        bsp_led.c/h, bsp_adc.c/h, bsp_mpu6050.c/h              │
│        eMPL/inv_mpu.c/h (Invensense DMP 驱动)                 │
│        bsp_common.h                                             │
├─────────────────────────────────────────────────────────────────┤
│  HAL Layer / 硬件抽象层 (Hardware Abstraction Layer)              │
│  寄存器级操作：直接操作 MCU 外设寄存器                             │
│  文件: hal_uart.c/h, hal_gpio.c/h, hal_timer.c/h, hal_adc.c/h  │
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
          ┌────────────────────────┐    │
          │      task_control     │────┘
          │     (控制任务)         │─────────┐
          └──────────┬────────────┘         │
                     │                      │
          ┌──────────┴──────────┐  ┌────────┴────────┐
          │     bsp_encoder     │  │  app_model_id   │
          │      (编码器)       │  │  (模型辨识算法)  │
          └─────────────────────┘  └────────┬────────┘
                                            │
                                     ┌──────┴──────┐
                                     │   app_pid   │
                                     │ (PID算法)    │
                                     └─────────────┘

          ┌─────────────┐
          │  task_imu   │
          │  (IMU任务)   │
         └──────┬──────┘
                │
                ▼
         ┌──────────┐    ┌──────────────────┐
         │  inv_mpu  │──►│ app_complementary│
         │ (eMPL DMP)│    │    _filter      │
         └──────────┘    └──────────────────┘
                │
                ▼
         ┌──────────┐
         │bsp_mpu6050│
         │ (I2C bitbang)│
         └──────────┘
```

### 头文件包含规则 / Header Inclusion Rules

| 模块 | 可包含 / May Include | 禁止包含 / Must Not Include |
|---|---|---|
| `task_control.c` | `app_main.h`, `app_pid.h`, `app_model_id.h`, `bsp_encoder.h`, `bsp_motor.h`, `osal_api.h` | `project_config.h` (间接), `ti_msp_dl_config.h` |
| `task_menu.c` | `app_main.h`, `app_pid.h`, `app_vofa.h`, `bsp_uart.h`, `osal_api.h` | `hal_*.h`, `ti_msp_dl_config.h` |
| `app_main.c` | 所有 BSP/OSAL 头文件（初始化职责） | — |
| `app_pid.c` | `<math.h>` (仅) | 任何 BSP/HAL/OSAL 头文件 |
| `app_model_id.c` | `<math.h>` (仅) | 任何 BSP/HAL/OSAL 头文件 |
| `app_feedforward.c` | `<math.h>` (仅) | 任何 BSP/HAL/OSAL 头文件 |

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
| `imu.roll/pitch/yaw` | IMU任务 (DMP解算) | 菜单任务 (VOFA CH8-10) |

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

### 临界区最佳实践 (v2.4) / Critical Section Best Practices (v2.4)

**原则：尽量缩小临界区范围，仅保护必要的共享数据访问**

v2.4 版本优化了 `task_control.c` 中的临界区范围：

```c
// v2.4 之前的实现（❌ 临界区过大）
OSAL_CRITICAL_SECTION {
    // 编码器读取
    bsp_encoder_get_and_clear_all(deltas);
    // RPM 计算
    for (i = 0; i < BSP_MOTOR_COUNT; i++) {
        rpm[i] = bsp_encoder_counts_to_rpm(deltas[i], CONTROL_PERIOD_MS);
    }
    // PID 计算（耗时 ~16-36µs）
    for (i = 0; i < BSP_MOTOR_COUNT; i++) {
        if (ctx->motor_enabled[i]) {
            output = app_pid_compute(&ctx->pid[i], rpm[i], dt_s);
        }
    }
    // 共享数据写入
    ctx->status.rpm[i] = rpm[i];
    ctx->status.output[i] = (int32_t)output;
}
// 临界区时间过长，影响系统实时响应
```

```c
// v2.4 的实现（✅ 临界区最小化）
// 临界区外：编码器读取和 RPM 计算
bsp_encoder_get_and_clear_all(deltas);
for (i = 0; i < BSP_MOTOR_COUNT; i++) {
    rpm[i] = bsp_encoder_counts_to_rpm(deltas[i], CONTROL_PERIOD_MS);
}
// 临界区外：PID 计算（耗时 ~16-36µs）
for (i = 0; i < BSP_MOTOR_COUNT; i++) {
    if (ctx->motor_enabled[i]) {
        output = app_pid_compute(&ctx->pid[i], rpm[i], dt_s);
    }
}
// 仅共享数据写入在临界区内（~2µs）
OSAL_CRITICAL_SECTION {
    ctx->status.rpm[i] = rpm[i];
    ctx->status.output[i] = (int32_t)output;
}
// 临界区时间极短，对实时性影响最小
```

**优化效果**：
- 临界区时间：~16-36µs → ~2µs（减少 87.5%+）
- PID 计算在临界区外执行，不被中断延迟
- 系统可响应更高优先级的外设中断

**注意事项**：
- 读 `motor_enabled` 在临界区内确保一致性
- 写 `status.rpm/output` 在临界区内确保菜单任务读取时数据完整
- PID 计算中间变量（`rpm[]`、`output`）在临界区外，无数据竞争风险

---

## 4. 任务模型 / Task Model

### 任务配置 / Task Configuration

| 属性 | 控制任务 (control) | IMU任务 (imu) | 菜单任务 (menu) |
|---|---|---|---|
| **函数** | `app_control_task()` | `app_imu_task()` | `app_menu_task()` |
| **优先级** | 5 (最高) | 4 (中) | 2 (低) |
| **栈大小** | 384 字 (1536 B) | 256 字 (1024 B) | 512 字 (2048 B) |
| **周期** | 5 ms | 10 ms | 100 ms (菜单) / 30 ms (运行) |
| **实时性** | 硬实时：PID 必须准时计算 | 中等：IMU 数据更新可容忍少量抖动 | 软实时：菜单延迟可容忍 |

### 优先级设计理由 / Priority Design Rationale

- **控制任务 = 5 (最高)**: PID 闭环需要精确的 5ms 周期，任何抖动都会影响控制质量。如果菜单任务（含 printf、sscanf 等阻塞操作）优先级更高，会抢占控制任务导致控制周期不稳定。
- **IMU任务 = 4 (中)**: IMU 数据更新频率要求不高（10ms），DMP 解算需要一定时间，优先级介于 control 和 menu 之间避免抢占控制任务。
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
  ├── app_main_init()             ← 应用层初始化
  │     ├── bsp_modules_init()    ← BSP 模块初始化
  │     │     ├── bsp_led_init()          → LED GPIO
  │     │     ├── bsp_uart_init()         → UART 环形缓冲区
  │     │     ├── bsp_motor_init()        → PWM Timer + GPIO
  │     │     └── bsp_encoder_init()      → Encoder Timer Capture
    │     │
    │     ├── pid_controllers_init()        → 4路PID参数初始化
    │     ├── app_cf_init()                 → 互补滤波器初始化
    │     ├── app_id_init()                 → 模型辨识模块初始化
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

### M/T 法测速原理 / M/T-Method Speed Measurement

M/T 法结合了 M 法（测频）和 T 法（测周期）的优点：在固定窗口 **和** 一个完整脉冲边沿
结束时采样，同时测量脉冲数 `M` 和首次至末次边沿的实际时间 `t_ref`。

```
  固定窗口 (5ms)
  ┌──────────────────┐
  │        ┌──┐  ┌──┐│──┐
  │  ┌──┐  │  │  │  ││  │
──┘  └──┘──┘  └──┘  └──┘──
  ↑     ↑          ↑     ↑
  │     │          │     └─ 读取 + 清除
  │     │          └──────── t_ref (边沿捕获时间)
  │     └─────────────────── M = 脉冲数
  └───────────────────────── 窗口起点
```

### RPM 计算公式 / RPM Formula

```
RPM = M * 6000000 / (t_ref * PPR)

其中:
  M        = 有效脉冲数 (signed, 含方向)
  t_ref    = 从首次到末次边沿的时间 (μs)
  PPR      = 编码器每转脉冲数 (520)
  6000000  = 60 * 10^6 (μs → min 转换)
```

### 三分支处理 / Three-Branch Logic

`bsp_encoder_get_all_rpm_mt()` 根据 `M` 分为三种情况：

| 条件 | 处理方式 | 适用场景 |
|------|----------|----------|
| `M ≥ 2` | 标准 M/T：`RPM = M * 6000000 / (t_ref * PPR)` | 中高速，精度最高 |
| `M = 1` | 纯 T 法：`RPM = 6000000 / (t_ref * PPR)` | 极低速（一个脉冲跨越整个窗口） |
| `M = 0` | `RPM = 0.0` | 静止/超低速 |

### 数值分析 / Numerical Analysis

| 参数 | 值 |
|---|---|
| PPR | 520 |
| 窗口周期 | 5 ms |
| **检测下限 (M=1 时)** | t_ref ≤ 5ms → RPM ≥ 6000000/(5000*520) ≈ 2.3 RPM |
| **检测下限 (传统 M 法)** | ~23.08 RPM |
| **低速精度提升** | **~10 倍** |

### 实现细节 / Implementation Details

- **边沿捕获定时器**: 使用 Timer32 的捕获模式，在编码器 A 相上升沿捕获计数值
- **定时器时钟**: 与系统时钟相同（通常 40 MHz），分辨率为 1 timer tick = 25 ns
- **溢出处理**: 当边沿数超出硬件捕获深度时，脉冲计数溢出标志置位，RPM 返回 0.0
- **方向判断**: 读取第二个捕获值并与第一个对比，决定方向标记

### API 接口 / API Reference

```c
// bsp_encoder_mt.h — M/T 法编码器接口

void bsp_encoder_mt_init(void);
    // 初始化编码器 GPIO、Timer 时钟、捕获通道

void bsp_encoder_mt_read_get_and_clear_all(int32_t deltas[], int32_t deltas_ex[]);
    // 原子读取全部四路脉冲计数并清零 (delta_raw + delta_ex)

void bsp_encoder_get_all_rpm_mt(bsp_motor_control_t *ctx, float rpm[], int32_t raw_counts[]);
    // 计算四路 RPM (M/T 法)，返回 float RPM + 原始脉冲数
```

### M/T 读取时序 / M/T Read Timing

```
Control Task (5ms cycle):
  │
  ├── bsp_encoder_mt_read_get_and_clear_all(deltas, deltas_ex)
  │     └── 原子读取四路脉冲计数 + 边沿捕获溢出标志，立即清零
  ├── bsp_encoder_get_all_rpm_mt(ctx, rpm, raw)
  │     └── 根据 M 分支计算 RPM
  ├── PID 计算
  ├── 设置电机 PWM
  └── osal_task_delay_ms(5)
```

在 5ms 窗口结束时读取并清零，确保：
- 不丢失脉冲（读和清零是原子操作）
- 不重复计数（每个窗口独立）
- 窗口一致，低速精度较 M 法提升显著

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

## 8. 模型参数辨识 / Model Parameter Identification

### 概述 / Overview

模型参数辨识模块 (`app_model_id.c/h`) 通过阶跃响应实验，自动提取直流电机的一阶模型参数：
$$G(s) = \frac{\omega(s)}{U(s)} = \frac{K}{T s + 1}$$

其中 $K$ 为直流增益，$T$ 为机电时间常数。这些参数可用于极点配置 PID 自动整定。

### 架构位置 / Architecture Position

`app_model_id` 位于 App 层，是纯算法模块（仅依赖 `<math.h>`），与 `app_pid` 同级。它通过 `app_id_shared_t` 共享数据接口与控制任务交互。

### 阶跃响应流程 / Step Response Flow

```
菜单任务发送 "Step" 命令
  │
  ├── app_motor_stop(motor_id)                 ← 停止电机
  ├── osal_task_delay_ms(300)                  ← 等待完全停止
  │
  ├── app_id_start_step(motor_id, PWM)         ← 设置 ID 状态为 PRE_STOP
  │     └── 控制任务下一周期检测到 PRE_STOP
  │           ├── bsp_motor_stop(BRAKE)        ← 持续制动
  │           ├── 检查 RPM < 5 保持 100ms      ← 确认静止
  │           └── 状态 → STEP_RECORDING
  │
  ├── 控制任务进入 RECORDING 模式
  │     ├── bsp_motor_set_speed(motor, PWM)    ← 施加阶跃（开环）
  │     ├── rpm_buf[idx++] = 编码器RPM          ← 记录响应
  │     └── 重复 400 次 (5ms × 400 = 2秒)
  │
  ├── 缓冲区满 → done = true, 电机刹车停止
  │
  ├── app_id_process_step(rpm_buf, ...)        ← 辨识算法
  │     ├── 稳态转速 omega_ss = 最后25%均值
  │     ├── K = omega_ss / PWM
  │     ├── 搜索 0.632 × omega_ss 穿越点 + 线性插值 → T
  │     └── 计算拟合质量 NRMSE
  │
  └── 打印结果: K, T, fit_quality
```

### 极点配置 PID 整定 / Pole Placement PID Tuning

基于辨识得到的 $K$ 和 $T$，使用零极点对消法自动计算 PI 参数：

$$\tau_{cl} = \frac{1}{2\pi f_{BW}}, \quad K_p = \frac{T}{K \cdot \tau_{cl}}, \quad K_i = \frac{1}{K \cdot \tau_{cl}}$$

其中 $f_{BW}$ 为用户期望的闭环带宽，默认 5Hz，限幅范围 [1Hz, 1/(2π·0.01) ≈ 16Hz]。

### 数据接口 / Shared Data Interface

| 变量 | 写入方 | 读取方 | 说明 |
|---|---|---|---|
| `g_id_data.write_idx` | 控制任务 | 菜单任务 | 已采集样本数 |
| `g_id_data.done` | 控制任务 | 菜单任务 | 采集完成标志 |
| `g_id_data.rpm_buf[]` | 控制任务 | 菜单任务 | 转速样本 |
| `g_id_data.pwm_buf[]` | 控制任务 | 菜单任务 | PWM 样本 |
| `g_id_data.result` | 菜单任务 | — | 辨识结果 |

---

## 9. 数据流向 / Data Flow

### 控制环路数据流 / Control Loop Data Flow

```
┌──────────┐    delta    ┌──────────┐    rpm     ┌──────────┐
│ Encoder │───────────►│ M/T Method│───────────►│  PID     │
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

### IMU 数据流 / IMU Data Flow

```
                         ┌──────────────────┐
                         │   MPU6050 传感器  │
                         │  (I2C 从机地址0x68)│
                         └────────┬─────────┘
                                  I2C (软件bitbang)
                                  PA0=SCL, PA1=SDA
                                  │
                                  ▼
                         ┌──────────────────┐
                         │  bsp_mpu6050.c   │
                         │  I2C bitbang驱动 │
                         └────────┬─────────┘
                                  │
                                  ▼
                         ┌──────────────────┐
                         │   inv_mpu.c       │
                         │  (Invensense eMPL)│
                         │  mpu_dmp_get_data│
                         │  返回四元数→欧拉角│
                         └────────┬─────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    │                             │
                    ▼                             ▼
          ┌──────────────────┐        ┌────────────────────┐
          │   task_imu.c    │        │ app_complementary  │
          │   (10ms周期)    │        │    _filter.c      │
          │  写入 ctx->imu  │        │  航向角/速度融合  │
          └──────────────────┘        └────────────────────┘
                    │                             │
                    │        ┌────────────────────┘
                    │        │
                    ▼        ▼
          ┌──────────────────────────┐
          │   task_menu (VOFA输出)  │
          │  CH8=roll, CH9=pitch    │
          │  CH10=heading, CH11=vx  │
          └──────────────────────────┘
```

### MPU6050 DMP 解算 / MPU6050 DMP Processing

本项目使用 Invensense eMPL (Motion Driver Library) 驱动实现 MPU6050 DMP 解算：

- **DMP (Digital Motion Processor)**: MPU6050 内置的运动处理内核
- **四元数→欧拉角**: DMP 输出四元数，通过算法转换为 roll/pitch/yaw
- **数据来源**: `mpu_dmp_get_data(pitch, roll, yaw)` 返回 DMP 解算后的欧拉角
- **航向角来源**: DMP yaw 直接作为 `app_cf_update()` 的输入，用于 `app_cf_get_heading()`

**注意**: 航向角使用 DMP 直接解算的 yaw 角，而非通过陀螺仪积分计算，避免了积分漂移问题。

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
- 11 通道 ≈ 143 字节/帧
- 30ms 周期 → 143 / 0.03 ≈ 4767 B/s
- **占用率**: 4767 / 11520 ≈ 41% ✅ 安全

**裕量**: 剩余 59% 带宽可用于下行命令和菜单交互。

---