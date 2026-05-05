---
name: restore-control-task
overview: 恢复控制任务创建，消除竞态：pid_tuning_step_2s仅在监控任务调用，控制任务纯做PID计算+电机驱动，监控任务读s_control_status输出RPM
todos:
  - id: restore-control-task
    content: 恢复控制任务创建，修正监控任务从s_control_status读RPM
    status: pending
---

## 用户需求

恢复控制任务——电机必须运行才能进行PID阶跃调参。当前控制任务创建被注释掉，电机不转，PID调参无意义。

## 产品概述

嵌入式电机PID速度控制系统(MSPM0G3507 + FreeRTOS)。控制任务5ms周期驱动电机，监控任务500ms周期做PID阶跃+串口输出RPM+LED心跳。

## 核心问题

1. 控制任务创建被注释掉(第158-170行) → 电机不转 → PID阶跃无意义
2. 监控任务直接调`bsp_encoder_get_and_clear_all`读编码器(第236-237行) → 与控制任务抢数据，控制任务已清零，监控任务读到0或残余值 → RPM波形异常

## 修复方案

1. 恢复控制任务创建(取消注释)
2. 控制任务：读编码器 + PID计算 + 电机输出 + 写s_control_status.rpm(纯控制，无I/O阻塞)
3. 监控任务：pid_tuning_step_2s(设setpoint) + 读s_control_status.rpm输出RPM + LED翻转
4. 监控任务不再直接读编码器(控制任务每5ms读且清零，监控任务500ms读一次会拿到空数据)

## Tech Stack

嵌入式C (MSPM0G3507 + FreeRTOS)，与现有项目一致

## Implementation Approach

整体重写app_main.c，核心改动：

1. 恢复控制任务创建
2. 监控任务从s_control_status.rpm读取RPM(已有临界区保护)，不再直接读编码器
3. 控制任务保留pid_tuning_step_2s的调用——不对，pid_tuning_step_2s只应在监控任务调用，避免竞态

**数据流修正**：

- 控制任务(5ms): 读编码器 → PID计算 → 电机输出 → 写s_control_status
- 监控任务(500ms): pid_tuning_step_2s(设setpoint) → 读s_control_status → printf RPM → LED翻转
- setpoint由监控任务设置，控制任务的app_pid_compute使用该setpoint，无竞态(app_pid_set_setpoint写float是原子的在Cortex-M0+上)

**关键修正**：监控任务不能直接调bsp_encoder_get_and_clear_all，因为控制任务每5ms已经在读且清零，监控任务500ms来一次读到的增量几乎为0。应从s_control_status.rpm读取控制任务已计算好的RPM。

## Directory Structure

```
MSPM0G3507_FreeRTOS/
└── App/
    └── app_main.c  # [MODIFY] 恢复控制任务、修正监控任务RPM读取方式
```

### app_main.c 修改详情

- **第158-170行**: 取消注释，恢复控制任务创建
- **第227-250行**: 重写app_monitor_task():
- 保留pid_tuning_step_2s()
- 删除bsp_encoder_get_and_clear_all()直接读编码器
- 改为从s_control_status.rpm[]读取(临界区保护)
- printf输出RPM，格式"%ld\n"
- 保留bsp_led_toggle()