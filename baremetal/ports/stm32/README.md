# STM32F4 Platform Port

STM32F4 系列微控制器的 AxiomTrace 平台适配实现。

## 特性

- **DWT_CYCCNT 时间戳**: 利用 ARM CoreSight DWT 单元实现高精度微秒级时间戳
- **PRIMASK 临界区**: 简单高效的中断禁用/启用机制
- **UART DMA COBS 传输**: 非阻塞传输，COBS 帧编码，0x00 帧分隔符
- **零 malloc**: 所有缓冲区均为静态分配
- **ISR 安全**: 可在中断上下文中调用

## 文件结构

```
baremetal/ports/stm32/
├── axiom_port_stm32.c     # 平台抽象层实现
├── axiom_backend_uart.c   # UART DMA 后端
├── CMakeLists.txt        # CMake 配置模板
└── README.md             # 本文档
```

## 快速开始

### 1. 集成到 CMake 项目

```cmake
# 在您的 CMakeLists.txt 中
set(AXIOM_STM32_CLOCK_FREQ 84000000)  # 您的系统时钟
set(AXIOM_STM32_BAUD_RATE 921600)     # 串口波特率
set(AXIOM_UART_TX_BUFFER_SIZE 512)    # TX 缓冲区大小

# 添加 axiomtrace 子目录
add_subdirectory(vendor/axiomtrace/baremetal/ports/stm32 axiom_trace_stm32)

# 链接到您的固件
target_link_libraries(your_firmware PRIVATE axiom_trace_stm32 axiomtrace)
```

### 2. 初始化代码

```c
#include "axiomtrace.h"

int main(void) {
    // 1. 初始化平台端口
    axiom_port_init();

    // 2. 配置系统时钟 (可选，默认 168MHz)
    // axiom_port_set_system_clock(168000000);

    // 3. 初始化 UART 后端
    axiom_backend_uart_init(UART1, DMA2_Stream7, 84000000, 921600);

    // 4. 注册 UART 后端
    axiom_backend_t uart_backend = axiom_backend_uart("uart", NULL, NULL);
    axiom_backend_register(&uart_backend);

    // 5. 初始化 AxiomTrace
    axiom_init();

    // 6. 使用跟踪
    AX_EVT(INFO, 0x01, 0x01, (uint16_t)3200);

    while (1) {
        // 主循环
    }
}
```

### 3. DMA 中断处理

在您的 STM32 中断处理文件中添加:

```c
void DMA2_Stream7_IRQHandler(void) {
    // 检查传输完成标志
    if (DMA2->LISR & DMA_LISR_TCIF7) {
        DMA2->LISR = DMA_LISR_TCIF7;  // 清除标志
        axiom_backend_uart_dma_complete();
    }
}
```

## ST-Link 配置

### 硬件连接

| STM32F4 引脚 | ST-Link 引脚 | 说明 |
|-------------|-------------|------|
| PA9 (USART1_TX) | RX | 发送数据到主机 |
| PA10 (USART1_RX) | TX | 接收命令 (可选) |
| GND | GND | 共地 |
| (可选) NRST | RST | 复位信号 |

### 串口参数

- 波特率: 921600 (推荐) 或 115200
- 数据位: 8
- 停止位: 1
- 校验: 无
- 流控制: 无

### 虚拟 COM 端口

使用 ST-Link V2 或 ST-Link V3 的虚拟 COM 端口功能:

1. 安装 [ST-Link 驱动](https://www.st.com/en/development-tools/st-link-windows.html)
2. 在设备管理器中确认 COM 端口号
3. 使用串口工具连接并保存原始二进制输出（如 PuTTY、Tera Term 或 `socat`）

## GPIO 说明

### UART 引脚复用

```c
// 启用 GPIOA 时钟
RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

// 配置 PA9 为复用功能 (USART1_TX)
GPIOA->MODER &= ~GPIO_MODER_MODER9;
GPIOA->MODER |= GPIO_MODER_MODER9_1;  // 复用功能
GPIOA->AFR[1] |= (7 << 4);  // AF7 = USART1

// 配置 PA10 为复用功能 (USART1_RX)
GPIOA->MODER &= ~GPIO_MODER_MODER10;
GPIOA->MODER |= GPIO_MODER_MODER10_1;  // 复用功能
GPIOA->AFR[1] |= (7 << 8);  // AF7 = USART1
```

### UART/USART 外设选择

默认配置使用 USART1，可根据需要修改:

| 实例 | TX | RX | DMA | 时钟 |
|-----|----|----|-----|------|
| USART1 | PA9 | PA10 | DMA2_Stream7 | APB2 |
| USART2 | PA2 | PA3 | DMA1_Stream6 | APB1 |
| USART3 | PB10 | PB11 | DMA1_Stream3 | APB1 |
| UART4 | PC10 | PC11 | DMA1_Stream7 | APB1 |
| UART5 | PC12 | PD2 | DMA1_Stream0 | APB1 |

### DMA 时钟启用

```c
// DMA2 时钟 (用于 USART1)
RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

// DMA1 时钟 (用于 USART2/3)
RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
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

先用串口工具捕获原始二进制流，再用当前 Python CLI 解码：

```bash
axiom-decoder trace.bin --bundle build/axiomtrace-bundle --format text
```

如果还没有匹配的 bundle，可先做结构化 raw 解码：

```bash
axiom-decoder trace.bin --format raw
```

## 性能考量

- **时间戳精度**: DWT_CYCCNT 在 168MHz 系统时钟下提供 ~6ns 分辨率
- **DMA 传输**: 传输 256 字节约需 22μs @ 921600 波特率
- **帧间隔**: 建议帧间至少 100μs 间隔
- **缓冲区大小**: 根据事件频率调整 `AXIOM_UART_TX_BUFFER_SIZE`

## 故障排除

### 无数据输出

1. 检查 TX 引脚连接
2. 确认波特率匹配
3. 验证 DMA 传输完成中断
4. 检查 `axiom_backend_register()` 是否调用

### 数据丢失

1. 增加 `AXIOM_UART_TX_BUFFER_SIZE`
2. 降低事件频率
3. 提高 UART 波特率
4. 检查 DMA 优先级设置

### 解析错误

1. 确认使用 COBS 解码
2. 检查 0x00 分隔符是否正确
3. 验证帧格式版本匹配

## 自定义配置

### 使用其他 UART 实例

修改 `axiom_backend_uart.c` 中的外设地址定义:

```c
// 例如使用 USART2
#define UART2_BASE               0x40004400UL
#define UART2                    ((UART_TypeDef *) UART2_BASE)
#define DMA1_STREAM6_BASE        0x40026400UL + 0x18
```

### 调整缓冲区大小

```cmake
set(AXIOM_UART_TX_BUFFER_SIZE 1024)
```

### 阻塞式 panic 写入

当前 `panic_write` 使用轮询方式传输，确保在严重故障时数据不丢失。

## 许可证

继承 AxiomTrace 项目许可证。
