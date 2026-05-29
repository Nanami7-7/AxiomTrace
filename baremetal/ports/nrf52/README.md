# nRF52 Platform Port for AxiomTrace

nRF52 系列微控制器（nRF52810, nRF52832, nRF52840 等）的 AxiomTrace 平台移植实现。

## 特性

- **Timestamp**: 使用 RTC0 计数器，LFRC 时钟源（32.768kHz），微秒级分辨率
- **Critical Section**: 基于 PRIMASK 的全局中断禁用，支持嵌套
- **RTT Backend**: SEGGER RTT 高速实时传输，零拷贝设计
- **零依赖**: 不依赖 Nordic SDK，直接操作硬件寄存器

## 文件结构

```
baremetal/ports/nrf52/
├── axiom_port_nrf52.c    # 平台移植实现（timestamp、critical section）
├── axiom_backend_rtt.c    # RTT 后端实现
├── CMakeLists.txt         # CMake 构建配置
└── README.md              # 本文件
```

## 快速开始

### 1. 添加到你的项目

在你的 `CMakeLists.txt` 中添加：

```cmake
# 添加 AxiomTrace nRF52 端口
add_subdirectory(path/to/axiomtrace/baremetal/ports/nrf52 axiom_nrf52)

# 链接到你的固件
target_link_libraries(your_app PRIVATE axiomtrace axiom_port_nrf52)
```

### 2. 初始化

在你的固件初始化代码中：

```c
#include "axiomtrace.h"

// 初始化 AxiomTrace
axiom_init();

// 初始化 RTT 后端（可选，如果你需要 RTT 输出）
axiom_backend_rtt_init();

// 初始化 RTC（如果在 port 中未自动初始化）
axiom_port_rtc_init();
```

### 3. 使用示例

```c
#include "axiomtrace.h"

int main(void) {
    // 初始化
    axiom_init();
    axiom_backend_rtt_init();
    axiom_port_rtc_init();

    // 记录事件
    AX_EVT(INFO, 0x01, 0x01, (uint16_t)100);

    // 开发日志（RTT 输出）
    AX_LOG("System initialized");

    // 探针数据
    AX_PROBE("adc", (uint16_t)4095);

    return 0;
}
```

## RTT 配置说明

### J-Link 连接

1. 使用 J-Link 或 J-Trace 调试器连接 nRF52 目标板
2. 确保 SWD 接口正确连接（SWDIO, SWCLK, GND）
3. 目标板供电正常

### J-Link 软件包

推荐使用 V6.30 或更高版本的 J-Link 软件包：

- **Windows**: [SEGGER 下载页面](https://www.segger.com/downloads/jlink/)
- **Linux/macOS**: 通过包管理器或官网下载

### RTT Viewer 使用

#### 方法 1: J-Link RTT Viewer（官方工具）

1. 打开 J-Link RTT Viewer
2. 选择目标芯片型号（如 nRF52832_xxAA）
3. 选择连接接口（SWD）
4. 点击 OK 连接

```
J-Link RTT Viewer
├── Connection
│   ├── Device: nRF52832_xxAA
│   ├── Interface: SWD
│   └── Speed: 4000 kHz
└── RTT Config
    ├── Up Buffer: 1024 bytes
    └── Down Buffer: 256 bytes
```

#### 方法 2: Ozone 调试器

在 Ozone 中启用 RTT：

1. 打开 Project -> Settings
2. 选择调试配置
3. 启用 RTT 并配置缓冲区

#### 方法 3: 命令行 RTT

使用 J-Link Commander 或 J-Link RTT Client：

```bash
# J-Link Commander
JLink.exe -device nRF52832_xxAA -if SWD -speed 4000
# 在命令行中输入 "rtt" 启动 RTT

# 或使用 RTT Client
JLinkRTTClient.exe
```

### 缓冲区配置

`axiom_backend_rtt.c` 默认配置：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| Buffer Index | 1 | 使用通道 1（通道 0 通常用于日志） |
| Up Buffer Size | 1024 bytes | 上行缓冲区（MCU→Host）|
| Flags | 0 | 默认标志 |

如需修改，编辑 `axiom_backend_rtt.c` 中的宏定义：

```c
#define AXIOM_RTT_BUFFER_SIZE     1024
#define AXIOM_RTT_BUFFER_INDEX    1
```

## Timestamp 实现细节

### RTC 配置

- **时钟源**: LFRC（32.768kHz 低频 RC 振荡器）
- **预分频**: 0（无分频，每 tick 为 1/32768 秒 ≈ 30.517μs）
- **计数器**: 32 位，溢出周期约 36 小时

### 微秒转换

```
ticks = RTC->COUNTER
微秒 = ticks * 30517 / 1000
```

近似公式，误差约 0.05%。

### 精度考虑

- LFRC 精度：±250ppm（典型值）
- 对于调试和性能分析足够精确
- 如需更高精度，可使用外部 32.768kHz 晶振并启用 LFXO

## Critical Section 实现

使用 ARM Cortex-M4 的 PRIMASK 寄存器实现全局中断禁用：

```c
void axiom_port_critical_enter(void) {
    __asm volatile ("cpsid i");  // 禁用中断
}

void axiom_port_critical_exit(void) {
    __asm volatile ("cpsie i");  // 启用中断
}
```

**特性**：
- 支持嵌套（通过引用计数）
- ISR 安全（确保配对调用）
- 零开销（单指令）

## 与 Nordic SDK 集成

虽然本移植不依赖 Nordic SDK，但可以与 nRF5 SDK 共存：

### 软设备兼容

**重要**：在 SoftDevice（S132, S140 等）启用的情况下：

- RTC0 通常被 SoftDevice 占用
- 请使用 RTC1 或 RTC2 作为 AxiomTrace timestamp 源
- 修改 `axiom_port_nrf52.c` 中的 `RTC_BASE` 定义

```c
// SoftDevice 环境下使用 RTC1
#define RTC_BASE        0x40011000UL  // RTC1 base address
```

### nrfx 驱动集成

如果你使用 nrfx 驱动，可以利用其 RTC 驱动：

```c
#include <nrfx_rtc.h>

// 使用 nrfx RTC 替代直接寄存器访问
```

## 故障排除

### RTT 无输出

1. **检查连接**：
   - J-Link 固件是否为最新版本
   - SWD 接口是否正确连接

2. **检查缓冲区配置**：
   - RTT Viewer 中选择的芯片型号是否正确
   - 缓冲区大小是否匹配

3. **检查初始化**：
   - `axiom_backend_rtt_init()` 是否在 `axiom_init()` 之后调用
   - 是否有编译错误

### Timestamp 不准确

1. 检查 LFRC 是否正常工作
2. 如果使用外部晶振，确保在代码中启用 LFXO
3. 检查 RTC 是否有其他中断干扰

### 编译错误

确保包含正确的头文件路径：

```
-I<path_to>/AxiomTrace/baremetal/port
-I<path_to>/AxiomTrace/baremetal/include
-I<path_to>/SEGGER_RTT (如果使用独立 RTT)
```

## 性能数据

| 操作 | 耗时 |
|------|------|
| `axiom_port_timestamp()` | ~20 cycles (~0.5μs @ 40MHz) |
| `axiom_port_critical_enter/exit` | 1 cycle each |
| RTT write (1 byte) | ~10 cycles |
| RTT write (64 bytes) | ~50 cycles |

## 许可证

与 AxiomTrace 主项目相同，详见 [AxiomTrace LICENSE](../../../LICENSE)。

## 参考资料

- [SEGGER RTT 官方文档](https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/)
- [nRF52 Series Reference Manual](https://infocenter.nordicsemi.com/)
- [ARM Cortex-M4 Programming Guide](https://developer.arm.com/documentation/)
