# 版本映射

本文档记录 AxiomTrace 每个版本对应的 Git 提交范围。可用于快速定位问题或了解各版本包含的变更。

> **注意**: v0.5.0-probe 已跳过，探针时序分析功能推迟到后续版本。

| 版本 | 日期 | 提交范围 | 关键变更 |
|------|------|----------|----------|
| v0.7.0 | 2026-05-30 | 4a58af7 ~ f0e4c7f | Wire v2、元数据包、CI 脚本、架构重构 |
| v0.6.0 | 2026-05-27 | f11a10a ~ c2914c0 | 文档同步、代码质量、MCU 基准测试、Wire v2 载荷 |
| v0.5.0-probe | — | — | **已跳过**（探针时序分析推迟） |
| v0.4.0 | 2026-05-07 | 97cdd7e ~ fcd9a5b | 后端 API、ring consume、并发修复 |
| v0.3.0 | 2026-05-02 | 363aa01 ~ 5adb1f1 | 时间戳修复 (0xFE)、ring 2 次幂断言 |
| v0.2.0 | 2026-05-01 | 227381f ~ cde0f4c | CI 流水线、GPL-3.0 许可证、双语文档 |
| v0.1.0 | 2026-05-01 | f4018c6 ~ 12e5e90 | 核心、前端、后端、移植层、工具、测试 |

## 使用方法

检出特定版本的代码：

```bash
git checkout <commit-hash>
```

查看两个版本之间的所有变更：

```bash
git log --oneline <from-hash>..<to-hash>
```

## 提交详情

### v0.7.0（7 个提交）
- 4a58af7 feat: add metadata bundles and source location decoding
- 11f5bad fix(tests): extend config preset assertions and add core module tests
- 92be6ba docs(version): sync README and spec headers to v0.7.0
- b89e649 docs(route): sync ROUTE.md checkboxes with implementation state
- 09b63a8 chore: clean up project structure docs and gitignore
- 5422684 docs(changelog): restructure CHANGELOG with correct version history
- f0e4c7f feat(ci): add local CI, clean, and pre-push scripts

### v0.6.0（3 个提交）
- f11a10a docs: sync all documentation with latest API changes
- 780bdd8 feat: comprehensive code quality improvements and MCU benchmark suite
- c2914c0 feat: migrate event payloads to wire v2

### v0.4.0（2 个提交）
- 97cdd7e fix: resolve concurrency issues, correct docs, and improve code quality
- fcd9a5b feat: add interface anti-bloat infrastructure for backward-compatible evolution

### v0.3.0（2 个提交）
- 363aa01 fix: use 0xFE instead of 0xFF for large delta marker
- 5adb1f1 fix: add power-of-2 assertion for ring buffer size

### v0.2.0（6 个提交）
- 227381f docs: fix bilingual links, switch to GPL-3.0 license, fix README license misstatement
- 9d338c0 ci: fix cppcheck include paths for three-layer port architecture
- 7723cde ci: make cppcheck/scan-build advisory, fix include paths, add continue-on-error
- f18a342 ci: fix cortex-m-qemu-test - add CMAKE_SYSTEM_NAME=Generic and -mcpu=cortex-m3 -mthumb
- d498e8c ci: fix cortex-m-qemu-test - disable tests/examples for cross-compile
- cde0f4c ci: strip to minimal build-and-test only, remove broken jobs

### v0.1.0（6 个提交）
- f4018c6 Initial commit
- 097a8fc docs: refactor all markdown files and add bilingual support
- a5e4bcf chore: add repository configuration and ignore patterns
- 049096f feat: initial project structure with baremetal core, tools and tests
- 9d34fc5 refactor: achieve industrial-grade O(1) performance with D2R and incremental CRC
- 12e5e90 chore: finalize industrial excellence refactor before history purge
