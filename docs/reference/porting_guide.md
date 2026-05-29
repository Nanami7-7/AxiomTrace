# AxiomTrace Porting Guide

> [English](porting_guide.md) | [简体中文](porting_guide.md)
>
> 版本: v1.0  |  状态: 现行（中英双语合并文档）

---

## 目录

1. [架构概述](#架构概述)
2. [目录结构](#目录结构)
3. [CMake 选项](#cmake-选项)
4. [快速开始](#快速开始)
5. [添加新平台](#添加新平台)
6. [添加新 SoC](#添加新-soc)
7. [添加新开发板](#添加新开发板)
8. [接口参考](#接口参考)
9. [最佳实践](#最佳实践)

---

## 架构概述

AxiomTrace 采用 **三层架构** 设计，参考 Zephyr RTOS 的 arch/soc/board 分层模式：

```
┌─────────────────────────────────────────────────────────┐
│                      Board Layer                         │
│              (开发板特定配置: 传感器, LED, 按键)           │
├─────────────────────────────────────────────────────────┤
│                       SoC Layer                          │
│            (芯片特定实现: STM32F4, nRF52, ESP32)          │
├─────────────────────────────────────────────────────────┤
│                     Arch Layer                           │
│              (架构特定实现: Cortex-M, RISC-V)             │
├─────────────────────────────────────────────────────────┤
│                    Generic Layer                         │
│               (默认弱符号实现, 可被上层覆盖)               │
└─────────────────────────────────────────────────────────┘
```

### 设计原则

1. **向下兼容**: 现有 `axiom_port.h` 接口保持不变
2. **弱符号覆盖**: 上层实现可覆盖下层弱符号实现
3. **零依赖**: Port 实现不依赖外部库 (如 HAL)
4. **可组合**: arch + soc + board 组合出完整移植

---

## 目录结构

```
baremetal/
├── port/                      # 原有 port 目录 (保持向后兼容)
│   ├── axiom_port.h           # 公共接口定义
│   └── axiom_port_generic.c  # 默认实现
│
└── ports/                     # 新分层 port 结构
    ├── CMakeLists.txt         # 统一管理文件
    │
    ├── generic/               # 通用/主机实现
    │   └── axiom_port_generic.c
    │
    ├── arch/                  # 架构层
    │   ├── cortex-m/          # ARM Cortex-M
    │   │   ├── CMakeLists.txt
    │   │   └── axiom_port_cortex_m.c
    │   └── riscv/             # RISC-V
    │       ├── CMakeLists.txt
    │       └── axiom_port_riscv.c
    │
    ├── soc/                   # 芯片层
    │   ├── stm32f4/           # STM32F4 系列
    │   │   └── CMakeLists.txt
    │   ├── nrf52/             # Nordic nRF52 系列
    │   │   └── CMakeLists.txt
    │   └── esp32/             # Espressif ESP32
    │       └── CMakeLists.txt
    │
    └── board/                 # 板级层
        └── generic/           # 默认板级配置
```

---

## CMake 选项

### AXIOM_PLATFORM

目标处理器架构。

| 值 | 说明 |
|---|---|
| `cortex-m` | ARM Cortex-M 架构 |
| `riscv` | RISC-V 架构 |
| `host` | 主机/模拟环境 (默认) |

### AXIOM_SOC

目标芯片型号。

| 值 | 说明 |
|---|---|
| `generic` | 通用配置 (默认) |
| `stm32f4` | STM32F4 系列 |
| `nrf52` | Nordic nRF52 系列 |
| `esp32` | Espressif ESP32 |

### AXIOM_BOARD

目标开发板型号 (可选, 覆盖 SoC 默认配置)。

| 值 | 说明 |
|---|---|
| `generic` | 通用配置 (默认) |

---

## 快速开始

### 主机环境 (默认)

```bash
cmake -B build -S .
cmake --build build
```

自动检测架构为 `host`, 使用通用 port 实现。

### Cortex-M (通用)

```bash
cmake -B build -DAXIOM_PLATFORM=cortex-m -DAXIOM_SOC=generic
cmake --build build
```

### STM32F4 完整配置

```bash
cmake -B build -DAXIOM_PLATFORM=cortex-m -DAXIOM_SOC=stm32f4 -DAXIOM_BOARD=generic
cmake --build build
```

### nRF52 完整配置

```bash
cmake -B build -DAXIOM_PLATFORM=cortex-m -DAXIOM_SOC=nrf52 -DAXIOM_BOARD=generic
cmake --build build
```

### ESP32 完整配置

```bash
cmake -B build -DAXIOM_PLATFORM=riscv -DAXIOM_SOC=esp32 -DAXIOM_BOARD=generic
cmake --build build
```

---

## 添加新平台

### 1. 创建架构目录

```bash
mkdir -p baremetal/ports/arch/<platform-name>
```

### 2. 实现 Port 函数

创建 `axiom_port_<platform-name>.c`，实现以下函数：

```c
#include "axiom_port.h"

/* 必须实现的函数 */
uint32_t axiom_port_timestamp(void);
void axiom_port_critical_enter(void);
void axiom_port_critical_exit(void);

/* 可选实现 (默认空实现) */
void axiom_port_string_out(const char *str);
void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                           const uint8_t *payload, uint8_t payload_len);
uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len);
int axiom_port_flash_erase(uint32_t addr, uint32_t len);
int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);
```

### 3. 创建 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)

set(AXIOM_PORT_ARCH_NAME "<platform-name>")

set(AXIOM_PORT_<PLATFORM>_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/axiom_port_<platform-name>.c
    PARENT_SCOPE
)
```

### 4. 更新上层 CMakeLists

在 `baremetal/ports/CMakeLists.txt` 中添加新平台到 `AXIOM_PLATFORM_OPTIONS`。

---

## 添加新 SoC

### 1. 创建 SoC 目录

```bash
mkdir -p baremetal/ports/soc/<soc-name>
```

### 2. 实现芯片特定功能

创建 `axiom_port_<soc-name>.c`，实现芯片特定功能：

- Flash 操作
- 外设初始化
- 时钟配置

### 3. 创建 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)

set(AXIOM_PORT_SOC_NAME "<soc-name>")
set(<SOC>_SPECIFIC_CONFIG "value")
```

---

## 添加新开发板

### 1. 创建 Board 目录

```bash
mkdir -p baremetal/ports/board/<board-name>
```

### 2. 实现板级配置

创建 `axiom_port_<board-name>.c`，实现：

- GPIO 配置 (LED, 按钮)
- 传感器初始化
- 特定外设配置

### 3. 创建 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)

set(AXIOM_PORT_BOARD_NAME "<board-name>")
set(<BOARD>_SPECIFIC_PIN 13)  # 示例: LED_PIN
```

---

## 接口参考

### axiom_port_timestamp()

返回单调递增的微秒时间戳。

```c
extern uint32_t axiom_port_timestamp(void);
```

**实现要求**:
- 返回值应为微秒级精度
- 自然回绕 (32位溢出)
- ISR 安全 (无锁实现)

**Cortex-M 推荐**: DWT_CYCCNT 寄存器
**RISC-V 推荐**: mcycle CSR

### axiom_port_critical_enter() / axiom_port_critical_exit()

临界区保护。

```c
extern void axiom_port_critical_enter(void);
extern void axiom_port_critical_exit(void);
```

**实现要求**:
- 必须可嵌套 (配对调用)
- ISR 安全
- 禁止调度器 (如使用 RTOS)

**Cortex-M 推荐**: PRIMASK 或 BASEPRI
**RISC-V 推荐**: mstatus.MIE

### axiom_port_string_out()

开发日志输出。

```c
extern void axiom_port_string_out(const char *str);
```

**实现要求**:
- 非阻塞
- ISR 可调用
- 目标设备: UART / RTT / SWO / ITM

### axiom_port_fault_hook()

故障钩子函数。

```c
extern void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                                  const uint8_t *payload, uint8_t payload_len);
```

**调用时机**: AX_FAULT 事件触发时

**用途示例**:
- 触发硬件断点
- 写入调试器
- 保存故障上下文

### axiom_port_fault_snapshot()

寄存器快照。

```c
extern uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len);
```

**返回**: 实际写入字节数

**Cortex-M 推荐快照**:
- R0-R3, R12, LR, PSR, PC
- CONTROL, PRIMASK, BASEPRI

### axiom_port_flash_erase() / axiom_port_flash_write() / axiom_port_flash_read()

Flash 操作 (用于 Fault Capsule)。

```c
extern int axiom_port_flash_erase(uint32_t addr, uint32_t len);
extern int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);
extern int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len);
```

**返回**: 0 成功, -1 失败

**要求**:
- 非 ISR 调用
- 擦除必须按页/扇区
- `AXIOM_CAPSULE_ENABLED=1` 时必须实现 read/write/erase 才能跨重启保留 capsule
- `tiny` / `prod` preset 默认关闭 capsule；需要故障舱时显式打开并配置 Flash 区域

---

## 最佳实践

### 0. 先选资源预设

移植新 MCU 时优先从预设开始，再做少量单项覆盖：

| 预设 | 适用场景 | 建议 |
|:---|:---|:---|
| `custom` | 已有工程规范或精确资源预算 | 使用单项 `AXIOM_*` 宏组合，不接受预设覆写 |
| `tiny` | 低频、极小 SRAM、严格 ISR 栈预算 | 首个 bring-up 默认用它，确认链路后再打开更多功能 |
| `prod` | 量产默认 | 保留结构化事件，裁掉 debug 文本和 probe |
| `field` | 现场服务/售后诊断 | 保留 probe 和较小 capsule 窗口 |
| `dev` | 主机、评估板、开发调试 | 功能完整，适合验证，不代表最低资源占用 |

```bash
cmake -B build-tiny -S . -G Ninja -DAXIOM_PRESET=tiny
```

### 1. 使用弱符号

```c
// 在 generic 层使用 __attribute__((weak))
__attribute__((weak)) uint32_t axiom_port_timestamp(void) {
    return 0u;  // 默认返回 0
}
```

### 2. 保持热路径高效

```c
// 错误: 热路径中不应有复杂逻辑
void axiom_port_critical_enter(void) {
    // 禁止在临界区入口做复杂判断
}

// 正确: 简单禁用中断
void axiom_port_critical_enter(void) {
    __asm volatile ("cpsid i" : : : "memory");
}
```

### 3. Flash 操作的原子性

```c
// 确保 Flash 操作不被中断打断
void axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    critical_enter();
    // 执行 Flash 写入
    critical_exit();
}
```

### 4. 时间戳精度

```c
// 推荐: 使用硬件计数器
uint32_t axiom_port_timestamp(void) {
    return DWT_CYCCNT;  // 单周期递增, 精度高
}

// 不推荐: SysTick 可能有 jiter
// 不推荐: 软件计数器可能被中断延迟
```

### 5. 向后兼容

保持 `axiom_port.h` 接口不变，只扩展内部实现。

---

## 常见问题

### Q: 如何选择 arch / soc / board?

**A**: 参考以下决策树:

```
需要修改处理器指令?
├── Yes → 添加 arch 层
└── No
    ├── 需要修改芯片外设?
    │   ├── Yes → 添加 soc 层
    │   └── No
    │       ├── 需要修改开发板 IO?
    │       │   ├── Yes → 添加 board 层
    │       │   └── No → 使用 generic
```

### Q: 如何调试 Port 实现?

**A**: 使用主机模式测试:

```bash
# 启用调试输出
cmake -B build -DAXIOM_PLATFORM=host -DAXIOM_DEBUG=ON
```

### Q: 如何验证时间戳精度?

**A**: 使用 `axiom_port_timestamp()` 连续调用检查递增:

```c
uint32_t t1 = axiom_port_timestamp();
uint32_t t2 = axiom_port_timestamp();
assert(t2 > t1);  // 应始终成立
```

---

## 相关文档

- [AxiomTrace README](../../README.md)
- [平台参考实现](./platform_reference.md)

---

## 更新日志

| 版本 | 日期 | 说明 |
|---|---|---|
| 1.0 | 2026-04-30 | 初始版本, 三层架构设计 |
