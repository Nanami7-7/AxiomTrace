> [English](minimalist_architecture_analysis.md) | [简体中文](minimalist_architecture_analysis_zh.md)

# AxiomTrace 极简架构分析

## 三维结论 (Triple-Dimension Conclusions)

### 1. 应该立即砍掉的步骤

| 优先级 | 移除内容 | 理由 |
|----------|-------------------|-----------|
| **P0** | 将 YAML 字典作为固件构建依赖 | 增加构建复杂度，需要 Python 环境，与 MCU 开发环境脱节。 |
| **P0** | `axiom_events_generated.h` 代码生成 | 完全可以通过编译期的 C 宏 (X-Macro) 处理，无需外部工具。 |
| **P1** | 拆分为 7 个头文件 + 7 个 C 文件 | 过度工程化；对于 v0.1 版本，应合并为最小集合。 |
| **P1** | 将 CMake 作为唯一构建入口 | 应支持直接拷贝 .c/.h 到现有工程中编译。 |

**核心判断**：当前设计遵循“主机端工具链优先”的思维模式，但 MCU 开发者的真正痛点是“我不想安装 Python”。

---

### 2. 我们能否实现“零 YAML、零外部工具、零生成文件”？

**结论：可以，但需要权衡。**

#### 方案 A：纯 X-Macro 真值源（推荐作为默认层）

```c
/* axiom_events.h —— 开发者唯一需要编辑的事件定义文件 */
#ifndef AXIOM_EVENTS_H
#define AXIOM_EVENTS_H

/* 模块列表：X(module_name, module_id) */
#define AXIOM_MODULE_LIST(X) \
    X(MOTOR, 0x03)           \
    X(SENSOR, 0x05)

/* 事件列表：X(module, event_name, event_id, level, argc) */
#define AXIOM_EVENT_LIST(X)                                              \
    X(MOTOR, START,        0x01, INFO, 2)  /* rpm:u16, ts:ts */         \
    X(MOTOR, STOP,         0x02, INFO, 1)  /* reason:u8 */              \
    X(MOTOR, CURRENT_OVER, 0x03, WARN, 2)  /* phase:u8, current:i16 */  \
    X(SENSOR, TEMP_HIGH,   0x01, WARN, 2)  /* adc:u16, limit:i16 */

#endif
```

```c
/* axiom.h —— 核心头文件，包含所有代码 */
#ifndef AXIOM_H
#define AXIOM_H

#include <stdint.h>
#include <stddef.h>

/* 用户在包含此头文件前必须定义事件列表 */
#ifndef AXIOM_MODULE_LIST
#error "Define AXIOM_MODULE_LIST before including axiom.h"
#endif
#ifndef AXIOM_EVENT_LIST
#error "Define AXIOM_EVENT_LIST before including axiom.h"
#endif

/* ---- 通过 X-Macro 自动生成的模块 ID 宏 ---- */
#define _AXIOM_EXPAND_MODULE(name, id)  \
    static const uint8_t _AXIOM_MODULE_ID_##name = (id);

AXIOM_MODULE_LIST(_AXIOM_EXPAND_MODULE)

/* ---- 通过 X-Macro 自动生成的事件 ID 宏 ---- */
#define _AXIOM_EXPAND_EVENT(mod, name, id, level, argc)  \
    static const uint16_t _AXIOM_EVENT_ID_##mod##_##name = (id);

AXIOM_EVENT_LIST(_AXIOM_EXPAND_EVENT)

/* ---- 日志级别 ---- */
typedef enum {
    AXIOM_LEVEL_DEBUG = 0,
    AXIOM_LEVEL_INFO  = 1,
    AXIOM_LEVEL_WARN  = 2,
    AXIOM_LEVEL_ERROR = 3,
    AXIOM_LEVEL_FAULT = 4,
} axiom_level_t;

/* ---- 类型安全编码 (C11 _Generic) ---- */
static inline void _axiom_enc_u8 (uint8_t *b, uint8_t *p, uint8_t  v) { b[(*p)++] = 0x01; b[(*p)++] = v; }
static inline void _axiom_enc_i8 (uint8_t *b, uint8_t *p, int8_t   v) { b[(*p)++] = 0x02; b[(*p)++] = (uint8_t)v; }
static inline void _axiom_enc_u16(uint8_t *b, uint8_t *p, uint16_t v) { b[(*p)++] = 0x03; b[(*p)++] = v&0xFF; b[(*p)++] = v>>8; }
static inline void _axiom_enc_i16(uint8_t *b, uint8_t *p, int16_t  v) { b[(*p)++] = 0x04; b[(*p)++] = v&0xFF; b[(*p)++] = v>>8; }
static inline void _axiom_enc_u32(uint8_t *b, uint8_t *p, uint32_t v) { b[(*p)++] = 0x05; b[(*p)++] = v&0xFF; b[(*p)++] = (v>>8)&0xFF; b[(*p)++] = (v>>16)&0xFF; b[(*p)++] = v>>24; }
static inline void _axiom_enc_i32(uint8_t *b, uint8_t *p, int32_t  v) { b[(*p)++] = 0x06; /* ... */ }
static inline void _axiom_enc_f32(uint8_t *b, uint8_t *p, float    v) { b[(*p)++] = 0x07; /* memcpy */ }

#define _AXIOM_ENCODE_ONE(buf, pos, arg) \
    _Generic((arg), \
        uint8_t:  _axiom_enc_u8,  \
        int8_t:   _axiom_enc_i8,  \
        uint16_t: _axiom_enc_u16, \
        int16_t:  _axiom_enc_i16, \
        uint32_t: _axiom_enc_u32, \
        int32_t:  _axiom_enc_i32, \
        float:    _axiom_enc_f32  \
    )(buf, &(pos), arg)

/* ---- 变参计数宏 ---- */
#define _AXIOM_NARGS(...) _AXIOM_NARGS_IMPL(__VA_ARGS__, 8,7,6,5,4,3,2,1,0)
#define _AXIOM_NARGS_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N
#define _AXIOM_CAT(a,b) a##b

/* ---- 根据参数数量展开的日志宏 ---- */
#define _AXIOM_LOG_0(lvl, mod, evt) \
    axiom_write((lvl), _AXIOM_MODULE_ID_##mod, _AXIOM_EVENT_ID_##mod##_##evt, NULL, 0)

#define _AXIOM_LOG_1(lvl, mod, evt, a1) \
    do{ uint8_t _b[32]; uint8_t _p=0; _AXIOM_ENCODE_ONE(_b,_p,a1); \
        axiom_write((lvl), _AXIOM_MODULE_ID_##mod, _AXIOM_EVENT_ID_##mod##_##evt, _b, _p); }while(0)

/* ... 类似地省略 _LOG_2 到 _LOG_8 ... */

#define _AXIOM_LOG_DISPATCH(lvl, mod, evt, ...) \
    _AXIOM_CAT(_AXIOM_LOG_, _AXIOM_NARGS(__VA_ARGS__))(lvl, mod, evt, __VA_ARGS__)

#define AXIOM_DEBUG(mod,evt,...) _AXIOM_LOG_DISPATCH(AXIOM_LEVEL_DEBUG, mod, evt, __VA_ARGS__)
#define AXIOM_INFO(mod,evt,...)  _AXIOM_LOG_DISPATCH(AXIOM_LEVEL_INFO,  mod, evt, __VA_ARGS__)
#define AXIOM_WARN(mod,evt,...)  _AXIOM_LOG_DISPATCH(AXIOM_LEVEL_WARN,  mod, evt, __VA_ARGS__)
#define AXIOM_ERROR(mod,evt,...) _AXIOM_LOG_DISPATCH(AXIOM_LEVEL_ERROR, mod, evt, __VA_ARGS__)
#define AXIOM_FAULT(mod,evt,...) _AXIOM_LOG_DISPATCH(AXIOM_LEVEL_FAULT, mod, evt, __VA_ARGS__)

/* ---- Porting 层 (弱符号默认实现) ---- */
__attribute__((weak)) uint32_t axiom_timestamp(void) { return 0; }
__attribute__((weak)) void axiom_critical_enter(void) { }
__attribute__((weak)) void axiom_critical_exit(void) { }
__attribute__((weak)) void backend_send_byte(uint8_t b) { (void)b; }

/* ---- 前向声明 ---- */
void axiom_write(axiom_level_t lvl, uint8_t mod, uint16_t evt,
                 const uint8_t *payload, uint8_t len);

#endif /* AXIOM_H */
```

```c
/* axiom.c —— 单个 C 文件 */
#include "axiom.h"

#ifndef AXIOM_RING_SIZE
#define AXIOM_RING_SIZE 1024
#endif

static volatile uint8_t  _ring[AXIOM_RING_SIZE];
static volatile uint16_t _ring_head;
static volatile uint16_t _ring_tail;
static volatile uint16_t _seq;

static void _ring_put(const uint8_t *data, uint8_t len) {
    axiom_critical_enter();
    for (uint8_t i = 0; i < len; i++) {
        _ring[_ring_head] = data[i];
        _ring_head = (_ring_head + 1) % AXIOM_RING_SIZE;
        if (_ring_head == _ring_tail) {
            _ring_tail = (_ring_tail + 1) % AXIOM_RING_SIZE; /* 覆盖模式 */
        }
    }
    axiom_critical_exit();
}

void axiom_write(axiom_level_t lvl, uint8_t mod, uint16_t evt,
                 const uint8_t *payload, uint8_t len) {
    /* 历史直接写入草图；当前 core 默认使用 AXIOM_SHORT_CS。 */
    axiom_critical_enter();
    uint16_t total_len = 8 + 1 + len + 2; /* 报头 + 长度 + 有效载荷 + CRC (为简洁忽略变长时间戳) */
    if (_ring_has_space(total_len)) {
        uint16_t pos = _ring_acquire(total_len);
        _ring_put_byte_at(pos++, 0xA5);
        _ring_put_byte_at(pos++, 0x10);
        /* ... 直接写入其他字段并同步计算增量 CRC ... */
        _ring_commit(total_len);
    }
    axiom_critical_exit();
}

/* 后端调度：最简单的轮询实现 */
void axiom_flush(void) {
    axiom_critical_enter();
    while (_ring_tail != _ring_head) {
        backend_send_byte(_ring[_ring_tail]);
        _ring_tail = (_ring_tail + 1) % AXIOM_RING_SIZE;
    }
    axiom_critical_exit();
}
```

```c
/* main.c —— 开发者代码 */
#define AXIOM_MODULE_LIST(X) \
    X(MOTOR, 0x03)

#define AXIOM_EVENT_LIST(X) \
    X(MOTOR, START, 0x01, INFO, 1)

#include "axiom.h"

/* 覆盖弱符号：提供实际输出 */
void backend_send_byte(uint8_t b) {
    UART_DR = b; /* 伪代码 */
}

int main(void) {
    uint16_t rpm = 3200;
    AXIOM_INFO(MOTOR, START, rpm);  /* ← 这条是第一条日志 */
    axiom_flush();
    return 0;
}
```

#### 纯宏方案的代价 (Cost of the Pure Macro Solution)

| 方面 | 代价 | 严重性 |
|--------|------|----------|
| **字典导出** | 主机端解码器需要事件元数据。纯 C 宏无法自描述文本模板，需要额外工具从源码中提取。 | **高** |
| **ID 冲突检测** | 编译期无法检测跨模块的 event_id 冲突（不同模块可以重复），需要链接期或外部检查。 | 中 |
| **参数类型校验** | `_Generic` 只能校验 C 类型，无法校验“开发者声称的模板类型”与实际编码是否匹配。 | 中 |
| **ROM 占用** | `static const` 模块/事件 ID 会进入 ROM，但可以被编译器优化为立即数。 | 低 |
| **可维护性** | X-Macro 语法对初学者不友好；错误信息可能晦涩难懂。 | 中 |

**关键洞察**：纯 C 宏方案在 **固件侧** 完全可行，但 **主机侧解码器** 仍需要某种形式的字典。解决方案是：让字典从固件源码中“反向生成”，而不是“前向驱动”固件编译。

---

### 3. 开发者理想的第一条日志代码

```c
#include "axiom.h"

/* 可选：一行式配置 */
#pragma AXIOM_MODULE(MOTOR, 0x03)
#pragma AXIOM_EVENT(MOTOR, START, INFO, "rpm={u16}")

int main(void) {
    AXIOM_LOG("MOTOR_START", INFO, "rpm=%u", 3200);  /* 理想状态 */
    return 0;
}
```

**但是 `AXIOM_LOG("MOTOR_START", INFO, "rpm=%u", 3200)` 在技术上是行不通的**，因为：

- `"MOTOR_START"` 和 `"rpm=%u"` 编译后会变成 ROM 中的字符串常量，违反“固件内无字符串”原则。
- 运行时解析格式字符串需要 `printf` 级别的代码量。

**折中方案 (推荐)**：

```c
AXIOM_INFO(MOTOR, START, (uint16_t)3200);  /* 字符串不进入固件，且类型安全 */
```

如果坚持要有类似 `printf` 的体验，则必须在编译期解析格式字符串。这需要：

- 自定义编译器插件 (Clang 插件) —— 不通用。
- 或 C++20 `consteval` —— 不是 C11。
- 或外部代码生成器 —— 违反“零工具”目标。

**因此结论**：在 C11 + 零外部工具的约束下，`AXIOM_INFO(MOTOR, START, rpm)` 是可实现的最简接口。格式模板保留在主机侧字典中。

---

## 渐进式复杂度分层设计 (Progressive Complexity Layering Design)

```
┌─────────────────────────────────────────────────────────────┐
│ 第 3 层：高级控制 (可选)                                     │
│  - YAML 字典 + Python 代码生成                               │
│  - 针对大团队的精细化模块/事件管理                           │
│  - 为解码器生成 dictionary.json                              │
├─────────────────────────────────────────────────────────────┤
│ 第 2 层：结构化定义 (推荐)                                   │
│  - 手动编辑 axiom_events.h (X-Macro)                         │
│  - 固件编译零外部依赖                                        │
│  - 提供脚本从 X-Macro 中提取字典                             │
├─────────────────────────────────────────────────────────────┤
│ 第 1 层：极简模式 (默认)                                     │
│  - 2 个文件：axiom.h + axiom.c                               │
│  - 直接硬编码 module_id / event_id                          │
│  - 适用于 POC、快速验证、小项目                             │
└─────────────────────────────────────────────────────────────┘
```

### 第 1 层代码示例 (极简)

```c
/* axiom_minimal.h */
#pragma once
#include <stdint.h>

#define AXIOM_INFO(mid, eid, ...) \
    _axiom_log(1, (mid), (eid), __VA_ARGS__)

void _axiom_log(uint8_t lvl, uint8_t mod, uint16_t evt, ...);
void axiom_flush(void);
```

```c
/* user.c */
#include "axiom_minimal.h"

int main(void) {
    AXIOM_INFO(0x03, 0x01, (uint16_t)3200);  /* 硬编码 ID */
    axiom_flush();
}
```

---

## 最小文件集论证 (Minimal File Set Argument)

| 方案 | 文件数量 | 构成 | 适用场景 |
|----------|------------|-------------|----------|
| **极端极简** | 2 | `axiom.h` + `axiom.c` | 快速验证、教学、超小项目。 |
| **推荐默认** | 3 | `axiom.h` + `axiom.c` + `axiom_events.h` (+ 短临界区编码) | 需要模块/事件管理的正式项目。 |
| **当前设计** | 14+ | 7 个 .h + 7 个 .c + 各类 backend | 分拆过度，应推迟到 v0.5+。 |

**3 文件方案的内容分配**：

1. **`axiom_events.h`** —— 用户编辑：模块和事件定义 (X-Macro)。
2. **`axiom.h`** —— 库提供：所有宏、类型、内联函数、弱符号默认实现。
3. **`axiom.c`** —— 库提供：环形缓冲区、编码、CRC、后端调度。

---

## 最终建议

1. **将默认路径改为“零 YAML、零工具”**：使用 X-Macro 作为真值源；固件编译不依赖 Python。
2. **反向字典生成**：提供可选的 `extract_dict.py`，从 `axiom_events.h` 中提取 JSON 供解码器使用。这是 CI/发布步骤，而非构建依赖。
3. **文件合并**：在 v0.1~v0.2 阶段，将核心代码压缩为 `axiom.h` + `axiom.c`。后端作为独立文件提供但不强制要求使用。
4. **接口锁定**：锁定 `AXIOM_INFO(MODULE, EVENT, args...)` 作为稳定 API；内部实现可以迭代。
5. **保留高级路径**：保留 YAML + 代码生成作为第 3 层，供需要精细控制的大型项目使用。
