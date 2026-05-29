# ESP32 Platform Port

ESP32 系列微控制器的 AxiomTrace 平台适配实现。

## 特性

- **esp_timer 时间戳**: 利用 ESP-IDF 硬件定时器实现微秒级时间戳，双核安全
- **FreeRTOS 临界区**: `portENTER_CRITICAL` 多核安全中断禁用机制
- **UART COBS 传输**: 非阻塞传输，COBS 帧编码，0x00 帧分隔符
- **零 malloc**: 所有缓冲区均为静态分配
- **ISR 安全**: 可在中断上下文中调用
- **Kconfig 集成**: 通过 menuconfig 配置 UART 引脚和波特率

## 文件结构

```
baremetal/ports/esp32/
├── axiom_port_esp32.c     # 平台抽象层实现
├── axiom_backend_uart.c   # UART 后端
├── CMakeLists.txt         # ESP-IDF component 配置
├── Kconfig                # menuconfig 选项
└── README.md              # 本文档
```

## 快速开始

### 方式一：使用 idf.py 和 menuconfig

#### 1. 将组件添加到项目中

假设您的项目结构:

```
my_esp32_project/
├── main/
│   ├── CMakeLists.txt
│   └── main.c
├── components/
│   └── axiomtrace_esp32/   <- 将 axiom_port_esp32.c 等文件放在这里
└── CMakeLists.txt
```

或者在您项目的 CMakeLists.txt 中添加:

```cmake
# my_esp32_project/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# AxiomTrace ESP32 配置 (可选)
set(AXIOM_ESP32_UART_NUM 1)
set(AXIOM_ESP32_UART_TX_GPIO 10)
set(AXIOM_ESP32_UART_RX_GPIO 9)
set(AXIOM_ESP32_UART_BAUD 921600)

project(my_esp32_project)
```

#### 2. 配置菜单

```bash
cd my_esp32_project
idf.py menuconfig
```

导航到: `Component config -> AxiomTrace ESP32 Configuration`

```
-AxiomTrace ESP32 Configuration
  -> Enable AxiomTrace ESP32 Port [Y]
  -> UART port number [1]
  -> UART TX GPIO pin [10]
  -> UART RX GPIO pin [9]
  -> UART baud rate [921600]
  -> UART TX buffer size [256]
  -> Enable COBS encoding [Y]
```

#### 3. 初始化代码

```c
#include "axiomtrace.h"
#include "driver/uart.h"

/* 使用 Kconfig 配置的引脚 (需要在 menuconfig 中设置) */
#if defined(CONFIG_AXIOM_ESP32_UART_NUM)
#define AXIOM_UART_NUM    CONFIG_AXIOM_ESP32_UART_NUM
#define AXIOM_UART_TX_PIN CONFIG_AXIOM_ESP32_UART_TX_GPIO
#define AXIOM_UART_RX_PIN CONFIG_AXIOM_ESP32_UART_RX_GPIO
#define AXIOM_UART_BAUD   CONFIG_AXIOM_ESP32_UART_BAUD
#else
#define AXIOM_UART_NUM    UART_NUM_1
#define AXIOM_UART_TX_PIN 10
#define AXIOM_UART_RX_PIN 9
#define AXIOM_UART_BAUD   921600
#endif

void app_main(void) {
    /* 1. 初始化 UART 后端 */
    axiom_backend_uart_init(AXIOM_UART_NUM,
                           AXIOM_UART_TX_PIN,
                           AXIOM_UART_RX_PIN,
                           AXIOM_UART_BAUD);

    /* 2. 注册 UART 后端 */
    axiom_backend_t uart_backend = axiom_backend_uart("esp32-uart", NULL);
    axiom_backend_register(&uart_backend);

    /* 3. 初始化 AxiomTrace */
    axiom_init();

    /* 4. 使用跟踪 */
    AX_EVT(INFO, 0x01, 0x01, (uint16_t)3200);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 方式二：使用 ESP-Prog 和自动下载

ESP-Prog 是乐鑫官方的调试器/编程器，支持 UART 和 JTAG 接口。

#### 硬件连接

| ESP32 引脚 | ESP-Prog 引脚 | 说明 |
|-----------|--------------|------|
| TX (GPIO10) | RX | 发送数据到主机 |
| RX (GPIO9) | TX | 接收命令 (可选) |
| GND | GND | 共地 |
| EN | TRST_N | 复位信号 (可选) |

#### 串口参数

- 波特率: 921600 (推荐) 或 115200
- 数据位: 8
- 停止位: 1
- 校验: 无
- 流控制: 无

#### 接线注意事项

**警告**: 默认 GPIO9/GPIO10 是 SPI Flash 引脚!

对于 ESP32-WROOM 模块:

| 模块 | 可用 GPIO | 推荐 UART2 引脚 |
|-----|----------|---------------|
| ESP32-WROOM-32 | 1, 3, 5, 6, 7, 8, 9, 10, 11 | TX=17, RX=16 |
| ESP32-WROVER | 1, 3, 5, 6, 7, 8, 9, 10, 11 | TX=17, RX=16 |

对于使用内置 SPI Flash 的模块:
- **不要使用** GPIO6-11 (用于 SPI Flash)
- **推荐使用** UART2 (GPIO17=TX, GPIO16=RX)

### 方式三：idf.py 添加组件

```bash
# 在 components 目录下创建 axiomtrace_esp32 组件
mkdir -p components/axiomtrace_esp32
# 复制 axiom_port_esp32.c, axiom_backend_uart.c, CMakeLists.txt, Kconfig 到该目录

# 在项目 CMakeLists.txt 中
idf_component_register(...)
```

或使用:

```bash
idf.py add-component axiomtrace_esp32
# 然后替换生成的文件
```

## GPIO 说明

### ESP32 UART 引脚映射

| UART | TX | RX | RTS | CTS |
|-----|----|----|-----|-----|
| UART0 | GPIO1 | GPIO3 | N/A | N/A |
| UART1 | GPIO10* | GPIO9* | GPIO8 | GPIO7 |
| UART2 | GPIO17 | GPIO16 | GPIO19 | GPIO18 |

*注意: GPIO9-11 通常用于 SPI Flash，在使用前请确认您的模块配置。

### GPIO Matrix

如果需要使用其他引脚，可以通过 GPIO Matrix 配置:

```c
/* 将 UART1 映射到任意 GPIO */
uart_set_pin(UART_NUM_1, custom_tx_gpio, custom_rx_gpio,
            UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
```

## COBS 帧格式

AxiomTrace 使用 COBS (Consistent Overhead Byte Stuffing) 编码:

```
+-----------------+----------+--------+------+
| COBS 编码数据    | 0x00     | COBS   | 0x00 |
| (可变长度)       | 分隔符   | 编码    | 分隔符|
+-----------------+----------+--------+------+
```

### COBS 编码示例

原始数据: `0x01 0x02 0x00 0x03`
编码结果: `0x03 0x01 0x02 0x02 0x03 0x00`

解码时，第一个 `0x03` 表示距离下一个 0x00 (或数据结尾) 有 3 字节。

## 主机端解码

### 捕获后使用 axiom-decoder

```bash
# 当前 CLI 解码文件输入；先用串口工具捕获 trace.bin。
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format text

# 未准备 bundle 时可先查看结构化 raw 输出。
axiom-decoder trace.bin --format raw
```

### 使用 minicom 或 screen

```bash
# 使用 screen
screen /dev/ttyUSB1 921600

# 使用 minicom
minicom -D /dev/ttyUSB1 -b 921600
```

### 将输出重定向到文件

```bash
# 使用 socat 桥接串口到文件
socat -u /dev/ttyUSB1,b921600,raw,echo=0 FILE:/tmp/trace.bin,create,append

# 然后用 decoder 解码
axiom-decoder /tmp/trace.bin --bundle build/axiomtrace-bundle --format text
```

## 性能考量

- **时间戳精度**: `esp_timer_get_time()` 提供微秒级分辨率
- **双核安全**: ESP32 的 esp_timer 在双核上都是准确的
- **UART 传输**: 取决于缓冲区大小和波特率
- **帧间隔**: 建议帧间至少 100μs 间隔
- **缓冲区大小**: 根据事件频率调整 `AXIOM_ESP32_UART_TX_BUF_SIZE`

## 故障排除

### 无数据输出

1. 检查 TX 引脚连接
2. 确认 GPIO 引脚与 menuconfig 配置一致
3. 验证 UART 驱动是否正确安装
4. 检查 `axiom_backend_register()` 是否调用
5. 确认 UART TX 引脚没有被 SPI Flash 占用

### 数据丢失

1. 增加 `AXIOM_ESP32_UART_TX_BUF_SIZE`
2. 降低事件频率
3. 提高 UART 波特率
4. 检查是否存在 UART 缓冲区溢出

### 解析错误

1. 确认使用 COBS 解码
2. 检查 0x00 分隔符是否正确
3. 验证帧格式版本匹配
4. 检查主机端波特率与 ESP32 配置一致

### 编译错误

确保在 CMakeLists.txt 中添加了必要的组件依赖:

```cmake
idf_component_register(
    ...
    PRIV_REQUIRES
        esp_timer
        freertos
    REQUIRES
        driver
    ...
)
```

## 与其他 ESP-IDF 组件的兼容性

### WiFi/BLE 同时使用

AxiomTrace ESP32 端口**不依赖** WiFi 或 BLE API，可以在使用这些组件的项目中正常工作。

注意: 避免在热路径中调用 WiFi/BLE API，这可能导致时序问题。

### FreeRTOS 兼容性

使用 FreeRTOS 的临界区 (`portENTER_CRITICAL`) 确保双核安全的中断控制。

## 自定义配置

### 修改默认引脚

通过 menuconfig:
```bash
idf.py menuconfig -> Component config -> AxiomTrace ESP32 Configuration
```

或通过 CMake:
```cmake
set(AXIOM_ESP32_UART_TX_GPIO 17)
set(AXIOM_ESP32_UART_RX_GPIO 16)
```

### 调整缓冲区大小

```cmake
set(AXIOM_ESP32_UART_TX_BUF_SIZE 512)
```

或在 menuconfig 中:
```
Component config -> AxiomTrace ESP32 Configuration -> UART TX buffer size
```

### 使用不同的 UART 实例

```c
// 使用 UART2
axiom_backend_uart_init(UART_NUM_2, 17, 16, 921600);
```

## 许可证

继承 AxiomTrace 项目许可证。
