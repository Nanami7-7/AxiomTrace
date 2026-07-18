# AxiomTrace 移植指南

> 适用于 AxiomTrace 1.0。公共 Port 接口位于 `baremetal/port/axiom_port.h`。

## 1. 架构边界

AxiomTrace 的仓库内核按职责分为五个平面：

```text
frontend/  ->  core/  ->  backend/
                 ^          |
                 |          +--> downstream transport
                 +-- port/  +--> vendor port package
```

- `frontend/`：`AX_EVT`、`AX_LOG`、`AX_PROBE`、`AX_FAULT`、`AX_KV` 宏。
- `core/`：事件帧、编码、Ring、过滤、时间戳、诊断和 Fault Capsule。
- `backend/`：Backend 契约、Memory Backend、Deferred Backend。
- `port/`：稳定的 `axiom_port_*` 接口声明。
- `ports/`：Host 默认实现、架构实现和需要外部 SDK 的厂商包。

顶层 CMake 只负责选择架构 Port；它不猜测 SoC 或开发板，也不把厂商 SDK 混入核心库。

## 2. Port 目录约定

```text
baremetal/ports/
├── CMakeLists.txt              # host/cortex-m/riscv 选择器
├── generic/                    # Host 和默认弱符号实现
├── arch/
│   ├── cortex-m/               # Cortex-M 架构实现
│   └── riscv/                  # RISC-V 架构实现
├── stm32/                      # STM32 + UART 外部包
├── nrf52/                      # nRF52 + SEGGER RTT 外部包
└── esp32/                      # ESP-IDF Component 外部包
```

`soc/`、`board/` 不再作为空的占位目录提交。新的 SoC 或开发板适配应放在实际需要 SDK/链接配置的厂商包中，并用 README 说明依赖。

## 3. 顶层 CMake 选项

| 选项 | 可选值 | 作用 |
| --- | --- | --- |
| `AXIOM_PLATFORM` | `host`、`cortex-m`、`riscv` | 选择一个架构级 Port；默认按 `CMAKE_SYSTEM_PROCESSOR` 自动检测 |
| `AXIOM_PRESET` | `custom`、`tiny`、`prod`、`field`、`dev` | 选择资源预设 |
| `AXIOM_BUILD_TESTS` | `ON`/`OFF` | 构建 Host 测试 |
| `AXIOM_BUILD_EXAMPLES` | `ON`/`OFF` | 构建 `baremetal/examples` |

`AXIOM_SOC` 和 `AXIOM_BOARD` 不是顶层项目的选项。厂商 Port 自己处理 SDK、芯片和板级配置，避免“配置成功但实际仍使用 generic Port”的隐式回退。

## 4. 构建方式

Host 默认使用 generic Port：

```sh
cmake -S . -B build -G Ninja \
  -DAXIOM_BUILD_TESTS=ON -DAXIOM_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

交叉编译时显式指定架构和工具链：

```sh
cmake -S . -B build-cortex-m -G Ninja \
  -DAXIOM_PLATFORM=cortex-m \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake \
  -DAXIOM_BUILD_TESTS=OFF -DAXIOM_BUILD_EXAMPLES=OFF
cmake --build build-cortex-m
```

## 5. 使用厂商 Port 包

厂商包要求核心目标已经存在，并通过 `AxiomTrace::axiomtrace` 继承公共头文件和编译设置。以 STM32 为例：

```cmake
add_subdirectory(path/to/AxiomTrace axiomtrace-core)
add_subdirectory(path/to/AxiomTrace/baremetal/ports/stm32 axiomtrace-stm32)
target_link_libraries(firmware PRIVATE axiom_trace_stm32)
```

STM32 包需要工程提供芯片启动代码、HAL/寄存器环境和最终链接脚本；它不会在仓库内伪造这些依赖。nRF52 包同样需要 SEGGER RTT 头文件和实现：

```cmake
add_subdirectory(path/to/AxiomTrace axiomtrace-core)
add_subdirectory(path/to/AxiomTrace/baremetal/ports/nrf52 axiomtrace-nrf52)
target_link_libraries(firmware PRIVATE axiom_port_nrf52)
```

ESP32 包是 ESP-IDF Component，应复制/纳入 Component 目录，由 `idf_component_register()` 提供 `esp_timer`、FreeRTOS 和 UART 依赖；不能用普通 Host CMake 配置它。

## 6. 添加新的架构 Port

1. 在 `baremetal/ports/arch/<name>/` 添加 `axiom_port_<name>.c`。
2. 实现以下基础接口：

   ```c
   uint32_t axiom_port_timestamp(void);
   void axiom_port_critical_enter(void);
   void axiom_port_critical_exit(void);
   ```

3. 按需实现字符串输出、Fault Hook、快照和 Flash 接口；不需要的接口可以返回默认失败值。
4. 在 `baremetal/ports/CMakeLists.txt` 增加一个明确的 `AXIOM_PLATFORM` 分支，并确认源文件存在。
5. 不要为单个源文件再创建未被父级调用的 CMake 包装文件。

时间戳必须单调递增且允许 32 位自然回绕；临界区必须成对、可嵌套或明确禁止嵌套；Port 热路径不能分配堆内存、阻塞等待或擦写 Flash。

## 7. 添加新的厂商/板级包

当适配需要 SDK 头文件、UART/RTT 驱动、启动代码或链接脚本时，在 `baremetal/ports/<vendor>/` 建立独立包：

- `CMakeLists.txt` 只声明真实存在的源文件和依赖；
- `README.md` 记录 SDK 版本、必需符号、初始化顺序和链接要求；
- 包目标通过 `AxiomTrace::axiomtrace` 依赖核心库；
- 不修改核心 `CMakeLists.txt` 来猜测具体开发板；
- 在目标工具链/SDK 工程中验证，Host CTest 不冒充硬件验证。

## 8. 验证清单

- Host：`cmake --build`、`ctest --test-dir build --output-on-failure`。
- Python 工具：`uv run --project tool --extra test python -m pytest -q`。
- 单头文件：运行 `tool/scripts/amalgamate.py` 并编译单 implementation TU。
- 文档与版本：运行 `python scripts/release_checks.py`。
- 目标平台：使用真实交叉编译器检查启动、Port、链接脚本和资源预算。

最后一次修改 Port 后，应同步更新 `docs/reference/DIR_STRUCTURE.md`、README 的平台说明和 `docs/changelog/CHANGELOG*.md`。
