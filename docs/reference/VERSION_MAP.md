# Version Map

This document maps each AxiomTrace release to its corresponding commit range
in the Git history. Use this as a quick reference when bisecting issues or
understanding which changes shipped in each version.

> **Note**: v0.5.0-probe was intentionally skipped. The probe timing
> investigation was deferred to a later release.

| Version | Date | Commit Range | Key Changes |
|---------|------|--------------|-------------|
| v0.7.0 | 2026-05-30 | 4a58af7 ~ f0e4c7f | Wire v2, metadata bundle, CI, architecture refactor |
| v0.6.0 | 2026-05-27 | f11a10a ~ c2914c0 | Docs sync, code quality, MCU benchmark, wire v2 payload |
| v0.5.0-probe | — | — | **Skipped** (probe timing investigation deferred) |
| v0.4.0 | 2026-05-07 | 97cdd7e ~ fcd9a5b | Backend API, ring consume, concurrency fixes |
| v0.3.0 | 2026-05-02 | 363aa01 ~ 5adb1f1 | Timestamp fix (0xFE), ring power-of-2 assertion |
| v0.2.0 | 2026-05-01 | 227381f ~ cde0f4c | CI pipeline, GPL-3.0 license, bilingual docs |
| v0.1.0 | 2026-05-01 | f4018c6 ~ 12e5e90 | Core, frontend, backend, port, tool, tests |

## How to Use

To check out the code at a specific version:

```bash
git checkout <commit-hash>
```

To see all changes between two versions:

```bash
git log --oneline <from-hash>..<to-hash>
```

## Commit Details

### v0.7.0 (7 commits)
- 4a58af7 feat: add metadata bundles and source location decoding
- 11f5bad fix(tests): extend config preset assertions and add core module tests
- 92be6ba docs(version): sync README and spec headers to v0.7.0
- b89e649 docs(route): sync ROUTE.md checkboxes with implementation state
- 09b63a8 chore: clean up project structure docs and gitignore
- 5422684 docs(changelog): restructure CHANGELOG with correct version history
- f0e4c7f feat(ci): add local CI, clean, and pre-push scripts

### v0.6.0 (3 commits)
- f11a10a docs: sync all documentation with latest API changes
- 780bdd8 feat: comprehensive code quality improvements and MCU benchmark suite
- c2914c0 feat: migrate event payloads to wire v2

### v0.4.0 (2 commits)
- 97cdd7e fix: resolve concurrency issues, correct docs, and improve code quality
- fcd9a5b feat: add interface anti-bloat infrastructure for backward-compatible evolution

### v0.3.0 (2 commits)
- 363aa01 fix: use 0xFE instead of 0xFF for large delta marker
- 5adb1f1 fix: add power-of-2 assertion for ring buffer size

### v0.2.0 (6 commits)
- 227381f docs: fix bilingual links, switch to GPL-3.0 license, fix README license misstatement
- 9d338c0 ci: fix cppcheck include paths for three-layer port architecture
- 7723cde ci: make cppcheck/scan-build advisory, fix include paths, add continue-on-error
- f18a342 ci: fix cortex-m-qemu-test - add CMAKE_SYSTEM_NAME=Generic and -mcpu=cortex-m3 -mthumb
- d498e8c ci: fix cortex-m-qemu-test - disable tests/examples for cross-compile
- cde0f4c ci: strip to minimal build-and-test only, remove broken jobs

### v0.1.0 (6 commits)
- f4018c6 Initial commit
- 097a8fc docs: refactor all markdown files and add bilingual support
- a5e4bcf chore: add repository configuration and ignore patterns
- 049096f feat: initial project structure with baremetal core, tools and tests
- 9d34fc5 refactor: achieve industrial-grade O(1) performance with D2R and incremental CRC
- 12e5e90 chore: finalize industrial excellence refactor before history purge
