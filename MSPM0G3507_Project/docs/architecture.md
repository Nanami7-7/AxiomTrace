# MSPM0G3507 工程架构设计文档

## 1. 架构总览

本工程采用五层分层架构，确保硬件依赖隔离、RTOS可替换和模块化开发：

```
┌──────────────────────────────────────────────────────┐
│                    APP 应用层                         │
│   app_main.c / app_task.c — 业务逻辑，硬件/RTOS无关   │
├──────────────────────┬───────────────────────────────┤
│     OSAL 操作系统抽象  │       BSP 板级支持包          │
│  封装FreeRTOS接口      │  引脚映射、板级设备驱动        │
│  (任务/信号量/队列等)   │  (LED/按键/串口/SPI/I2C/ADC) │
├──────────────────────┴───────────────────────────────┤
│                  HAL 硬件抽象层                        │
│   统一外设接口 (GPIO/UART/SPI/I2C/ADC/Timer/PWM/DAC) │
├──────────────────────────────────────────────────────┤
│           FreeRTOS + SDK/DriverLib + CMSIS            │
│        RTOS内核 + TI官方驱动库 + 处理器支持            │
└──────────────────────────────────────────────────────┘
```

## 2. 层间依赖规则

- **单向依赖**：上层只能调用下层接口，禁止反向调用
- **头文件隔离**：每层通过头文件暴露接口，实现细节对上层隐藏
- **APP层**：只依赖 `osal_*.h` 和 `bsp_*.h`，不直接调用 `hal_*.h`
- **BSP层**：只依赖 `hal_*.h`，不直接操作寄存器或SDK
- **HAL层**：只依赖 `dl_*.h`（TI SDK），封装寄存器操作
- **OSAL层**：只封装FreeRTOS API，不依赖硬件

## 3. 数据流

### 中断数据流（以UART接收为例）

```
[硬件中断] → UART0_IRQHandler() (hal_uart.c)
         → s_uart_callbacks[0].rx_cb() (BSP或APP注册的回调)
         → osal_queue_send_from_isr() (OSAL ISR安全接口)
         → [任务上下文] osal_queue_receive() (APP任务)
         → 业务处理
```

### 任务间通信

```
[Task A] → osal_queue_send() ────→ [Queue] ────→ osal_queue_receive() → [Task B]
[Task A] → osal_semaphore_give() → [Semaphore] → osal_semaphore_take() → [Task B]
```

## 4. 启动流程

```
Reset_Handler()           // startup_mspm0g3507.c
  ├── Copy .data          // Flash → SRAM
  ├── Zero .bss
  ├── SystemInit()        // cmsis: 配置时钟
  └── main()              // app_main.c
       ├── bsp_board_init()    // BSP: 初始化所有硬件
       │    ├── bsp_gpio_init()
       │    ├── bsp_uart_init()
       │    ├── bsp_spi_init()
       │    ├── bsp_i2c_init()
       │    └── bsp_adc_init()
       ├── app_task_create_all()  // APP: 创建FreeRTOS任务
       └── vTaskStartScheduler()  // 启动调度器，永不返回
```

## 5. 配置层次

```
app_config.h          ← 应用层：任务参数、功能开关
  ↓
osal_conf.h           ← OSAL层：RTOS模块开关
FreeRTOSConfig.h      ← FreeRTOS：内核参数
  ↓
bsp_pin_config.h      ← BSP层：引脚映射
bsp_config.h          ← BSP层：时钟、默认参数
  ↓
hal_conf.h            ← HAL层：外设模块开关
```

## 6. 中断优先级规划

MSPM0G3507有2位NVIC优先级（4级），FreeRTOS需要管理优先级划分：

| 优先级等级 | 数值 | 用途 |
|-----------|------|------|
| Highest | 0 | 看门狗、硬实时外设 |
| High | 1 | 通信中断(UART/SPI/I2C) |
| Medium | 2 | GPIO、ADC、Timer |
| Lowest | 3 | FreeRTOS可屏蔽的最低优先级 |

**注意**：优先级数值 ≤ `configMAX_SYSCALL_INTERRUPT_PRIORITY` 的中断可以调用FreeRTOS API。

## 7. 内存规划

MSPM0G3507有32KB SRAM，分配如下：

| 区域 | 大小 | 说明 |
|------|------|------|
| .data + .bss | ~4KB | 全局变量 |
| FreeRTOS堆 | 16KB | 任务栈、队列、信号量等 |
| 主栈(MSP) | 1KB | 中断和调度器启动前使用 |
| 任务栈 | ~8KB | 3个任务 × ~2.5KB |
| 剩余 | ~3KB | 预留 |

## 8. 添加新外设驱动步骤

1. **HAL层**：创建 `hal/inc/hal_xxx.h` 和 `hal/src/hal_xxx.c`
2. **使能配置**：在 `hal/inc/hal_conf.h` 添加 `#define HAL_XXX_MODULE_ENABLED`
3. **BSP层**：创建 `bsp/inc/bsp_xxx.h` 和 `bsp/src/bsp_xxx.c`
4. **板级配置**：在 `bsp/inc/bsp_pin_config.h` 添加引脚定义
5. **初始化**：在 `bsp/src/bsp_board.c` 的 `bsp_board_init()` 中调用
6. **应用**：在 `app/` 中通过BSP接口使用
7. **编译**：在 `Makefile` 中添加新的源文件路径
