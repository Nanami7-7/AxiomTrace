# AxiomTrace Benchmark Baseline

> Version: v1.0 baseline candidate | Update Date: 2026-05-28

This document records the host-side regression benchmark used as the v1.0 release gate. Host timings are not MCU cycle guarantees; they catch regressions in encode, CRC, ring write, filtering, and overflow behavior. Target MCU cycle counts still require platform-specific measurement.

## Current Local Baseline

Command:

```bash
build_codex_audit_gcc/tests/host/test_benchmark
```

Verified scenarios:

| Scenario | Avg | P99 | P99.9 | FOC Budget |
|---|---:|---:|---:|---|
| FOC 30kHz | ~0.05 us | ~0.10 us | ~0.10 us | PASS <= 3.3 us |
| FOC 30kHz max load | ~0.06 us | ~0.10 us | ~0.10 us | PASS <= 3.3 us |
| FOC 30kHz small ring | ~0.05 us | ~0.10 us | ~0.10 us | PASS <= 3.3 us |
| IoT sensor | ~0.05 us | ~0.10 us | ~0.10 us | PASS <= 3.3 us |
| ISR nesting pressure | ~0.06 us | ~0.10 us | ~0.10 us | PASS <= 3.3 us |
| Full encode pipeline | ~0.06 us | ~0.20 us | ~0.12 us | PASS <= 3.3 us |

Standalone checks:

| Check | Avg | P99 | P99.9 |
|---|---:|---:|---:|
| CRC-16, 32B | ~0.15 us | ~0.20 us | ~0.30 us |
| Critical enter+exit | ~0.02 us | ~0.10 us | ~0.10 us |

## Release Gate

- CI must run `tests/host/test_benchmark` and publish the output.
- A regression is release-blocking when FOC P99.9 exceeds the 3.3 us host gate or changes by more than 20% without a documented reason.
- MCU estimates in benchmark output are advisory. Exact Cortex-M/RISC-V numbers must be measured in a target project before claiming target-specific cycle guarantees.
