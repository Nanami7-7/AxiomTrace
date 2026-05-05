---
name: code-review-fix
overview: 基于代码审查报告，修复5项经验证确认的问题：UART发送超时、编码器死数据、手动itoa提取、裸机临界区退出缺陷、main.c错误指示
todos:
  - id: fix-uart-timeout
    content: hal_uart_transmit添加超时机制(hal_uart.c + hal_uart.h)
    status: pending
  - id: remove-encoder-omega
    content: 删除bsp_encoder omega死数据及相关接口(bsp_encoder.c/h)
    status: pending
  - id: extract-util-itoa
    content: 提取util_itoa工具函数+app_set_target_speed加static(app_main.c)
    status: pending
  - id: fix-critical-exit
    content: 修复裸机osal_critical_exit的PRIMASK恢复(osal_critical.c)
    status: pending
  - id: add-led-error
    content: main.c初始化失败添加LED快闪错误指示
    status: pending
---

## 产品概述

根据代码审查报告，修复MSPM0G3507 FreeRTOS工程中经确认的6项问题

## 核心功能

- hal_uart_transmit添加超时机制，防止硬件故障死循环
- 删除bsp_encoder中未使用的omega死数据及相关接口
- 提取app_main中冗长的手动itoa为静态工具函数
- 修复裸机模式osal_critical_exit的PRIMASK恢复缺陷
- main.c初始化失败添加LED快闪错误指示
- app_set_target_speed加static限定作用域

## 技术栈

- 与现有工程一致：嵌入式C (C99), MSPM0G3507, FreeRTOS, TI MSPM0 SDK

## 实现方案

### 1. hal_uart_transmit 添加超时

在 `hal_uart.c` 的 `hal_uart_transmit` 函数中，将无限 `while (DL_UART_isBusy)` 替换为带超时计数器的循环。超时值基于UART波特率：115200下1字节约87us，80MHz下约7000个循环周期，取 `HAL_UART_TX_TIMEOUT = 10000U`（约125us余量）。超时返回 `HAL_ERR_TIMEOUT`。同步更新 `hal_uart.h` 的文档注释。

### 2. 删除 bsp_encoder omega 死数据

从 `bsp_encoder.c` 删除：`s_encoder_omega` 数组、`TWO_PI_F` 宏、`ENCODER_PPR`/`ENCODER_REDUCTION_RATIO`/`ENCODER_TOTAL_RESOLUTION` 宏（均未被使用）。从 `bsp_encoder.h` 删除：`bsp_encoder_get_omega()` 声明、`bsp_encoder_data_t.omega` 字段、`bsp_encoder_get_all_data()` 声明。从 `bsp_encoder.c` 删除 `bsp_encoder_get_omega()` 和 `bsp_encoder_get_all_data()` 实现，以及 `bsp_encoder_init` 中的 `s_encoder_omega` 清零。

### 3. 提取 util_itoa 工具函数

在 `app_main.c` 内部添加 `static int32_t util_itoa(int32_t val, char *buf)` 函数，返回字符串长度。替换 `app_monitor_task` 中约27行的内联转字符串代码为3行调用。

### 4. 修复裸机 osal_critical_exit

在 `osal_critical.c` 的裸机分支中，添加文件静态变量 `s_primask` 保存中断状态。`osal_critical_enter()` 改为保存 `osal_critical_enter_raw()` 返回值到 `s_primask`；`osal_critical_exit()` 改为调用 `osal_critical_exit_raw(s_primask)` 恢复。

### 5. main.c LED错误指示

在初始化失败的死循环中，利用 `DL_GPIO_readPins`/`DL_GPIO_togglePins` 操作LED引脚（PA14），以快闪方式指示错误码：闪烁|err|次后暂停，循环。

### 6. app_set_target_speed 加 static

将 `app_main.c:91` 的 `void app_set_target_speed` 改为 `static void app_set_target_speed`，该函数在 `app_main.h` 中无声明，仅内部使用。

## 实现注意事项

- 所有修改遵循现有编码规范：函数≤80行、行宽≤80列、中文注释、s_前缀文件静态
- 修改后按safety-checklist执行12阶段审查
- bsp_encoder的omega删除不影响任何调用者（当前无代码调用omega相关接口）
- hal_uart超时值需定义为宏 `HAL_UART_TX_TIMEOUT`，便于配置
- 裸机临界区修复仅影响 `!USE_FREERTOS` 分支，FreeRTOS路径不受影响
- main.c中直接操作GPIO寄存器（不通过HAL），因为此时HAL可能未初始化

## 目录结构

```
MSPM0G3507_FreeRTOS/
├── HAL/
│   ├── hal_uart.c       # [MODIFY] transmit添加超时计数器
│   └── hal_uart.h       # [MODIFY] 文档增加HAL_ERR_TIMEOUT说明
├── BSP/
│   ├── bsp_encoder.c    # [MODIFY] 删除omega/未使用宏/相关函数
│   └── bsp_encoder.h    # [MODIFY] 删除omega字段/接口/bsp_encoder_data_t简化
├── App/
│   └── app_main.c       # [MODIFY] 提取util_itoa+app_set_target_speed加static
├── OSAL/
│   └── osal_critical.c  # [MODIFY] 裸机分支保存/恢复primask
└── main.c               # [MODIFY] 初始化失败LED快闪指示
```