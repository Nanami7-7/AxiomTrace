> [English](static_analysis.md) | [简体中文](static_analysis_zh.md)

# AxiomTrace 质量保证与静态分析设计文档

> **版本**：v1.0-草案  
> **角色**：质量保证设计师 (Quality Assurance Designer)  
> **目标**：为 AxiomTrace MCU 裸机结构化日志内核建立“卓越静态分析”体系，涵盖编译期断言、静态分析、CI/CD、测试金字塔、代码规范及文档规范。

---

## 目录

1. [三层静态分析体系](#1-三层静态分析体系)
2. [CI/CD 流水线](#2-ci-cd-流水线)
3. [测试金字塔](#3-测试金字塔)
4. [代码质量标准](#4-代码质量标准)
5. [文档标准](#5-文档标准)
6. [总结：实现卓越静态分析的路径](#6-总结实现卓越静态分析的路径)

---

## 1. 三层静态分析体系

### 1.1 L1 — 编译期断言 (`_Static_assert` + 自定义宏)

AxiomTrace 要求 **零运行时开销** 的约束校验。所有与 ABI、内存布局及配置值相关的约束必须在编译期强制执行。

#### 1.1.1 现有基础

项目已提供 `baremetal/include/axiom_static_assert.h`：

```c
#define AXIOM_STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)
#define AXIOM_CHECK_SIZE(type, expected)      ...
#define AXIOM_CHECK_ALIGN(type, align)        ...
#define AXIOM_CHECK_FIELD_OFFSET(type, field, expected) ...
#define AXIOM_CHECK_RANGE(val, min, max)      ...
```

#### 1.1.2 完整 L1 宏列表及应用场景

| 宏名称 | 用途 | 应用场景 |
|------------|---------|----------|
| `AXIOM_STATIC_ASSERT(expr, msg)` | 通用 C11 编译期断言 | 所有编译期检查的基础宏 |
| `AXIOM_CHECK_SIZE(type, expected)` | 冻结结构体/联合体大小 | `axiom_event_header_t` 必须精确为 8 字节；`axiom_backend_desc_t` 大小在主版本内冻结 |
| `AXIOM_CHECK_ALIGN(type, align)` | 校验对齐要求 | 确保环形缓冲区元素满足 4 字节对齐（利于 DMA） |
| `AXIOM_CHECK_FIELD_OFFSET(type, field, expected)` | 冻结字段偏移 | `axiom_event_header_t` 字段偏移必须符合有线格式 (spec/event_model.md) |
| `AXIOM_CHECK_RANGE(val, min, max)` | 配置值的范围检查 | `AXIOM_RING_BUFFER_SIZE` 必须是 2 的幂；`AXIOM_MAX_PAYLOAD_LEN <= 255` |
| `AXIOM_CHECK_POWER_OF_TWO(val)` | 2 的幂检查 | 环形缓冲区大小、故障舱 (Capsule) 缓冲区大小 |
| `AXIOM_CHECK_SAME_TYPE(a, b)` | 类型一致性 | 确保 `_Generic` 分支与 dictionary 定义的 wire v2 值编码对应 |
| `AXIOM_CHECK_ARRAY_SIZE(arr, expected)` | 冻结数组长度 | CRC 查找表必须为 256 项；后端注册表容量 |
| `AXIOM_CHECK_POINTER_SIZE(expected)` | 指针大小校验 | 确保满足 32 位 MCU 假设（排除 8/16 位） |
| `AXIOM_CHECK_BIT_WIDTH(type, bits)` | 整数位宽校验 | `uint32_t` 必须精确为 32 位 (`sizeof(type) * CHAR_BIT == bits`) |
| `AXIOM_CHECK_NO_MALLOC_FN(fn)` | 禁止 malloc 检查 | 关键函数指针类型的静态断言（通过宏封装实现） |

#### 1.1.3 L1 使用示例（建议包含在源文件中）

```c
/* axiom_event.h — 建议在现有基础上扩展 */
AXIOM_CHECK_SIZE(axiom_event_header_t, 8);
AXIOM_CHECK_ALIGN(axiom_event_header_t, 1);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, sync,       0);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, version,    1);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, level,      2);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, module_id,  3);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, event_id,   4);
AXIOM_CHECK_FIELD_OFFSET(axiom_event_header_t, seq,        6);

/* axiom_config.h — 配置值范围及 2 的幂检查 */
AXIOM_CHECK_RANGE(AXIOM_MAX_PAYLOAD_LEN, 1, 255);
AXIOM_CHECK_POWER_OF_TWO(AXIOM_RING_BUFFER_SIZE);
AXIOM_CHECK_RANGE(AXIOM_CAPSULE_PRE_EVENTS,  0, 256);
AXIOM_CHECK_RANGE(AXIOM_CAPSULE_POST_EVENTS, 0, 256);

/* axiom_crc.c —— 查找表大小冻结 */
AXIOM_CHECK_ARRAY_SIZE(crc16_table, 256);

/* axiom_port.h / 移植实现 — 32 位假设 */
AXIOM_CHECK_POINTER_SIZE(4);
AXIOM_CHECK_BIT_WIDTH(uint32_t, 32);
AXIOM_CHECK_BIT_WIDTH(uint16_t, 16);
AXIOM_CHECK_BIT_WIDTH(uint8_t,   8);
```

#### 1.1.4 补充 L1 宏实现

建议添加到 `axiom_static_assert.h`：

```c
#define AXIOM_CHECK_POWER_OF_TWO(val) \
    AXIOM_STATIC_ASSERT(((val) != 0) && (((val) & ((val) - 1)) == 0), \
                        #val " must be a power of two")

#define AXIOM_CHECK_SAME_TYPE(a, b) \
    AXIOM_STATIC_ASSERT(__builtin_types_compatible_p(typeof(a), typeof(b)), \
                        #a " and " #b " must be the same type")

#define AXIOM_CHECK_ARRAY_SIZE(arr, expected) \
    AXIOM_STATIC_ASSERT((sizeof(arr) / sizeof((arr)[0])) == (expected), \
                        "Array " #arr " size mismatch")

#define AXIOM_CHECK_POINTER_SIZE(expected) \
    AXIOM_STATIC_ASSERT(sizeof(void*) == (expected), \
                        "Pointer size mismatch: expected " #expected)

#define AXIOM_CHECK_BIT_WIDTH(type, bits) \
    AXIOM_STATIC_ASSERT(sizeof(type) * CHAR_BIT == (bits), \
                        #type " bit width mismatch")
```

> **注意**：`__builtin_types_compatible_p` 是 GCC/Clang 扩展；IAR/Keil 的兼容层可以回退到运行时断言或通过 `#ifdef` 跳过。

---

## 2. CI/CD 流水线

### 2.1 GitHub Actions 工作流概览

设计一个主工作流 `.github/workflows/ci.yml`，包含以下并行/串行任务：

```
┌─────────────────────────────────────────────────────────────┐
│                        CI 流水线                             │
├─────────────────────────────────────────────────────────────┤
│  阶段 1: 快速反馈 (Fast Feedback)                            │
│    ├── lint-commit-msg    (提交信息格式)                     │
│    ├── lint-clang-format  (代码格式化)                       │
│    └── lint-clang-tidy    (快速静态分析扫描)                 │
├─────────────────────────────────────────────────────────────┤
│  阶段 2: 构建矩阵 (Build Matrix)                             │
│    ├── build-host-gcc     (x86_64-linux-gnu)                │
│    ├── build-host-clang   (x86_64-linux-gnu)                │
│    ├── build-arm-none-eabi-gcc (Cortex-M4, thumb)           │
│    └── build-arm-none-eabi-gcc (Cortex-M7, thumb/fpu)       │
├─────────────────────────────────────────────────────────────┤
│  阶段 3: 深度分析与测试                                      │
│    ├── static-scan-build    (路径敏感分析)                   │
│    ├── static-cppcheck      (跨函数分析)                     │
│    ├── test-host-unit       (Unity 单元测试)                 │
│    ├── test-golden-frame    (Golden Frame 对比)              │
│    ├── test-conformance     (Python 一致性测试)              │
│    └── test-fuzz-smoke      (libFuzzer 冒烟测试, 60s)        │
├─────────────────────────────────────────────────────────────┤
│  阶段 4: 质量门禁 (Quality Gate)                             │
│    ├── coverage-gcov        (代码覆盖率, 阈值 85%)           │
│    ├── benchmark-smoke      (基准测试冒烟测试)               │
│    └── docs-build           (Doxygen + MkDocs)               │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 测试金字塔

### 3.1 金字塔结构

```
         /\
        /  \   模糊测试 (libFuzzer)      — 探索性测试, 边界情况
       /____\
      /      \  一致性测试 (Python)      — 严格遵守有线格式规范
     /________\
    /          \ Golden Frame 测试       — 字节级二进制对比
   /____________\
  /              \ 基准测试 (Cycles/RAM/ROM) — 回归检测
 /________________\
/                  \
主机单元测试 (Unity) — 编码器、解码器、CRC、环形缓冲区逻辑
```

### 3.2 主机单元测试 (Unity 框架)

#### 3.2.1 目录结构

```
tests/
├── host/
│   ├── CMakeLists.txt
│   ├── test_ring_buffer.c      # 无锁环形缓冲区测试
│   ├── test_encode_decode.c    # 编码器/解码器对称性测试
│   ├── test_crc.c              # CRC-16/CCITT 查找表校验
│   ├── test_capsule.c          # 故障舱状态机测试
│   ├── test_filter.c           # 级别/模块掩码过滤测试
│   ├── test_backend_dispatch.c # 后端调度与优先级测试
│   └── unity/                  # Unity 框架源码
├── conformance/
│   ├── requirements.txt
│   ├── test_wire_format.py     # 帧格式一致性
│   └── test_golden_frames.py   # Golden frame 对比
├── fuzz/
│   ├── CMakeLists.txt
│   └── fuzz_encode.c           # libFuzzer 目标
├── golden/
│   ├── frame_01_minimal.bin    # 最小帧 (0 有效载荷)
│   ├── frame_02_u8_u16.bin     # 混合类型有效载荷
│   ├── frame_03_full_payload.bin # 255 字节有效载荷
│   └── expected/
│       ├── frame_01.json
│       ├── frame_02.json
│       └── frame_03.json
└── benchmark/
    ├── CMakeLists.txt
    ├── bench_encode.c          # 编码吞吐量
    ├── bench_ring.c            # 环形缓冲区吞吐量
    └── bench_crc.c             # CRC 计算速度
```

---

## 4. 代码质量标准

### 4.1 代码审查清单 (Checklist)

每位审查者 (Reviewer) 在批准 PR 前必须确认：

#### 功能正确性
- [ ] 新功能具有对应的单元测试覆盖。
- [ ] Golden Frame 测试通过（如果修改了编码器）。
- [ ] 一致性测试通过（如果修改了有线格式）。

#### 静态分析
- [ ] `clang-tidy` 零警告（处于 `WarningsAsErrors` 级别）。
- [ ] `clang-format` 无差异。
- [ ] `cppcheck` 无错误。
- [ ] `scan-build` 无高/中风险错误。

#### 约束合规性
- [ ] 热路径（Hot path）无 `malloc` / `calloc` / `realloc` / `free`。
- [ ] 热路径无 `printf` / `sprintf` / `fprintf`。
- [ ] `axiom_log_write` 调用链中无阻塞调用（如 `sleep`, `sem_wait`）。
- [ ] 新的 `_Static_assert` 覆盖了 ABI / 布局 / 配置约束。

#### 性能与资源
- [ ] 认知复杂度 ≤ 15 (`readability-function-cognitive-complexity`)。
- [ ] 函数行数 ≤ 80，语句数 ≤ 100。
- [ ] 新的全局变量经过论证且已记录。
- [ ] 指针算术限制在同一数组内。

#### 兼容性与文档
- [ ] 已考虑 IAR/Keil 兼容层（或标记为仅限 GCC/Clang）。
- [ ] 公共 API 具有 Doxygen 注释。
- [ ] 已根据 SemVer 更新版本号（如果影响 ABI）。
- [ ] `CHANGELOG.md` 已更新。

---

## 5. 文档标准

### 5.1 API 文档 (Doxygen)

**注释模板**：

```c
/**
 * @brief 将结构化日志事件写入环形缓冲区。
 *
 * 这是主要的路径入口点。它不执行任何动态内存分配、不进行格式化，
 * 也不涉及阻塞式 I/O。
 *
 * @param level      日志严重级别 (DEBUG..FAULT)。
 * @param module_id  模块标识符 (0..255)。
 * @param event_id   模块内事件标识符。
 * @param payload    原始有效载荷字节（如果 @p payload_len 为 0，则可以为 NULL）。
 * @param payload_len 有效载荷字节长度 (0..255)。
 *
 * @pre payload_len <= AXIOM_MAX_PAYLOAD_LEN
 * @pre axiom_backend_init_all() 已被调用。
 *
 * @note 中断安全 (IRQ-safe)。可以从中断上下文中调用。
 */
void axiom_log_write(axiom_level_t level,
                     uint8_t module_id,
                     uint16_t event_id,
                     const uint8_t *payload,
                     uint8_t payload_len);
```

---

## 6. 总结：实现卓越静态分析的路径

AxiomTrace 的“卓越静态分析”通过 **三层防护 + 自动流水线 + 测试金字塔** 实现：

### 第一层：编译期断言 (L1) — 零运行时开销的绝对约束

- 利用 C11 `_Static_assert` 和 GCC 扩展，在 **编译阶段** 冻结所有 ABI 契约：结构体大小、字段偏移、对齐方式、配置值范围以及指针宽度。
- 任何违反 ABI 或配置约束的代码都 **无法通过编译**，从根本上杜绝了因布局变化导致的有线格式不兼容。
- 专门针对 MCU 场景增加了 2 的幂检查、数组长度冻结及类型一致性校验，确保环形缓冲区、CRC 表、后端注册表等核心数据结构在编译期即被验证。

### 第二层：编译期分析 (L2) — `clang-tidy` 的深度规则覆盖

- 启用 `bugprone-*`、`clang-analyzer-*`、`cppcoreguidelines-*`、`performance-*`、`portability-*` 以及 `readability-*` 类别，将 **路径敏感分析、性能模式及移植性问题** 引入编译期反馈循环。
- 强制执行 **认知复杂度 ≤ 15** 以及 **带前缀的命名规范** (`axiom_` / `AXIOM_`)，确保代码在可读性和可维护性上达到工业级标准。

### 第三层：跨函数分析 (L3) — `cppcheck` + `scan-build` 的全局视角

- `cppcheck` 补充了跨翻译单元的分析，能够检测出单文件 `clang-tidy` 可能遗漏的 **未初始化变量传递、资源泄漏及冗余条件**。
- `scan-build` 生成路径敏感的 HTML 报告，对 `axiom_log_write` → `axiom_backend_dispatch` 等跨单元调用链进行深度溯源分析。

### 最终目标

> **任何违反“热路径零 malloc/printf/阻塞”约束、破坏 ABI 兼容性、引入未定义行为或降低代码可读性的变更，都将无法通过编译、无法通过静态分析、无法通过 CI 门禁，且不得进入主分支。**

这就是 AxiomTrace 实现“卓越静态分析”的路径。
