# MSPM0G3507_FreeRTOS 架构目录图

> 本文档描述固件工程的目录结构与模块职责，帮助新开发者快速理解架构。

## 目录结构

```
MSPM0G3507_FreeRTOS/
├── main.c                          # 系统入口
│
├── Application/                    # 应用层
│   ├── app_main.c/h                # 应用初始化与共享上下文
│   ├── app_vofa.c/h                # 串口协议编解码
│   ├── app_debug.c/h               # 诊断与调试
│   ├── Algorithm/                  # 控制算法
│   │   ├── app_pid.c/h             # PID 控制器
│   │   ├── app_feedforward.c/h     # 前馈控制
│   │   ├── app_planner.c/h         # 轨迹规划
│   │   ├── app_position_control.c/h # 位置-速度串级控制
│   │   ├── app_model_id.c/h        # 模型参数辨识
│   │   └── app_complementary_filter.c/h # 互补滤波
│   ├── Filter/                     # 滤波算法
│   │   ├── filter.h                # 滤波器统一接口
│   │   ├── filter_config.c/h        # 滤波器配置
│   │   ├── filter_core.c           # 滤波器核心逻辑
│   │   ├── filter_internal.h       # 内部接口
│   │   ├── filter_complementary.c  # 互补滤波实现
│   │   ├── filter_madgwick.c       # Madgwick 算法
│   │   ├── filter_mahony.c         # Mahony 算法
│   │   ├── filter_ekf.c            # 扩展卡尔曼滤波
│   │   ├── filter_kf.c             # 卡尔曼滤波
│   │   ├── filter_lkf.c            # 线性卡尔曼滤波
│   │   └── filter_lpf.c            # 低通滤波
│   ├── Tasks/                      # FreeRTOS 任务
│   │   ├── task_control.c/h        # 控制任务 (5ms)
│   │   ├── task_imu.c/h            # IMU 任务
│   │   └── task_menu.c/h           # 菜单任务
│   └── Tests/                      # 测试代码
│       ├── app_test_runner.c/h     # 测试运行器
│       └── app_test_timer.c/h      # 测试计时器
│
├── BSP/                            # 板级支持包
│   ├── Common/                     # 公共定义
│   │   ├── bsp_common.h            # BSP 错误码、类型、环形缓冲区
│   │   └── bsp_mathacl.h           # 数学加速器接口
│   ├── Platform/                   # 平台抽象
│   │   ├── platform.c/h            # 平台抽象接口（延时、计时、调试输出）
│   │   └── log.h                   # 日志宏
│   ├── HAL/                         # 硬件抽象层
│   │   ├── hal_common.h            # HAL 公共类型与错误码
│   │   ├── hal_adc.c/h             # ADC 抽象
│   │   ├── hal_gpio.c/h            # GPIO 抽象
│   │   ├── hal_timer.c/h           # Timer/PWM 抽象
│   │   └── hal_uart.c/h            # UART 抽象
│   ├── Peripherals/                # 外设板级驱动
│   │   ├── bsp_adc.c/h             # ADC 板级驱动
│   │   ├── bsp_encoder.c/h         # 编码器驱动
│   │   ├── bsp_led.c/h             # LED 驱动
│   │   ├── bsp_timer.c/h           # 定时器驱动
│   │   └── bsp_uart.c/h            # UART 板级驱动
│   ├── Motor/                      # 电机驱动子系统
│   │   ├── bsp_motor.c/h           # 统一电机门面
│   │   ├── bsp_drv8870.c/h         # DRV8870 后端（默认）
│   │   └── bsp_tb6612.c/h          # TB6612 后端（备用）
│   └── IMU/                        # IMU 驱动
│       ├── bsp_lsm6dsr.c/h         # LSM6DSR 板级驱动
│       ├── lsm6dsr.c/h             # LSM6DSR 设备驱动
│       └── Ports/mspm0g3507/       # MSPM0 平台移植
│           ├── platform_mspm0.c    # 平台实现
│           └── spi_bridge.c/h      # SPI 桥接
│
├── Config/                         # 配置层（含 TI 自动生成代码）
│   ├── project_config.h            # 项目硬件配置集中定义
│   ├── project_version.h           # 版本信息
│   ├── board.c/h                   # 板级初始化（printf 重定向等）
│   ├── ti_msp_dl_config.c/h        # SysConfig 自动生成（禁止手改）
│   ├── empty.syscfg                # SysConfig 源文件
│   ├── FreeRTOSConfig.h            # FreeRTOS 配置
│   └── startup_mspm0g350x_uvision.s # 启动文件
│
├── Lib/                            # 第三方库
│   ├── OSAL/                       # 操作系统抽象层
│   ├── AxiomTrace/                 # 结构化日志系统
│   ├── FreeRTOS/                   # FreeRTOS 内核
│   └── Math/                       # 数学库
│
├── Docs/                           # 文档
└── keil/                           # Keil 工程文件
```

## 模块依赖关系

```
Application
    │
    ├── Algorithm/     (控制算法，无硬件依赖)
    ├── Filter/        (滤波算法，无硬件依赖)
    ├── Tasks/         (FreeRTOS 任务，依赖 BSP 和 OSAL)
    └── Tests/         (测试代码)
        │
        ▼
BSP
    ├── Common/        (公共定义)
    ├── Platform/      (平台抽象)
    ├── HAL/           (硬件抽象层，封装 DriverLib)
    ├── Peripherals/   (外设驱动，依赖 HAL)
    ├── Motor/         (电机驱动，依赖 HAL)
    └── IMU/           (IMU 驱动，依赖 HAL)
        │
        ▼
Config (SysConfig 生成 + 项目配置)
        │
        ▼
Lib (FreeRTOS + OSAL + AxiomTrace + Math)
```

## 添加新模块指南

1. 确定模块所属层级：
   - 硬件相关驱动 → `BSP/Peripherals/` 或 `BSP/Motor/` 或 `BSP/IMU/`
   - 硬件抽象 → `BSP/HAL/`
   - 控制算法 → `Application/Algorithm/`
   - 滤波算法 → `Application/Filter/`
   - FreeRTOS 任务 → `Application/Tasks/`
2. 在对应目录下创建源文件和头文件
3. 更新 Keil 工程 (`.uvprojx`) 的 Group 和 Include Path
4. 更新 `.clangd` 配置（如使用 clangd）
5. 更新本文件的目录结构

## 命名规范

| 前缀 | 含义 | 示例 |
|:---|:---|:---|
| `app_` | 应用层模块 | `app_main.c`, `app_vofa.c` |
| `task_` | FreeRTOS 任务 | `task_control.c`, `task_imu.c` |
| `app_test_` | 测试代码 | `app_test_runner.c` |
| `app_pid/ff/planner` | 控制算法 | `app_pid.c`, `app_feedforward.c` |
| `filter_` | 滤波算法 | `filter_ekf.c`, `filter_madgwick.c` |
| `bsp_` | 板级支持包 | `bsp_motor.c`, `bsp_encoder.c` |
| `hal_` | 硬件抽象层 | `hal_gpio.c`, `hal_uart.c` |
| `osal_` | 操作系统抽象 | `osal_api.h`, `osal_task.c` |