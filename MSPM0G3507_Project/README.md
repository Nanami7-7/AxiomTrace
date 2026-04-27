# MSPM0G3507 分层工程架构

为TI MSPM0G3507微控制器设计的五层分层嵌入式工程框架，集成FreeRTOS实时操作系统。

## 架构概览

```
┌─────────────────────────────┐
│       APP 应用层             │  业务逻辑，硬件/RTOS无关
├──────────┬──────────────────┤
│   OSAL   │      BSP        │  操作系统抽象 + 板级支持包
├──────────┴──────────────────┤
│       HAL 硬件抽象层         │  统一外设接口
├─────────────────────────────┤
│  FreeRTOS + SDK/DriverLib   │  RTOS内核 + TI官方库
└─────────────────────────────┘
```

**依赖方向**：APP → OSAL + BSP → HAL → FreeRTOS + SDK（单向依赖，禁止反向调用）

## 目录结构

```
MSPM0G3507_Project/
├── app/          # 应用层 — 业务任务、功能开关
├── osal/         # OS抽象层 — 封装FreeRTOS（任务/信号量/互斥量/队列/定时器）
├── bsp/          # 板级支持包 — 引脚映射、板卡设备驱动
├── hal/          # 硬件抽象层 — GPIO/UART/SPI/I2C/ADC/Timer/PWM/DAC
├── freertos/     # FreeRTOS内核 + 配置
├── sdk/          # TI MSPM0 SDK（DriverLib）
├── cmsis/        # CMSIS核心
├── startup/      # 启动代码和中断向量表
├── linker/       # 链接脚本
├── docs/         # 文档
├── Makefile      # 构建脚本
└── README.md     # 本文件
```

## 命名规范

| 类别 | 格式 | 示例 |
|------|------|------|
| 函数 | `层前缀_模块_操作()` | `hal_gpio_init()`, `osal_task_create()`, `bsp_led_on()` |
| 类型 | `层前缀_模块_类型_t` | `hal_gpio_config_t`, `osal_task_handle_t` |
| 宏/枚举 | `层前缀_模块_名称` | `HAL_GPIO_PIN_HIGH`, `OSAL_WAIT_FOREVER` |
| 文件 | `层前缀_模块.c/.h` | `hal_uart.c`, `osal_queue.h`, `bsp_board.c` |
| 回调 | `层前缀_模块_事件_cb_t` | `hal_uart_rx_cb_t`, `osal_timer_cb_t` |

## 标准驱动接口模板

每个外设模块统一提供：

- `xxx_init(config)` — 初始化
- `xxx_deinit()` — 反初始化
- `xxx_read()` — 读数据
- `xxx_write()` — 写数据
- `xxx_register_callback()` — 注册回调

## OSAL层接口

| OSAL模块 | 核心接口 | 说明 |
|----------|---------|------|
| osal_task | create / delete / suspend / resume | 任务管理 |
| osal_delay | delay_ms / delay_until / get_tick | 延时与时钟 |
| osal_semaphore | create / take / give / *_from_isr | 信号量 |
| osal_mutex | create / take / give / recursive | 互斥量 |
| osal_queue | create / send / receive / *_from_isr | 消息队列 |
| osal_timer | create / start / stop / change_period | 软件定时器 |
| osal_critical | enter / exit / *_from_isr | 临界区保护 |

## 快速开始

### 前置条件

- `arm-none-eabi-gcc` 工具链
- Make 构建工具
- TI MSPM0 SDK（放置到 `sdk/` 目录）

### 编译

```bash
cd MSPM0G3507_Project
make
```

### 烧录

```bash
make flash
```

### 清理

```bash
make clean
```

## 移植指南

### 更换开发板

1. 修改 `bsp/inc/bsp_pin_config.h` — 更新引脚映射
2. 修改 `bsp/inc/bsp_config.h` — 更新时钟和参数
3. 修改 `bsp/src/bsp_*.c` — 调整板级设备初始化

### 更换RTOS

1. 修改 `osal/src/osal_*.c` — 用新RTOS API替换FreeRTOS调用
2. 保持 `osal/inc/osal_*.h` 接口不变
3. APP层无需修改

### 添加新外设

1. 在 `hal/inc/` 添加 `hal_xxx.h` 头文件
2. 在 `hal/src/` 添加 `hal_xxx.c` 驱动实现
3. 在 `hal_conf.h` 添加模块使能宏
4. 在 `bsp/` 层添加对应板级封装

## 配置文件说明

| 文件 | 作用 |
|------|------|
| `hal/inc/hal_conf.h` | HAL外设模块使能开关 |
| `osal/inc/osal_conf.h` | OSAL功能开关和默认参数 |
| `freertos/FreeRTOSConfig.h` | FreeRTOS内核配置 |
| `bsp/inc/bsp_pin_config.h` | 引脚映射表 |
| `bsp/inc/bsp_config.h` | 板级时钟和参数 |
| `app/inc/app_config.h` | 应用任务参数和功能开关 |

## 许可证

MIT License
