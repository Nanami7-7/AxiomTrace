> [English](backend_contract.md) | [简体中文](backend_contract_zh.md)

# AxiomTrace 后端契约规范

> 版本：v0.1 | 状态：**已冻结** | 参考代码：`baremetal/backend/axiom_backend.h`

---

## 1. 概述

后端是将编码后的事件记录（Event Records）从核心环形缓冲区（Core Ring Buffer）移动到外部世界的传输层。所有后端都遵循单一契约；任何后端都不得定义私有协议。

**设计目标**：

- **零侵入**：添加新后端无需修改 `core/` 或 `frontend/`。
- **用户拥有驱动程序**：AxiomTrace 提供契约和分发器；用户提供实际的外设驱动函数。
- **非阻塞**：后端不得无限期阻塞。如果传输忙，返回 `-EAGAIN`。

---

## 2. 后端描述符

```c
typedef struct {
    uint16_t size;              /* sizeof(this struct) at compile time */
    const char *name;
    uint32_t caps;

    int  (*write)(const uint8_t *buf, uint16_t len, void *ctx);
    int  (*ready)(void *ctx);
    int  (*flush)(void *ctx);
    int  (*panic_write)(const uint8_t *buf, uint16_t len, void *ctx);

    void (*on_drop)(uint32_t lost, void *ctx);
    void *ctx;
} axiom_backend_t;

/* 推荐：使用 AXIOM_BACKEND_INIT() 实现结构体前向兼容初始化 */
#define AXIOM_BACKEND_INIT(...)  { .size = sizeof(axiom_backend_t), __VA_ARGS__ }
```

### 2.1 字段描述

| 字段 | 类型 | 必选 | 描述 |
|------|------|------|------|
| `size` | `uint16_t` | 自动 | 编译时结构体大小。由 `AXIOM_BACKEND_INIT()` 自动填充。旧的零初始化后端（size==0）视为旧版布局。 |
| `name` | `const char *` | 是 | 用于诊断的可读名称 |
| `caps` | `uint32_t` | 否 | 能力标志（见第 4 节） |
| `write` | `int (*)(...)` | 是 | 传输一个完整的帧。必须是中断安全（IRQ-safe）的或在临界区调用。成功返回 0，忙返回 `-EAGAIN`，错误返回负数。 |
| `ready` | `int (*)(void*)` | 否 | 如果后端可以接受数据，则返回非零值。在 `write` 之前调用，以避免不必要的丢弃。 |
| `flush` | `int (*)(void*)` | 否 | 排空任何内部缓冲区。由用户显式调用或在关闭期间调用。 |
| `panic_write` | `int (*)(...)` | 否 | 在故障/崩溃上下文中使用的备用写入。必须尽可能简单（例如，轮询 UART 字节输出）。如果为 NULL，则使用 `write`。 |
| `on_drop` | `void (*)(...)` | 否 | 当核心丢弃旨在发送到此后端的帧时调用的回调。`lost` = 连续丢弃的帧数。 |
| `ctx` | `void *` | 否 | 传递给所有回调的用户上下文指针。 |

### 2.2 注册

```c
/* 用户代码：在启动时注册后端 */
static const axiom_backend_t my_uart = AXIOM_BACKEND_INIT(
    .name = "uart0",
    .write = my_uart_dma_send,
    .ready = my_uart_tx_ready,
    .ctx = &uart0_handle
);
axiom_backend_register(&my_uart);
```

**规则**：

- 注册必须在第一次日志调用之前发生（通常在完成外设初始化的 `main()` 中）。
- 注册**不是**通过链接器节（linker sections）自动发现的（为了移植性刻意删除了此特性）。
- 最多可以注册 `AXIOM_BACKEND_MAX` 个后端（默认为 4，编译时可配置）。
- `axiom_backend_register()` 如果结构体的 `size` 字段非零但小于 `sizeof(axiom_backend_t)`，则返回 `-3`——表明该后端是使用旧版头文件编译的。需要重新编译后端。

---

## 3. 生命周期

1. **启动时**：用户创建 `axiom_backend_t` 实例并调用 `axiom_backend_register()`。
2. **运行时**：核心在向环形缓冲区写入一帧后调用 `axiom_backend_dispatch()`。
   - 对于每个已注册的后端，核心调用 `ready()`（如果非 NULL）。
   - 如果已就绪，核心调用 `write()` 并传入完整的帧缓冲区。
   - 如果 `write()` 返回 `-EAGAIN` 或 `ready()` 返回 false，核心增加全局丢弃计数器并调用 `on_drop()`（如果非 NULL）。
3. **故障时**：如果发生故障，核心尝试在所有提供该功能的后端上执行 `panic_write()`。
4. **关闭时**：用户可以调用 `axiom_backend_flush_all()` 来排空缓冲区。

---

## 4. 能力标志

```c
#define AXIOM_BACKEND_CAP_MEMORY  (1u << 0)  /* RAM/Flash 直接转储 */
#define AXIOM_BACKEND_CAP_UART    (1u << 1)  /* UART 字节流 */
#define AXIOM_BACKEND_CAP_USB     (1u << 2)  /* USB CDC */
#define AXIOM_BACKEND_CAP_RTT     (1u << 3)  /* SEGGER RTT */
#define AXIOM_BACKEND_CAP_SWO     (1u << 4)  /* SWO/ITM */
#define AXIOM_BACKEND_CAP_CANFD   (1u << 5)  /* CAN-FD */
#define AXIOM_BACKEND_CAP_CAPSULE (1u << 6)  /* Flash 故障胶囊 */
```

能力标志仅作为工具和过滤的**参考信息**。核心不会根据能力标志强制执行行为。

---

## 5. 后端模板

AxiomTrace 为常见后端提供**模板**。模板是一个 `.c` 文件，它实现了 `axiom_backend_t` 接口，除了实际的外设发送函数（由用户提供）。

| 模板文件 | 传输方式 | 用户必须提供 |
|----------|----------|--------------|
| `axiom_backend_memory.c` | RAM | 无（直接使用核心环形缓冲区） |
| `axiom_backend_uart.c` | UART | `uart_write(const uint8_t *buf, uint16_t len, void *ctx)` |
| `axiom_backend_usb.c` | USB CDC | `usb_cdc_write(const uint8_t *buf, uint16_t len, void *ctx)` |
| `axiom_backend_rtt.c` | RTT | `SEGGER_RTT_WriteBlock()` 或封装 |
| `axiom_backend_swo.c` | SWO/ITM | `ITM_SendChar()` 或激励端口写入 |
| `axiom_backend_canfd.c` | CAN-FD | `canfd_send(uint32_t id, const uint8_t *buf, uint8_t len, void *ctx)` |
| `axiom_capsule_flash.c` | Flash | `flash_erase_sector()` / `flash_program()` (移植层) |

**用法**：将模板 `.c` 文件添加到您的构建系统中。实现所需的用户函数。注册后端。

---

## 6. 约束

- `write()` 必须是中断安全的，或者仅从临界区调用（分发器保证了这一点）。
- `write()` 不得无限期阻塞；如果传输忙，返回 `-EAGAIN`。
- 后端不得动态分配内存。
- 后端不得修改传递给 `write()` 的帧缓冲区。
- `panic_write()` 应避免使用 DMA、中断或复杂的状态机。首选轮询字节输出。

