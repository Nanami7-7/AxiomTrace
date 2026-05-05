---
name: encoder-t-method
overview: 将编码器RPM计算从M法改为T法测速，利用定时器捕获周期值计算转速，处理溢出问题
todos:
  - id: add-hal-load-raw
    content: HAL层新增hal_timer_is_load_event_raw函数(hal_timer.h/c)
    status: pending
  - id: add-config-params
    content: project_config.h新增PRJ_ENCODER_TIMER_CLK_HZ和PRJ_ENCODER_TIMER_PERIOD
    status: pending
  - id: impl-t-method-bsp
    content: "bsp_encoder.c/h实现T法测速: 新增私有变量, ISR改造, bsp_encoder_get_rpm_t函数"
    status: pending
    dependencies:
      - add-hal-load-raw
      - add-config-params
  - id: update-app-main
    content: app_main.c控制任务RPM计算改用bsp_encoder_get_rpm_t
    status: pending
    dependencies:
      - impl-t-method-bsp
---

## 用户需求

将编码器RPM计算从M法(固定周期计脉冲数)改为T法(测量两次边沿间时间)。

## 产品概述

编码器驱动模块的测速算法升级。当前M法在低速时精度差(5ms采样周期内可能0~1个脉冲)，改为T法利用定时器捕获硬件直接测量两个边沿间的时间间隔，从根本上提升低速测速精度。

## 核心功能

- T法测速: 利用CC0捕获值计算两次下降沿间的时间差，换算RPM
- 溢出处理: 记录LOAD中断次数构建扩展时间戳，处理定时器溢出
- 停转检测: 超过2个溢出周期无新边沿则RPM归零
- 保持脉冲计数功能不变(PID反馈仍用M法脉冲增量)
- 修改应用层RPM显示逻辑，使用新T法接口

## 技术栈

- 嵌入式C, MSPM0G3507 HAL(DL_Timer), FreeRTOS

## 定时器硬件参数(已验证)

- 4路TIMG定时器, 组合捕获模式(CC0脉宽+CC1周期)
- **定时器有效时钟**: 50kHz (所有4路一致)
- **LOAD_VALUE**: 65499, 溢出周期 = 65500/50000 = 1.31秒
- **CC0**: A相下降沿捕获, **CC1**: A相上升沿捕获
- 中断使能: CC0_UP + CC1_UP + LOAD

## 编码器参数

- PPR=13, 减速比=20, 双边沿倍频=2
- CC0单边沿每转脉冲数 = PPR x 减速比 = 260
- 每个CC0边沿对应输出轴角度 = 360/260 = 1.385度

## T法公式

- RPM = 60 x f_timer / (period_ticks x 260)
- RPM = 3,000,000 / (period_ticks x 260)
- 带符号: rpm = sign x (int32_t)((int64_t)3000000 / ((int64_t)period_ticks x 260))

## 溢出处理策略

构建扩展时间戳: `current_ext = s_timer_base[id] + cc0_val`

- **LOAD中断**: `s_timer_base[id] += 65500` (LOAD_VALUE+1)
- **CC0中断**: `current_ext = s_timer_base + cc0_capture_value`, `period = current_ext - last_ext`
- **竞态修复**: CC0与LOAD同时挂起时, CC0先被处理(优先级高), 此时timer_base未更新。通过`DL_Timer_getRawInterruptStatus(inst, DL_TIMER_INTERRUPT_LOAD_EVENT)`检查LOAD是否同时挂起, 若是则补加一次65500
- 停转检测: LOAD中断递增stale计数, CC0清零, stale>=2则RPM=0

## 实现方案

### 1. HAL层新增(hal_timer.h/c)

```c
bool hal_timer_is_load_event_raw(hal_timer_id_t id);
```

- 内部调用`DL_Timer_getRawInterruptStatus(inst, DL_TIMER_INTERRUPT_LOAD_EVENT)`
- 不清除中断标志, 仅读取原始状态
- 用于CC0 ISR中修复LOAD竞态

### 2. 配置层(project_config.h)

新增:

```c
#define PRJ_ENCODER_TIMER_CLK_HZ   (50000UL)
#define PRJ_ENCODER_TIMER_PERIOD   (65500U)  /* LOAD_VALUE + 1 */
```

### 3. BSP层(bsp_encoder.h/c) - 核心改动

新增私有变量:

- `s_timer_base[4]` - 扩展时间戳基准
- `s_last_cc0_ext[4]` - 上次CC0扩展时间戳
- `s_period_ticks[4]` - 最新周期(ticks)
- `s_period_valid[4]` - 至少收到2次CC0
- `s_stale_count[4]` - 停转检测计数
- `s_period_sign[4]` - 最新周期对应方向

ISR改动:

- **LOAD**: `s_timer_base[id] += PERIOD; s_stale_count[id]++`
- **CC0**: 读CC0捕获值 -> 检查LOAD竞态 -> 计算扩展时间戳 -> 算周期 -> 保留判向+计数
- **CC1**: 保留判向+计数, 不参与T法计算

新增API:

- `bsp_encoder_get_rpm_t(id)` - T法RPM, 不需要dt_ms参数

修改API:

- `bsp_encoder_get_rpm(id, dt_ms)` - 内部改调T法(dt_ms废弃)

保留不变:

- 所有脉冲计数API (get_count, get_and_clear等)
- `bsp_encoder_rpm_to_pulse()` - PID设定值转换仍用M法

### 4. 应用层(app_main.c)

替换控制任务中RPM计算:

```c
// 旧: rpm_local[i] = bsp_encoder_counts_to_rpm(delta_pulse[i], APP_CONTROL_PERIOD_MS);
// 新: rpm_local[i] = bsp_encoder_get_rpm_t((bsp_encoder_id_t)i);
```

PID反馈仍用`delta_pulse[i]`, 不受影响。

## 目录结构

```
MSPM0G3507_FreeRTOS/
├── HAL/
│   ├── hal_timer.h   # [MODIFY] 新增hal_timer_is_load_event_raw声明
│   └── hal_timer.c   # [MODIFY] 新增hal_timer_is_load_event_raw实现
├── BSP/
│   ├── bsp_encoder.h # [MODIFY] 新增bsp_encoder_get_rpm_t声明, 更新文档
│   └── bsp_encoder.c # [MODIFY] ISR增加T法逻辑, 新增RPM计算函数, 新增私有变量
├── Config/
│   └── project_config.h # [MODIFY] 新增PRJ_ENCODER_TIMER_CLK_HZ/TIMER_PERIOD
└── App/
    └── app_main.c    # [MODIFY] 控制任务RPM计算改用bsp_encoder_get_rpm_t
```

## 实现注意事项

- ISR中`hal_timer_is_load_event_raw`调用是安全的: 它只读RIS寄存器, 不修改任何状态
- 扩展时间戳用uint32_t, 溢出周期65500 ticks, 32位可容纳约24小时数据, 足够
- `period = current_ext - last_ext`利用无符号减法自动处理s_timer_base回绕
- s_stale_count阈值=2对应约2.62秒无脉冲, 等效转速<0.5RPM
- 所有新增volatile变量均遵循ISR写/任务读的单生产者模式