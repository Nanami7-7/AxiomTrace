# 文档索引 / Documentation Index

## 项目简介 / Project Overview

MSPM0G3507 FreeRTOS 四路直流电机 PID 闭环速度控制系统。

基于 TI MSPM0G3507 LaunchPad，运行 FreeRTOS 实时操作系统，支持 VOFA+ 远程调参和 AxiomTrace 调试日志。

A FreeRTOS-based 4-channel DC motor PID closed-loop speed control system running on TI MSPM0G3507 LaunchPad, with VOFA+ remote tuning and AxiomTrace debug logging support.

---

## 文档目录 / Document Map

| 文档 / Document | 说明 / Description |
|---|---|
| [architecture.md](architecture.md) | 系统架构详解 — 四层架构、任务模型、数据流、同步机制 / System architecture deep dive — layered design, task model, data flow, synchronization |
| [pid_controller.md](pid_controller.md) | PID 控制器算法详解 — 公式推导、抗饱和、多实例设计 / PID algorithm deep dive — formula derivation, anti-windup, multi-instance design |
| [vofa_protocol.md](vofa_protocol.md) | VOFA+ 协议详解 — 上行格式、下行命令、通道分配、带宽计算 / VOFA+ protocol spec — uplink format, downlink commands, channel mapping, bandwidth analysis |
| [menu_guide.md](menu_guide.md) | 菜单交互与操作指南 — 状态机、操作流程、VOFA+ 运行模式 / Menu & operation guide — state machine, step-by-step workflow, VOFA+ runtime mode |
| [development_guide.md](development_guide.md) | 开发指南 — 环境搭建、编码规范、新增模块指南、OSAL API / Development guide — setup, coding conventions, adding modules, OSAL API reference |
| [pin_assignment.md](pin_assignment.md) | 引脚分配图 — SysConfig 完整引脚表、定时器分配、PWM/编码器/ADC 配置 / Pin assignment — full pin mapping based on current SysConfig |
| [pin_changes_vs_old.md](pin_changes_vs_old.md) | SysConfig 变更记录 — 新旧引脚对比、需处理的已知问题 / SysConfig changes — old vs new pin comparison, known issues |

---

## 快速开始 / Quick Start

### 硬件准备 / Hardware Preparation

1. TI MSPM0G3507 LaunchPad
2. 4 路直流电机 + 驱动板 (如 TB6612 / L298N)
3. 4 路编码器 (520 PPR)
4. USB-UART 转换器 (115200 baud)
5. 外部电源 (电机驱动用)

### 软件环境 / Software Environment

1. Keil MDK (v5.38+)
2. TI MSPM0 SDK (v2.05.00.05)
3. VOFA+ 调参软件 (可选)

### 编译与烧录 / Build & Flash

1. 打开 `keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`
2. 编译工程 (Build → Rebuild All)
3. 下载到 LaunchPad (Flash → Download)
4. 打开串口终端 (115200 baud, 8N1)

### 首次运行 / First Run

1. 连接串口终端，按 `Enter` 显示主菜单
2. 按 `b` 设置目标 RPM (如 `200`)
3. 按 `d` 启动电机
4. 电机将以默认 PID 参数 (Kp=0.8, Ki=0.3, Kd=0.0) 运行
5. 在运行模式下可通过 VOFA+ 命令远程调参
6. 按 `q` 停止电机

---

## 适用读者 / Target Audience

- **开发者 / Developers**: [development_guide.md](development_guide.md) + [architecture.md](architecture.md)
- **使用者 / Users**: [menu_guide.md](menu_guide.md) + [pin_assignment.md](pin_assignment.md) + [pin_changes_vs_old.md](pin_changes_vs_old.md)