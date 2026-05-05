---
name: encoder-t-method
overview: 将编码器RPM从M法改为T法测速
todos:
  - id: add-t-method-config
    content: 在project_config.h中新增PRJ_ENCODER_CLK_HZ/LOAD_TICKS/STALL_OVERFLOWS宏定义
    status: completed
  - id: refactor-bsp-encoder
    content: "重构bsp_encoder.c/h: ISR加时间戳与溢出处理, 实现T法RPM, 修改API签名"
    status: completed
    dependencies:
      - add-t-method-config
  - id: update-app-main
    content: "修改app_main.c: RPM显示改用T法bsp_encoder_get_all_rpm()"
    status: completed
    dependencies:
      - refactor-bsp-encoder
---

## 用户需求

将编码器RPM计算从M法(固定周期计脉冲数)改为T法测速。

## 产品概述

电机控制系统中的编码器速度测量模块，利用定时器捕获边沿间时间差计算转速，替代原有固定周期脉冲计数法。

## 核心功能

- T法测速：在ISR中通过`hal_timer_get_count()`记录每个编码器边沿的时间戳，结合LOAD溢出计数计算两次边沿间的真实时间差
- RPM计算：`RPM = 60 * f_timer / (period_ticks * PULSES_PER_REV)`，方向由边沿时刻的B相电平决定
- 溢出处理：LOAD中断递增溢出计数，边沿间delta计算支持任意溢出次数
- 停转判定：超过指定溢出次数无新边沿则RPM=0
- 保留脉冲计数功能（用于PID反馈），RPM显示改用T法

## 技术栈

- 嵌入式C (Cortex-M0+, MSPM0G3507, FreeRTOS)
- HAL层: hal_timer_get_count(), hal_timer_get_capture_value()
- BSP层: bsp_encoder模块重构

## 实现方案

### 核心策略

在CC0/CC1中断中用`hal_timer_get_count()`读取当前定时器计数值作为边沿时间戳，配合LOAD中断的溢出计数，计算两次边沿间的真实tick差值。每个边沿间隔对应1/520转(输出轴)，由此计算RPM。

### T法公式推导

- 编码器: PPR=13, 减速比=20, 双边沿倍频=2, 总边沿/转=520
- 每个边沿间隔 = 1/520 输出轴转角
- period_ticks = 两次边沿间定时器tick差(含溢出修正)
- RPM = 60 * f_timer / (period_ticks * 520)
- 代入 f_timer=50kHz: RPM = 3,000,000 / (period_ticks * 520)

### 溢出处理

- LOAD中断: `s_overflow_cnt[id]++`
- 边沿ISR中: `delta_ovf = current_ovf - last_ovf`
- `delta_ticks = (delta_ovf * LOAD_TICKS) + current_ticks - last_ticks`
- delta_ovf=0: `current - last`
- delta_ovf=1: `(LOAD_TICKS - last) + current`
- delta_ovf=N: `(N-1)*LOAD_TICKS + (LOAD_TICKS - last) + current`
- 统一公式: `delta = delta_ovf * LOAD_TICKS + current - last`
(因为UP计数模式下，LOAD_TICKS = LOAD_VALUE + 1)

### 停转判定

- 若 `s_overflow_cnt[id] - s_last_edge_ovf[id] > STALL_OVERFLOWS(2)` 则RPM=0
- 2个溢出 = 2.62秒无脉冲 → 对应 <0.4RPM输出轴

### 精度分析

- hal_timer_get_count()在ISR中有约10-20个CPU周期延迟(~0.25us)
- 50kHz定时器下延迟 < 0.02 tick，可忽略
- 两次边沿ISR的延迟相近，差分后误差相互抵消

### 线程安全

- ISR写: s_overflow_cnt, s_last_edge_*, s_edge_period, s_edge_period_valid
- 任务读: bsp_encoder_get_rpm() 读取上述变量
- 32位对齐访问在Cortex-M0+上原子，无需额外同步
- 停转判定存在微小竞争窗口(可接受，最差情况延迟一帧检测)

## 目录结构

```
MSPM0G3507_FreeRTOS/
├── BSP/
│   ├── bsp_encoder.c    # [MODIFY] ISR加时间戳记录+溢出处理, 新T法RPM计算, 删除M法get_rpm
│   └── bsp_encoder.h    # [MODIFY] 修改get_rpm/get_all_rpm签名(去掉dt_ms), 保留counts_to_rpm/rpm_to_pulse
├── Config/
│   └── project_config.h # [MODIFY] 新增PRJ_ENCODER_CLK_HZ, PRJ_ENCODER_LOAD_TICKS, PRJ_ENCODER_STALL_OVERFLOWS
└── App/
    └── app_main.c       # [MODIFY] RPM显示改用bsp_encoder_get_all_rpm()(T法)
```

## 关键代码结构

### bsp_encoder.c 新增私有变量

```c
/* T-method speed measurement */
static volatile uint32_t s_overflow_cnt[BSP_ENCODER_COUNT];
static volatile uint32_t s_last_edge_ticks[BSP_ENCODER_COUNT];
static volatile uint32_t s_last_edge_ovf[BSP_ENCODER_COUNT];
static volatile bool     s_last_edge_valid[BSP_ENCODER_COUNT];
static volatile uint32_t s_edge_period[BSP_ENCODER_COUNT];
static volatile int8_t   s_edge_period_sign[BSP_ENCODER_COUNT];
static volatile bool     s_edge_period_valid[BSP_ENCODER_COUNT];
```

### bsp_encoder_irq_handler 伪代码

```
CC0: 读B相判向 → 累加计数 → get_count()记录时间戳 → 计算period
CC1: 读B相判向 → 累加计数 → get_count()记录时间戳 → 计算period
LOAD: s_overflow_cnt[id]++
```

### bsp_encoder_get_rpm 伪代码

```
if !s_edge_period_valid[id] → return 0
if 停转(溢出次数超限) → return 0
RPM = (60000 * CLK_HZ) / (period * PULSES_PER_REV)
乘以方向符号
```