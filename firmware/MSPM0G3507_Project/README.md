# MSPM0G3507 四电机控制器

当前发布版本：**v0.1.0**（2026-07-18）
目标硬件：TI MSPM0G3507 + 四路 DRV8870 + 四路正交编码器 + LSM6DSR。

本仓库包含实时控制固件、硬件诊断 Target、协议文档，以及一个基于 PySide6 的中英双语配置与实时绘图软件。

## 主要能力

- FreeRTOS 下四电机 5 ms 速度闭环；
- DRV8870 单 PWM 锁相控制，40%～55%配置为停止死区；
- A/M1、B/M2、C/M3、D/M4 与 RB、RF、LF、LB 编码器的统一映射；
- PID、前馈、模型辨识、位置/角度控制与 IMU 基础能力；
- UART0 115200 文本协议 v1 与 11 通道 FireWater 遥测；
- PySide6 中英双语 GUI：串口配置、参数设置、控制命令、实时曲线、CSV 导出和原始终端；
- 生产固件与 DRV8870 FactoryTest 独立 Keil Target。

## 快速开始

### 1. 烧录生产固件

推荐使用版本化产物：

```text
MSPM0G3507_FreeRTOS/keil/Release/MSPM0G3507_MotorController_v0.1.0.hex
```

FactoryTest 仅用于受控台架诊断，不应作为车辆正常运行固件：

```text
MSPM0G3507_FreeRTOS/keil/Release/MSPM0G3507_DRV8870_FactoryTest_v0.1.0.hex
```

### 2. 启动 GUI

```powershell
cd tools\mspm0_configurator
python -m venv .venv
.venv\Scripts\python.exe -m pip install -e .
.venv\Scripts\python.exe -m mspm0_configurator
```

选择串口并以 **115200 8-N-1** 连接。首次运行必须悬空车轮并使用限流电源。GUI 0.1 只把配置保存为 PC 端 JSON，不会写入 MCU Flash。

### 3. 手工协议检查

```text
Info?
Config?
Status=0
Stream=1
Stream=0
StopAll
```

命令以 `\r\n` 结束。详见协议文档。

## 工程结构

```text
MSPM0G3507_FreeRTOS/
├─ Config/                  版本、机械参数、引脚与生成配置
├─ HAL/                     DriverLib 薄封装
├─ BSP/                     板级外设和 DRV8870/编码器/IMU
├─ Application/             任务、控制算法、协议与应用入口
├─ Middleware/              OSAL、滤波和日志
├─ Docs/                    架构、接口、协议、外设手册与Plans阶段记录
└─ keil/                    Keil 工程、FactoryTest、发布 HEX
tools/mspm0_configurator/   PySide6 配置与绘图软件
```

## 文档入口

- [架构与重构报告](MSPM0G3507_FreeRTOS/Docs/架构与重构报告_v0.1.0.md)
- [公共接口参考](MSPM0G3507_FreeRTOS/Docs/公共接口参考_v0.1.0.md)
- [外设分类开发参考手册](MSPM0G3507_FreeRTOS/Docs/外设分类开发参考手册.md)
- [通信协议 v1](MSPM0G3507_FreeRTOS/Docs/通信协议_v1.md)
- [GUI 配置软件使用说明](MSPM0G3507_FreeRTOS/Docs/GUI配置软件使用说明_v0.1.0.md)
- [DRV8870 综合技术审查与安全重构方案](MSPM0G3507_FreeRTOS/Docs/DRV8870综合技术审查与安全重构方案.md)
- [阶段计划与历史审计](MSPM0G3507_FreeRTOS/Docs/Plans/README.md)
- [v0.1.0 发布说明](RELEASE_NOTES_v0.1.0.md)

## v0.1.0 安全边界

1. 当前单 PWM 锁相硬件不具备软件可独立选择的真实 Coast/Brake；停止语义是 50% 中性锁相。
2. ADC 读取的是 LMV321 放大器输出，尚未完成“ADC 值 → 电流”的标定，不能作为正式过流保护阈值。
3. PPR、减速比、轮径、轮距和安装方向已集中配置，但仍须按实际整车标定。
4. 40%～55%是软件死区，跨越边界会产生占空比跳变；高惯量负载必须限制目标斜率。
5. `task_menu.c`仍同时承担菜单、协议和遥测调度，0.2 将继续拆分 UART 输出仲裁与命令服务。
