# Release HEX files

- `MSPM0G3507_MotorController_v0.1.0.hex`: production firmware. Use this for normal operation.
- `MSPM0G3507_DRV8870_FactoryTest_v0.1.0.hex`: diagnostic firmware. Use only on a suspended-wheel, current-limited test bench.

Both artifacts were built on 2026-07-18 with Keil ARMCLANG 6.24 and completed with 0 errors and 0 warnings.

Before first motion, query `Info?` and confirm `fw=0.1.0,proto=1`. Keep the hardware motor-power switch accessible.

## SHA-256

- `MSPM0G3507_MotorController_v0.1.0.hex`: `7662905BA2F85303FDF895301F5C28213BB8400437DA36C72C77FB1B6DE9E7DC`
- `MSPM0G3507_DRV8870_FactoryTest_v0.1.0.hex`: `44AC6899BFD3657E2C13F97F563CADEA75DF740EF9E9FC73B2416A8EDD72C7FF`
