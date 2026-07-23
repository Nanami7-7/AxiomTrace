> [English](README.md) | **简体中文**

# AxiomTrace 1.0

AxiomTrace 是面向裸机 C 固件的、有明确资源上界、无动态分配的 Wire v2 事件记录器。固件只记录 ID 与紧凑值；主机工具负责 CRC 校验，并将帧转换为 raw JSON 或由字典驱动的文本/JSON。Core ring 默认在容量不足时丢弃新帧，恢复后通过 `DROP_SUMMARY` 报告。

## 支持能力

- C11 `AX_EVT`、`AX_LOG`、`AX_PROBE`、`AX_FAULT`、`AX_KV` 前端。
- 带 CRC 的 Wire v2 帧、过滤、丢失诊断、Memory 与 Deferred Backend。
- Capsule v1 的故障前/故障/故障后捕获，以及显式分段 Flash 提交。
- 模块化 CMake 库和生成的 `axiomtrace.h` 单头文件制品。
- 无元数据 raw 解码；可选 JSON/X-Macro 字典、codegen、metadata identity 与 bundle 工作流。

正式编译器契约为 GNU11 模式下的 GCC 或 Clang。Host GCC/Clang 是测试平台；ARM GNU 与 RISC-V GNU 是仅编译 reference 检查。IAR、MSVC、Arm Compiler、厂商 Port 和真实 MCU 时序在集成项目单独测量前均为实验性。

## 快速开始

生成发布头文件：

```sh
python tool/scripts/amalgamate.py
```

仅在一个实现翻译单元中定义实现：

```c
/* axiomtrace_impl.c */
#define AXIOMTRACE_IMPLEMENTATION
#include "axiomtrace.h"
```

应用代码注册 Backend、发事件，并将 `axiom_flush()` 作为常规刷新入口：

```c
#include "axiomtrace.h"

static uint8_t trace[128];
static axiom_memory_backend_ctx_t memory_context;

int main(void) {
    axiom_backend_t memory =
        axiom_backend_memory("trace", trace, sizeof(trace), &memory_context);

    axiom_init();
    if (axiom_backend_register(&memory) != AXIOM_BACKEND_OK) return 1;
    AX_EVT(INFO, 0x03u, 0x0001u, (uint16_t)3200u);
    axiom_flush();

    return memory_context.head >= 12u ? 0 : 2;
}
```

自行提供 `axiom_port_*` 时，在实现 TU 定义 `AXIOMTRACE_NO_DEFAULT_PORT`；否则单头文件包含 generic weak defaults。

仓库示例会以十六进制打印一个完整帧：

```sh
cmake -S . -B build -DAXIOM_BUILD_TESTS=ON -DAXIOM_BUILD_EXAMPLES=ON
cmake --build build
./build/baremetal/examples/example_minimal
```

安装 Python 工具后，无需字典即可结构化解码二进制 trace：

```sh
axiom-decoder trace.bin --format raw
```

推荐按需逐层增加能力：

1. 原始 ID + raw decode，完成最小固件集成。
2. 提供手写 JSON 字典，或从 X-Macro 提取字典。
3. 仅在需要时启用 codegen、metadata identity、bundle 校验和源码定位。

## CMake 构建与消费

```sh
cmake -S . -B build -DAXIOM_BUILD_TESTS=ON -DAXIOM_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix install
```

消费者可以使用 `add_subdirectory()`，也可以使用安装后的包：

```cmake
find_package(AxiomTrace 1.0 CONFIG REQUIRED)
target_link_libraries(firmware PRIVATE AxiomTrace::axiomtrace)
```

AxiomTrace 作为子项目时，tests 与 examples 默认关闭。普通固件构建不依赖 Python、YAML、codegen 或 bundle。

### Port 选择

顶层构建只选择架构级 Port：

```sh
cmake -S . -B build-cortex-m -DAXIOM_PLATFORM=cortex-m \
  -DAXIOM_BUILD_TESTS=OFF -DAXIOM_BUILD_EXAMPLES=OFF
```

`AXIOM_SOC` 与 `AXIOM_BOARD` 不再是顶层选项。依赖 SDK 的适配分别位于 `baremetal/ports/stm32`、`baremetal/ports/nrf52` 和 `baremetal/ports/esp32`，具体依赖边界见[移植指南](docs/reference/porting_guide.md)。

## 资源预设

各 preset 的 RAM/Flash/stack 报告属于发布制品；下表配置容量不等同于整个固件的最终链接成本。

| Preset | Core ring | Payload | Capsule | 目标用途 |
| --- | ---: | ---: | --- | --- |
| `tiny` | 256 B | 32 B | 关闭 | 最小量产集成 |
| `prod` | 1 KiB | 64 B | 关闭 | 量产固件 |
| `field` | 2 KiB | 96 B | 2 KiB 帧历史 | 现场服务版本 |
| `dev` | 4 KiB | 128 B | 4 KiB 帧历史 | Host/开发版本 |
| `custom` | 项目定义 | 项目定义 | 项目定义 | 显式调优 |

ARM GNU MinSizeRel 发布预算为 Tiny RAM ≤512 B、Prod ≤1.5 KiB、Field ≤4.5 KiB、Dev ≤9 KiB。不能用 Host 测量代替目标测量。

## 文档

- [API 参考](spec/api_reference_zh.md)
- [Wire 格式](spec/wire_format_zh.md)
- [Backend 契约](spec/backend_contract_zh.md)
- [Fault Capsule](spec/fault_capsule_zh.md)
- [工具链设计](spec/toolchain_ecosystem_design_zh.md)
- [移植指南](docs/reference/porting_guide.md)
- [变更日志](docs/changelog/CHANGELOG_zh.md)

## 许可证

[GNU General Public License v3.0](LICENSE)
