---
name: fix-encoding-and-rpm-waveform
overview: 修复app_main.c中文乱码注释 + 排查RPM串口输出波形异常(监控任务误调pid_tuning_step_2s导致竞态)
todos:
  - id: fix-garbled-comments
    content: 修复app_main.c中4处中文乱码注释
    status: completed
  - id: fix-race-condition
    content: 删除监控任务中误调的pid_tuning_step_2s，消除竞态条件
    status: completed
    dependencies:
      - fix-garbled-comments
  - id: move-rpm-output
    content: 将RPM串口输出从控制任务移至监控任务，修复printf格式，恢复LED心跳
    status: completed
    dependencies:
      - fix-race-condition
---

## 用户需求

1. 修复 app_main.c 中的中文乱码注释（4处）
2. 排查并修复串口输出RPM值波形异常的原因

## 产品概述

嵌入式电机PID速度控制系统，基于MSPM0G3507 + FreeRTOS，4路编码器读取+4路电机PID闭环控制，串口输出RPM用于调试观测。

## 核心问题

### 问题1：中文注释乱码

app_main.c 中4处注释因编码问题显示为乱码，需恢复为正确中文。

### 问题2：RPM波形异常（3个根因）

**根因A（致命）— 竞态条件：**

- `pid_tuning_step_2s()` 使用 static 局部变量（`elapsed_ms`, `rpm_idx`, `initialized`）
- 同时被 `app_control_task()`（5ms周期）和 `app_monitor_task()`（500ms周期）调用
- 两个任务并发修改 static 变量 → setpoint 随机跳变 → RPM波形不规则
- 监控任务不应调用 PID 调阶函数

**根因B（中等）— printf格式问题：**

- 第225行 `printf(" %ld\r\n", ...)` 前导空格导致串口绘图器解析异常
- `\r\n` 应改为 `\n` 适配串口绘图器

**根因C（低）— 控制循环中阻塞printf：**

- fputc 实现为忙等（`while(DL_UART_isBusy())`），115200波特率每字节约87μs
- 5ms控制循环中打印5-10字节约0.5-1ms，占周期10-20%
- RPM输出应移至监控任务（500ms周期），降低对控制任务影响

## 技术栈

- 嵌入式C（MSPM0G3507 + FreeRTOS）
- 编码器捕获（TIMG定时器组合捕获模式）
- 串口调试输出（UART0, 115200bps, 阻塞发送）

## 实现方案

### 修复1：中文乱码注释

直接替换4处乱码为正确中文，文件编码保持不变。

### 修复2：竞态条件（致命）

删除 `app_monitor_task()` 中误调的 `pid_tuning_step_2s()`，恢复监控任务原有职责（LED心跳翻转 + RPM串口输出）。

### 修复3：printf格式

去掉前导空格，`\r\n` → `\n`，适配串口绘图器格式要求。

### 修复4：RPM输出移至监控任务

- 控制任务：仅写入 `s_control_status.rpm[]`（已有关键区保护）
- 监控任务：读取 `s_control_status.rpm[]` 并 printf 输出，同时完成 LED 心跳
- 控制任务栈可从256字降至128字（去掉printf后栈压力大幅降低）

## 实现注意事项

- `s_control_status` 的读写已有 `OSAL_CRITICAL_SECTION` 保护，无需额外同步
- 监控任务优先级(2)低于控制任务(5)，不会抢占控制任务
- printf 在监控任务中500ms周期调用，阻塞时间占比可忽略
- 保持控制任务5ms周期的纯控制逻辑，不掺杂任何I/O阻塞

## 目录结构

```
MSPM0G3507_FreeRTOS/
└── App/
    └── app_main.c  # [MODIFY] 修复乱码注释、修复竞态、调整RPM输出位置
```

### app_main.c 修改详情

- **第117行**: 乱码注释 → `初始化四路PID控制器`
- **第132行**: 乱码注释 → `速度环: 积分限幅为输出限幅的50%`
- **第152行**: 乱码注释 → `串口输出启动信息`
- **第211行**: 乱码注释 → `PID计算 + 电机输出(4路)`
- **第225行**: 删除控制任务中的 `printf(" %ld\r\n", ...)` 
- **第231-239行**: 重写 `app_monitor_task()`：删除 `pid_tuning_step_2s()` 调用，添加LED心跳翻转 + RPM串口输出（从 `s_control_status.rpm[]` 读取）