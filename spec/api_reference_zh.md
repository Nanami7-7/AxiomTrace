> [English](api_reference.md) | [简体中文](api_reference_zh.md)

# AxiomTrace API 参考手册

> 版本：v0.1 | 状态：**冻结中（v0.1-frontend 定稿）**

---

## 1. 概述

AxiomTrace 提供分层的 API 接口：

- **第 0 层 — 快速开始**：硬编码 ID，零配置，单文件库。
- **第 1 层 — 结构化**：X-Macro 事件定义，编译期 ID 管理。
- **第 2 层 — 团队规模**：YAML 字典 + Python 代码生成，全语义控制。

所有层级最终都会编译为相同的二进制帧；其区别仅在于开发体验和类型安全性。

---

## 2. 前端宏 (Frontend Macros)

### 2.1 AX_LOG — 开发日志

```c
AX_LOG("Boot complete, version=%u", 1u);
AX_LOG_DEBUG("Sensor temp=%d", (int16_t)25);
AX_LOG_INFO("System ready");
AX_LOG_WARN("Battery low, level=%u", 15u);
AX_LOG_ERROR("Comm timeout, retries=%u", 3u);
```

**语义**：

- 用于开发和现场调试的人类可读文本。
- 在 `PROD` (生产) Profile 中，所有 `AX_LOG*` 宏都会展开为空（零代码/数据开销）。
- 格式字符串 **不会** 传输到二进制帧中；它们由主机端解码器通过字典或从 ELF 节提取来还原。
- 参数通过 `_Generic` 类型分发被编码到二进制有效载荷中。

**Profile 行为**：

| Profile | `AX_LOG` | `AX_LOG_*` |
|---------|----------|------------|
| `DEV`   | 激活     | 激活       |
| `FIELD` | 激活     | 激活       |
| `PROD`  | **无操作**| **无操作** |

---

## 3. AX_EVT — 结构化事件

```c
AX_EVT(INFO, MOTOR, START, (uint16_t)3200);
AX_EVT(WARN, SENSOR, TEMP_HIGH, (uint16_t)adc_val, (int16_t)limit);
AX_EVT(ERROR, COM, TIMEOUT, (uint8_t)bus_id, (uint32_t)ms);
```

**语义**：

- 主要的生产级 API。无论何种 Profile，始终会被编译。
- `MOTOR`, `START` 是模块/事件名称，通过 X-Macro 或代码生成解析为 `module_id` / `event_id`。
- 参数通过 `_Generic` 保证类型安全；类型不匹配会导致编译错误。
- **Direct-to-Ring (D2R)**：编码直接在环形缓冲区中进行，确保零拷贝和极小的栈开销。
- 二进制帧仅包含 ID 和类型化有效载荷 —— 无字符串。

**要求**：

- 第 1/2 层：在包含 `axiomtrace.h` 之前必须定义 `AXIOM_MODULE_LIST` 和 `AXIOM_EVENT_LIST`。
- 第 0 层：直接使用原始 ID：`AX_EVT_RAW(INFO, 0x03, 0x01, (uint16_t)3200)`。

---

## 4. AX_PROBE — 时序探针

```c
AX_PROBE("pwm_duty", (uint16_t)duty);
AX_PROBE("adc_sample", (uint16_t)adc);
```

**语义**：

- 高频、低开销的信号追踪。
- 使用独立的 Probe Ring (探针环)，避免淹没主事件日志。
- 极简报头（可选：无 CRC，无模块 ID），以实现最大吞吐量。
- 推荐后端：SWO/ITM。

**Profile 行为**：

| Profile | `AX_PROBE` |
|---------|------------|
| `DEV`   | 激活       |
| `FIELD` | 激活       |
| `PROD`  | **无操作** |

---

## 5. AX_FAULT — 故障事件

```c
AX_FAULT(MOTOR, OVERCURRENT, (uint8_t)phase, (int16_t)current);
```

**语义**：

- 关键故障日志。无论何种 Profile，始终会被编译。
- 触发 `axiom_port_fault_hook()`。
- 启动 Fault Capsule (故障舱) 冻结（故障前窗口 + 故障后窗口捕获）。
- 级别隐式为 `AXIOM_LEVEL_FAULT`。

---

## 6. AX_KV — 键值对事件

```c
AX_KV(INFO, MOTOR, START, "rpm", (uint16_t)3200);
AX_KV(ERROR, COM, TIMEOUT, "bus", (uint8_t)1, "ms", (uint32_t)50);
```

**语义**：

- 带有命名参数的轻量级结构化日志。
- 键名在编译期被哈希为 16 位 ID；键名本身不进行传输。
- 有效载荷格式：`[key_hash_1 (2B)] [value_1 (typed)] [key_hash_2 (2B)] [value_2 (typed)] ...`
- 当事件语义稳定但需要通过参数名提高解码器可读性时非常有用。

---

## 7. Profile 控制

```c
#define AXIOM_PROFILE_DEV    0
#define AXIOM_PROFILE_FIELD  1
#define AXIOM_PROFILE_PROD   2

#ifndef AXIOM_PROFILE
#define AXIOM_PROFILE AXIOM_PROFILE_DEV
#endif
```

在包含 `axiomtrace.h` 之前设置 `AXIOM_PROFILE`：

```c
#define AXIOM_PROFILE AXIOM_PROFILE_PROD
#include "axiomtrace.h"
```

**不同 Profile 下的特性支持**：

| 特性             | DEV | FIELD | PROD |
|-----------------|-----|-------|------|
| `AX_LOG`        | 是  | 是     | 否   |
| `AX_EVT`        | 是  | 是     | 是   |
| `AX_PROBE`      | 是  | 是     | 否   |
| `AX_FAULT`      | 是  | 是     | 是   |
| `AX_KV`         | 是  | 是     | 是   |
| `DEBUG` 级别    | 是  | 否     | 否   |
| 断言 (Asserts)   | 是  | 是     | 否   |

---

## 8. 核心 API

### 8.1 初始化

```c
void axiom_init(void);
```

初始化内核环形缓冲区、序列计数器、级别掩码和丢弃计数器。必须在调用任何日志宏之前调用。

### 8.2 后端注册

```c
int axiom_backend_register(const axiom_backend_t *backend);
```

注册一个后端用于事件调度。成功返回 0，失败返回负值（例如达到最大后端数量）。

### 8.3 过滤器控制

```c
void axiom_level_mask_set(uint32_t mask);
uint32_t axiom_level_mask_get(void);

void axiom_module_mask_set(uint32_t mask);
uint32_t axiom_module_mask_get(void);
```

运行时过滤器控制。`mask` 是一个位字段，其中第 `N` 位对应于级别/模块 `N`。

### 8.4 刷新 (Flush)

```c
void axiom_flush(void);
```

显式地将环形缓冲区内容排出到所有已注册的后端。可选；后端也可以异步排出。

---

## 9. Port 层 API

所有 Port 层函数都提供 `__attribute__((weak))` 默认实现。通过在项目中提供强符号来覆盖它们。

```c
uint32_t axiom_port_timestamp(void);
void     axiom_port_critical_enter(void);
void     axiom_port_critical_exit(void);
void     axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                                const uint8_t *payload, uint8_t payload_len);

/* Flash 操作 (用于故障舱后端) */
int  axiom_port_flash_erase(uint32_t addr, uint32_t len);
int  axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);
```

---

## 10. 配置宏

| 宏 | 默认值 | 描述 |
|-------|---------|-------------|
| `AXIOM_RING_BUFFER_SIZE` | 4096 | 主环形缓冲区大小（字节） |
| `AXIOM_RING_BUFFER_POLICY` | `DROP` | `DROP` 或 `OVERWRITE` |
| `AXIOM_MAX_PAYLOAD_LEN` | 128 | 每个事件的最大有效载荷字节数 |
| `AXIOM_MODULE_MAX` | 32 | 过滤器位掩码支持的最大模块 ID 数（0 .. MAX-1） |
| `AXIOM_BACKEND_MAX` | 4 | 最大注册后端数量 |
| `AXIOMTRACE_VERSION_MAJOR` | 0 | 库主版本号 |
| `AXIOMTRACE_VERSION_MINOR` | 1 | 库次版本号 |
| `AXIOMTRACE_VERSION_PATCH` | 0 | 库补丁版本号 |
| `AXIOM_PROFILE` | `DEV` | `DEV`, `FIELD` 或 `PROD` |
| `AXIOM_CAPSULE_ENABLED` | 1 | 启用故障舱 |
| `AXIOM_CAPSULE_PRE_EVENTS` | 32 | 故障前窗口大小 |
| `AXIOM_CAPSULE_POST_EVENTS` | 16 | 故障后窗口大小 |

在包含 `axiomtrace.h` 之前覆盖：

```c
#define AXIOM_RING_BUFFER_SIZE 1024
#define AXIOM_PROFILE AXIOM_PROFILE_PROD
#include "axiomtrace.h"
```

---

## 11. 类型系统常量

### 11.1 同步字节 (Sync Byte)

```c
#define AXIOM_SYNC_BYTE 0xA5u
```

每个二进制帧起始处的固定同步字节。解码器使用此字节在字节流中定位帧边界。

### 11.2 报头长度 (Header Length)

```c
#define AXIOM_HEADER_LEN  8u  /* sync + version + level + module_id + event_id(2) + seq(2) */
```

### 11.3 CRC 长度

```c
#define AXIOM_CRC_LEN 2u  /* CRC-16/CCITT-FALSE 尾部字节 */
```

### 11.4 最大时间戳长度

```c
#define AXIOM_MAX_TIMESTAMP_LEN 5u  /* 变长编码：1–5 字节 */
```

### 11.5 有效载荷类型标签 (`axiom_type_t`)

```c
typedef enum {
    AXIOM_TYPE_BOOL      = 0x00,
    AXIOM_TYPE_U8        = 0x01,
    AXIOM_TYPE_I8        = 0x02,
    AXIOM_TYPE_U16       = 0x03,
    AXIOM_TYPE_I16       = 0x04,
    AXIOM_TYPE_U32       = 0x05,
    AXIOM_TYPE_I32       = 0x06,
    AXIOM_TYPE_F32       = 0x07,
    AXIOM_TYPE_TIMESTAMP = 0x08,
    AXIOM_TYPE_BYTES     = 0x09
    /* 0x0A–0x7F: 保留  |  0x80–0xFF: 用户自定义 */
} axiom_type_t;
```

每个标签是 1 字节的类型标记，前缀在二进制有效载荷中的值之前。原始 `#define` 常量（例如 `AXIOM_TYPE_U16`）仍然可用，以保持向后兼容。

### 11.6 编码器标签大小

```c
#define AXIOM_TAG_SIZE 1u  /* 所有类型标签恰好为 1 字节 */
```

编码器在溢出检查中使用此常量来计算类型标签字节的固定大小。

---

## 12. 后端错误码 (`axiom_backend_err_t`)

```c
typedef enum {
    AXIOM_BACKEND_OK       =  0,
    AXIOM_BACKEND_ERR_NULL = -1,  /* 空指针参数 */
    AXIOM_BACKEND_ERR_FULL = -2,  /* 注册表已满 */
    AXIOM_BACKEND_ERR_STRUCT = -3  /* 结构体损坏（无效的 .version） */
} axiom_backend_err_t;
```

原始整数返回值（`0`、`-1`、`-2`、`-3`）被保留，以保持源代码级向后兼容。
