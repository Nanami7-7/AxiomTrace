---
name: code-review-fix-v2
overview: 基于独立代码审查，修复7项问题：ADC轮询逻辑bug、UART发送超时、编码器omega死数据、裸机临界区退出缺陷、app_set_target_speed非static、app_pid.c多余include、main.c错误指示
todos:
  - id: fix-adc-polling
    content: "修复bsp_adc_read_raw致命轮询bug: 新增hal_adc_is_busy + 重写轮询逻辑"
    status: completed
  - id: fix-uart-timeout
    content: hal_uart_transmit添加超时+transmit_buf检查返回值(hal_uart.c/h)
    status: completed
  - id: remove-encoder-omega
    content: 删除bsp_encoder omega死数据及未使用宏(bsp_encoder.c/h)
    status: completed
  - id: fix-critical-exit
    content: 修复裸机osal_critical_exit的PRIMASK恢复(osal_critical.c)
    status: completed
  - id: fix-app-misc
    content: "app_main: 提取util_itoa+加static; app_pid: 删除多余include"
    status: completed
  - id: add-led-error
    content: main.c初始化失败添加LED快闪错误指示
    status: completed
---

## 产品概述

对MSPM0G3507 FreeRTOS工程进行独立代码审查，修复所有经确认的问题

## 核心功能

- 修复bsp_adc.c轮询逻辑致命bug：hal_adc_read_result永远返回HAL_OK导致超时机制无效
- hal_uart_transmit添加超时机制防止死循环
- hal_uart_transmit_buf检查中间发送错误
- 删除bsp_encoder中未使用的omega死数据及相关宏
- 修复裸机osal_critical_exit不恢复PRIMASK的缺陷
- app_set_target_speed加static限定作用域
- 删除app_pid.c中多余的#include math.h
- 提取app_main中内联itoa为静态工具函数
- main.c初始化失败添加LED快闪错误指示

## 技术栈

- 与现有工程一致：嵌入式C (C99), MSPM0G3507, FreeRTOS, TI MSPM0 SDK

## 实现方案

### 1. [致命] 修复bsp_adc_read_raw轮询逻辑

**问题根因**：`hal_adc_read_result()` 内部调用 `DL_ADC12_getMemResult()`，该函数仅读取结果寄存器，不检查转换状态，永远返回 `HAL_OK`。导致 bsp_adc.c 的 do-while 循环在首次迭代就 break，超时机制形同虚设，读取到的是旧数据或零值。

**修复方案**：

- 在 `hal_adc.h/c` 中新增 `hal_adc_is_busy()` 函数，封装 `DL_ADC12_isConversionsInProgress()`
- 修改 `bsp_adc_read_raw()` 的轮询逻辑：先等待转换完成（用 `hal_adc_is_busy()`），再调用 `hal_adc_read_result()` 读取结果
- 保留现有超时机制，但检查条件改为 `hal_adc_is_busy()` 而非 `hal_adc_read_result()` 的返回值

### 2. [高] hal_uart_transmit添加超时

在 `hal_uart.c` 的 `hal_uart_transmit` 函数中，将无限 `while (DL_UART_isBusy)` 替换为带超时计数器的循环。超时值基于UART波特率：115200下1字节约87us，80MHz下约7000个循环周期，取 `HAL_UART_TX_TIMEOUT = 10000U`。超时返回 `HAL_ERR_TIMEOUT`。同步更新 `hal_uart.h` 文档注释和 `hal_uart_transmit_buf` 的返回值检查。

### 3. [中] hal_uart_transmit_buf检查返回值

`hal_uart_transmit_buf` 中循环调用 `hal_uart_transmit` 但不检查返回值。修复为检查每次返回值，失败时立即返回错误码。

### 4. [中] 删除bsp_encoder omega死数据

从 `bsp_encoder.c` 删除：`s_encoder_omega` 数组、`TWO_PI_F` 宏、`ENCODER_PPR`/`ENCODER_REDUCTION_RATIO`/`ENCODER_TOTAL_RESOLUTION` 宏（均未被使用）。从 `bsp_encoder.h` 删除：`bsp_encoder_get_omega()` 声明、`bsp_encoder_data_t.omega` 字段、`bsp_encoder_get_all_data()` 声明。从 `bsp_encoder.c` 删除 `bsp_encoder_get_omega()` 和 `bsp_encoder_get_all_data()` 实现，以及 `bsp_encoder_init` 中的 `s_encoder_omega` 清零。

### 5. [中] 修复裸机osal_critical_exit

在 `osal_critical.c` 裸机分支中，添加文件静态变量 `s_primask` 保存中断状态。`osal_critical_enter()` 保存 `osal_critical_enter_raw()` 返回值到 `s_primask`；`osal_critical_exit()` 调用 `osal_critical_exit_raw(s_primask)` 恢复。

### 6. [低] app_set_target_speed加static

将 `app_main.c:91` 的 `void app_set_target_speed` 改为 `static void app_set_target_speed`。

### 7. [低] 删除app_pid.c多余include

删除 `app_pid.c:13` 的 `#include <math.h>`，该文件未使用任何数学函数。

### 8. [低] 提取util_itoa工具函数

在 `app_main.c` 内部添加 `static int32_t util_itoa(int32_t val, char *buf)` 函数，返回字符串长度。替换 `app_monitor_task` 中约27行的内联转字符串代码。

### 9. [低] main.c LED错误指示

在初始化失败的死循环中，直接操作GPIO寄存器（不通过HAL，此时LED已由SYSCFG_DL_init配置），以快闪方式指示错误码。

## 实现注意事项

- 所有修改遵循现有编码规范：函数<=80行、行宽<=80列、中文注释、s_前缀文件静态
- bsp_adc的修复是关键：必须先添加hal_adc_is_busy，再修改轮询逻辑
- hal_adc_is_busy命名与hal_uart_is_busy保持一致
- bsp_encoder的omega删除不影响任何调用者（当前无代码调用omega相关接口）
- hal_uart超时值需定义为宏 `HAL_UART_TX_TIMEOUT`，便于配置
- 裸机临界区修复仅影响 `!USE_FREERTOS` 分支，FreeRTOS路径不受影响
- main.c中直接操作GPIO寄存器（DL_GPIO_togglePins），因为此时BSP可能未完全初始化但SYSCFG_DL_init已完成GPIO配置

## 目录结构

```
MSPM0G3507_FreeRTOS/
├── HAL/
│   ├── hal_adc.c         # [MODIFY] 新增hal_adc_is_busy函数
│   ├── hal_adc.h         # [MODIFY] 新增hal_adc_is_busy声明
│   ├── hal_uart.c        # [MODIFY] transmit添加超时+transmit_buf检查返回值
│   └── hal_uart.h        # [MODIFY] 文档增加HAL_ERR_TIMEOUT说明
├── BSP/
│   ├── bsp_adc.c         # [MODIFY] 修复read_raw轮询逻辑(致命bug)
│   ├── bsp_encoder.c     # [MODIFY] 删除omega/未使用宏/相关函数
│   └── bsp_encoder.h     # [MODIFY] 删除omega字段/接口/bsp_encoder_data_t简化
├── App/
│   ├── app_main.c        # [MODIFY] 提取util_itoa+app_set_target_speed加static
│   └── app_pid.c         # [MODIFY] 删除多余#include math.h
├── OSAL/
│   └── osal_critical.c   # [MODIFY] 裸机分支保存/恢复primask
└── main.c                # [MODIFY] 初始化失败LED快闪指示
```