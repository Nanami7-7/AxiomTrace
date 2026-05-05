---
name: encoder-t-method-speed
overview: 将编码器RPM计算从M法(固定周期计脉冲数)改为T法(测量两次边沿间时间)，利用定时器组合捕获模式的CC1获取周期值，处理定时器溢出问题
todos:
  - id: add-t-method-config
    content: 在project_config.h中添加T法测速参数宏(TIMER_CLK_HZ/LOAD_VALUE/STALE_THRESHOLD)
    status: pending
  - id: modify-encoder-header
    content: "修改bsp_encoder.h: 移除dt_ms参数, 删除counts_to_rpm, 更新文档注释"
    status: pending
    dependencies:
      - add-t-method-config
  - id: implement-t-method
    content: "实现bsp_encoder.c的T法测速: ISR捕获+溢出处理+RPM计算+停转检测"
    status: pending
    dependencies:
      - modify-encoder-header
  - id: update-app-caller
    content: "适配app_main.c: 替换bsp_encoder_counts_to_rpm为bsp_encoder_get_rpm"
    status: pending
    dependencies:
      - implement-t-method
---

## 用户需求

当前RPM计算使用M法(固定周期内计脉冲数)，低速时精度差(delta=0或1)。需替换为T法测速：通过定时器捕获两次边沿之间的时间来计算转速。

## 产品概述

编码器T法测速驱动，利用硬件捕获功能测量相邻边沿时间间隔，实时计算RPM。

## 核心功能

- ISR中记录每次边沿的捕获值，结合溢出计数计算相邻边沿间的时间差(ticks)
- 基于T法公式计算RPM: `RPM = 60 * f_timer / (period_ticks * pulses_per_rev)`
- 处理定时器溢出：LOAD中断递增溢出计数，CC0/CC1中断中使用绝对时间戳计算差值
- 停转检测：若溢出计数远超过上次边沿时的溢出计数，判定为停转返回RPM=0
- 保留原有脉冲计数功能(用于PID反馈和位置跟踪)
- RPM接口不再需要dt_ms参数(由硬件直接测量周期)

## 技术栈

- 嵌入式C，MSPM0G3507 + FreeRTOS
- HAL层: hal_timer_get_capture_value() 获取捕获计数值
- OSAL临界区保护多变量原子读取

## 实现方案

### T法测速原理

每次编码器边沿(CCW/CC1)中断时，读取硬件捕获寄存器中的定时器计数值，结合溢出计数构造绝对时间戳，计算与上次边沿的时间差。每个边沿对应输出轴转过 360/520 度。

公式: `RPM = 60 * f_timer / (period_ticks * PULSES_PER_REV)`

- f_timer = 50000 Hz (4路定时器统一)
- PULSES_PER_REV = 520 (PPR*减速比*2)
- period_ticks = 两次相邻边沿间的定时器tick数

### 溢出处理

- LOAD中断递增 `s_overflow_count[id]`
- CC0/CC1中断中: `abs_ts = overflow_count * (LOAD_VALUE+1) + capture_value`
- `period_ticks = abs_ts - last_abs_ts`
- 所有中断同源同ISR，getPendingInterrupt按优先级(CC0>CC1>LOAD)返回，时序一致

### 停转检测

- 记录 `s_last_edge_overflow[id]` = 边沿发生时的溢出计数
- 读取RPM时: `current_ovf - last_edge_ovf > STALE_THRESHOLD(2)` → 返回0
- 阈值2对应约2.6秒无边沿，即输出轴转速低于 ~0.4 RPM

### 定时器时钟验证

- 4路LOAD_VALUE均为65499，注释均说溢出周期1.31s
- 65500/50000 = 1.31s → 确认f_timer = 50kHz
- LEFT_FRONT(TIMG8) prescale=99(可能MFCLK=40MHz), 其余prescale=199(BUSCLK=80MHz), 最终均为50kHz

## 实现注意事项

- ISR中读取捕获值必须用 `hal_timer_get_capture_value()` 而非 `hal_timer_get_count()`，前者是硬件锁存值无ISR延迟
- 临界区内仅读取4个volatile变量(period_ticks, sign, overflow_count, last_edge_overflow)，极短
- 首次边沿不计算周期(无前次时间戳)，s_period_valid标志控制
- 64位除法仅用于最终RPM计算(任务上下文)，ISR中不做除法
- `s_overflow_count` 为uint32_t，50kHz/65500≈0.76次/秒溢出，uint32可计~56亿溢出(约70年不溢出)

## 目录结构

```
MSPM0G3507_FreeRTOS/
├── Config/
│   └── project_config.h              # [MODIFY] 添加编码器T法参数宏
├── BSP/
│   ├── bsp_encoder.h                 # [MODIFY] 修改RPM接口签名，移除dt_ms参数
│   └── bsp_encoder.c                 # [MODIFY] 实现T法测速核心逻辑
└── App/
    └── app_main.c                    # [MODIFY] 适配新RPM接口
```

### 文件修改详情

**project_config.h** [MODIFY]

- 添加 `PRJ_ENCODER_TIMER_CLK_HZ (50000UL)` - 编码器捕获定时器时钟频率
- 添加 `PRJ_ENCODER_TIMER_LOAD_VALUE (65499U)` - 定时器LOAD值(溢出周期)
- 添加 `PRJ_ENCODER_STALE_OVF_THRESHOLD (2U)` - 停转检测溢出阈值

**bsp_encoder.h** [MODIFY]

- `bsp_encoder_get_rpm(id, dt_ms)` → `bsp_encoder_get_rpm(id)` 移除dt_ms
- `bsp_encoder_get_all_rpm(rpms[], dt_ms)` → `bsp_encoder_get_all_rpm(rpms[])` 移除dt_ms
- 删除 `bsp_encoder_counts_to_rpm(delta, dt_ms)` - T法不再需要
- 保留 `bsp_encoder_rpm_to_pulse(rpm, dt_ms)` - PID设定值转换仍需要
- 更新文档注释说明T法原理

**bsp_encoder.c** [MODIFY]

- 新增私有变量: s_overflow_count, s_last_timestamp, s_period_ticks, s_last_edge_overflow, s_period_valid
- 新增static inline `encoder_update_period(id, cc_val)` 辅助函数
- 修改 `bsp_encoder_irq_handler()`: CC0/CC1中记录捕获值+计算周期, LOAD中递增溢出计数
- 重写 `bsp_encoder_get_rpm()`: T法公式 + 停转检测 + 临界区保护
- 重写 `bsp_encoder_get_all_rpm()`: 移除dt_ms，调用新get_rpm
- 删除 `bsp_encoder_counts_to_rpm()` 实现
- 修改 `bsp_encoder_init()`: 初始化新增变量

**app_main.c** [MODIFY]

- 替换 `bsp_encoder_counts_to_rpm(delta_pulse[i], APP_CONTROL_PERIOD_MS)` → `bsp_encoder_get_rpm((bsp_encoder_id_t)i)`