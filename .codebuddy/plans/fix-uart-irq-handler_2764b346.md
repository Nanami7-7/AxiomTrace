---
name: fix-uart-irq-handler
overview: 修复UART接收中断处理函数缺失问题：在board.c中实现UART0_IRQHandler，调用bsp_uart_irq_handler()，使串口接收数据能正确进入环形缓冲区
todos:
  - id: fix-uart-irq
    content: 在board.c中实现UART_0_DEBUG_INST_IRQHandler并清理废弃代码
    status: completed
---

## 产品概述

修复MSPM0G3507 FreeRTOS项目中UART串口接收功能：用户通过串口输入1后系统无反馈

## 核心功能

- 实现缺失的UART0中断服务函数，使接收数据能进入应用层
- 清理冗余/废弃的旧代码（注释掉的ISR、未使用的debug_uart_init）
- 确保ISR符合极简原则：仅读数据+存入环形缓冲区

## 技术栈

- 嵌入式C (MSPM0G3507 + FreeRTOS + TI MSPM0 SDK)
- 已有分层架构：App -> BSP -> HAL -> 寄存器

## 实现方案

### 问题根因

`UART0_IRQHandler` 未实现。启动文件中该符号为WEAK，默认实现是死循环(`B .`)。串口收到字节触发中断后系统卡死，`bsp_uart_irq_handler()` 从未被调用，环形缓冲区永远为空。

### 修复策略

在 `board.c` 中实现 `UART_0_DEBUG_INST_IRQHandler`（宏展开为 `UART0_IRQHandler`），函数体仅调用 `bsp_uart_irq_handler()`。同时清理废弃代码和冗余函数。

### 中断安全性

- `bsp_uart_irq_handler()` 内部：读中断标志 + 读数据寄存器 + 写环形缓冲区
- 环形缓冲区为单生产者(ISR)单消费者(任务)模式，无需额外同步
- ISR中无阻塞调用、无内存分配、无互斥锁，符合规范

### 影响分析

- 调用者：启动文件向量表跳转，正常
- 被调用者：`bsp_uart_irq_handler()` 参数为空，无参数校验风险
- 结构体布局：无变更
- 头文件兼容：`bsp_uart.h` 已存在且导出该函数声明
- 初始化顺序：`bsp_uart_init()` 在 `bsp_modules_init()` 中已调用 `hal_uart_enable_irq()` 使能NVIC，无需 `debug_uart_init()`
- 清理序列：无影响

## 目录结构

```
MSPM0G3507_FreeRTOS/
├── board.c  # [MODIFY] 修复UART0中断处理函数
│   - 添加 #include "bsp_uart.h"
│   - 实现 UART_0_DEBUG_INST_IRQHandler() 调用 bsp_uart_irq_handler()
│   - 删除被注释掉的旧 ISR 代码(第86-98行)
│   - 删除冗余的 debug_uart_init() 函数(第75-79行)
│   - 删除 board.c 中重复的 fputc 实现(board.c第116-123行与UART0_Debug.c重复，
│     但UART0_Debug.c不在工程中，board.c的fputc是当前生效的，保留不动)
```