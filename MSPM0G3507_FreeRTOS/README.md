# MSPM0G3507 FreeRTOS 4-Motor PID Speed Control

基于 TI MSPM0G3507 LaunchPad 的 FreeRTOS 四路直流电机 PID 闭环速度控制系统，支持 VOFA+ 远程调参和 AxiomTrace 调试日志。

## 系统架构

```
┌─────────────────────────────────────────────┐
│  App Layer                                  │
│  ├── app_main.c/h    应用入口 + 公共函数    │
│  ├── app_pid.c/h     增量式 PID 控制器      │
│  ├── app_vofa.c/h    VOFA+ 协议(收发)       │
│  └── Task/                                   │
│      ├── task_control.c/h  控制任务(5ms)     │
│      └── task_menu.c/h     菜单任务(串口)    │
├─────────────────────────────────────────────┤
│  BSP Layer          Board Support Package    │
│  ├── bsp_motor.c/h   PWM 电机驱动           │
│  ├── bsp_encoder.c/h 编码器 + M法测速        │
│  ├── bsp_uart.c/h    串口(中断+环形缓冲)    │
│  ├── bsp_led.c/h     LED 心跳               │
│  ├── bsp_adc.c/h     ADC 电压采集           │
│  ├── bsp_soft_i2c.c/h 软件 I2C              │
│  └── bsp_common.h    环形缓冲/宏/状态码     │
├─────────────────────────────────────────────┤
│  HAL Layer          Hardware Abstraction     │
│  ├── hal_uart.c/h    UART 寄存器抽象        │
│  ├── hal_gpio.c/h    GPIO 寄存器抽象        │
│  ├── hal_timer.c/h   Timer 寄存器抽象       │
│  ├── hal_adc.c/h     ADC 寄存器抽象         │
│  └── hal_common.h    通用类型定义           │
├─────────────────────────────────────────────┤
│  OSAL Layer         OS Abstraction           │
│  ├── osal_task.c     FreeRTOS 任务封装       │
│  ├── osal_critical.c 临界区封装              │
│  ├── osal_delay.c    延时封装                │
│  ├── osal_mutex.c    互斥锁封装              │
│  ├── osal_sem.c      信号量封装              │
│  └── osal_queue.c    消息队列封装            │
├─────────────────────────────────────────────┤
│  lib/               第三方库                 │
│  └── axiomtrace/     AxiomTrace 调试日志库   │
└─────────────────────────────────────────────┘
```

## 目录结构

```
MSPM0G3507_FreeRTOS/
├── main.c                 FreeRTOS 入口 (创建 app_main_init)
├── board.c/h              Board 初始化
├── ti_msp_dl_config.c/h   SysConfig 生成的外设配置
├── FreeRTOSConfig.h       FreeRTOS 内核配置
├── empty.syscfg           SysConfig 工程文件
├── App/                   应用层
│   ├── app_main.c/h       应用初始化 + 公共电机控制函数
│   ├── app_pid.c/h        增量式 PID 控制器
│   ├── app_vofa.c/h       VOFA+ 协议实现
│   └── Task/
│       ├── task_control.c/h  PID 闭环控制任务
│       └── task_menu.c/h     串口交互菜单任务
├── BSP/                   板级支持包
├── Config/                工程配置 (project_config.h)
├── HAL/                   硬件抽象层
├── OSAL/                  操作系统抽象层
├── port/                  AxiomTrace 移植层
├── lib/                   第三方库
└── keil/                  Keil MDK 工程文件
```

## 功能特性

### PID 速度控制
- **算法**: 增量式 PID（抗积分饱和）
- **控制周期**: 5ms
- **测速方式**: M 法编码器测速
- **默认参数**: Kp=0.8, Ki=0.3, Kd=0.0
- **输出限幅**: ±PWM 周期值

### VOFA+ 远程调参
- **上行数据**: FireWater 文本格式，12 通道，30ms 周期
- **下行命令**: 支持远程修改 PID 参数、启停电机
- **安全防护**: NaN/Inf 拒绝、参数范围限幅

### AxiomTrace 调试日志
- **级别**: INFO / WARN / ERROR
- **输出**: 通过 bsp_uart_putc 发送
- **开关**: 可通过 axiom_config.h 宏控制

## VOFA+ 协议说明

### 上行数据格式 (FireWater)

```
val0,val1,val2,val3,val4,val5,val6,val7,val8,val9,val10,val11\n
```

- 逗号分隔浮点数，`\n` 结尾
- 精度：`%.6f`
- 输出周期：30ms

**12 通道分配：**

| 通道 | 内容 |
|------|------|
| CH0~CH3 | 电机 A/B/C/D 实际 RPM |
| CH4~CH7 | 电机 A/B/C/D 目标 RPM |
| CH8~CH11 | 电机 A/B/C/D PID 输出 |

### 下行命令

| 命令格式 | 说明 | 示例 |
|----------|------|------|
| `Kp=x` | 设置比例增益 | `Kp=1.5` |
| `Ki=x` | 设置积分增益 | `Ki=0.3` |
| `Kd=x` | 设置微分增益 | `Kd=0.1` |
| `Target=x` | 设置目标 RPM | `Target=200` |
| `Motor=x` | 选择电机 (0-3) | `Motor=0` |
| `Run` | 启动当前电机 | `Run` |
| `Run=x` | 启动指定电机 | `Run=1` |
| `Stop` | 停止当前电机 | `Stop` |
| `Stop=x` | 停止指定电机 | `Stop=1` |
| `StopAll` | 停止所有电机 | `StopAll` |

**参数安全限制：**
- Kp/Ki/Kd: ±100，拒绝 NaN/Inf
- Target RPM: ±500，拒绝 NaN/Inf

## 串口菜单操作

连接串口（115200 波特率）后，按 `Enter` 显示主菜单：

```
=== Motor: A ===
A: KP=0.80 KI=0.30 KD=0.00
B: KP=0.80 KI=0.30 KD=0.00
C: KP=0.80 KI=0.30 KD=0.00
D: KP=0.80 KI=0.30 KD=0.00
a:Motor b:RPM c:PID d:Run q:Back
```

| 按键 | 功能 |
|------|------|
| `a` | 选择电机 (输入 a-d) |
| `b` | 设置目标 RPM |
| `c` | 设置 PID 参数 (输入 `Kp Ki Kd`) |
| `d` | 启动电机，进入 VOFA+ 运行模式 |
| `q` | 停止当前电机 / 返回上级菜单 |

**运行模式下：**
- 自动以 30ms 周期输出 12 通道 VOFA+ 数据
- 支持下行命令远程调参
- 输入 `q` 停止电机并返回主菜单

## 硬件连接

**4 路电机 PWM 输出** — 通过 SysConfig 配置，详见 `project_config.h`

**4 路编码器输入** — Timer Capture 模式，正交解码

**UART 串口** — 115200 波特率，用于菜单交互 + VOFA+ 通信

## 构建说明

1. 使用 Keil MDK 打开 `keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`
2. 编译工程
3. 下载到 LaunchPad
4. 打开串口终端（115200 波特率）

## 任务配置

| 任务 | 优先级 | 栈大小 | 周期 |
|------|--------|--------|------|
| 控制任务 (control) | 5 (最高) | 256 字 | 5ms |
| 菜单任务 (menu) | 2 (低) | 512 字 | 100ms (菜单) / 30ms (运行) |