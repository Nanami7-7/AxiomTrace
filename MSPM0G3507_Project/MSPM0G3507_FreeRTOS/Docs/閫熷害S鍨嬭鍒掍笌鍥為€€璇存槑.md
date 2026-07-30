# 速度 S 型规划与回退说明

## 1. 目的

速度 S 型规划只负责把“最终目标 RPM”变成平滑目标，减少启动、变速和普通停车时的机械冲击。
它不替代速度 PID、编码器低速处理、左右轮同步和 IMU 航向修正。

当前控制链路：

```text
用户/协议目标 RPM
    -> S 型规划（100Hz，可关闭）
    -> IMU 直行差速修正
    -> 速度 PID（500Hz）
    -> PWM 输出变化率限制
    -> 电机
```

## 2. 一键启用和回退

配置文件：`Config/project_config.h`

```c
/* 1：启用 S 型规划；0：完整回退到原线性目标斜坡。 */
#define PRJ_SPEED_SCURVE_ENABLE             (1U)

/* S 型规划周期，10ms=100Hz；速度 PID 仍为 2ms=500Hz。 */
#define PRJ_SPEED_PLANNER_PERIOD_MS         (10U)

/* 最大目标加速度，越小则启动和刹车越慢。 */
#define PRJ_SPEED_SCURVE_MAX_ACCEL_RPM_S    (400.0f)

/* 最大 Jerk，越小则加速度变化越柔和。 */
#define PRJ_SPEED_SCURVE_MAX_JERK_RPM_S2    (1600.0f)
```

需要快速回退时只改一处：

```c
#define PRJ_SPEED_SCURVE_ENABLE             (0U)
```

宏关闭后，控制任务不创建或调用 S 型规划器，继续使用原来的 `app_chassis_slew()` 线性目标斜坡。规划器周期参数也不参与关闭配置的编译检查。

## 3. 最简 API

文件：

```text
Application/Algorithm/app_speed_planner.h
Application/Algorithm/app_speed_planner.c
```

模块是纯 C 算法，不依赖 FreeRTOS、BSP、UART 或具体电机驱动，移植时复制这两个文件即可。

```c
#include "app_speed_planner.h"

static app_speed_planner_t g_planner;

void speed_plan_init(void)
{
    app_speed_planner_init(&g_planner, 400.0f, 1600.0f);
    app_speed_planner_reset(&g_planner, 0.0f);
}

/* 每 10ms 调用一次。 */
float speed_plan_run(float final_target_rpm)
{
    return app_speed_planner_update(&g_planner,
                                    final_target_rpm,
                                    0.01f);
}
```

通常业务只需要三个函数：

- `app_speed_planner_init()`：设置最大加速度和最大 Jerk；
- `app_speed_planner_update()`：周期输入最终目标，取得平滑目标；
- `app_speed_planner_reset()`：禁用、急停、故障或控制器切换时清状态。

## 4. 停车规则

### 4.1 普通停车

用户正常把目标速度设为 `0 RPM` 时，继续调用 `update()`，让小车按 S 型曲线平滑减速。

### 4.2 急停、禁用、过流、故障

安全停车不能等待规划器：

```c
/* 第一步：立即关闭电机输出。 */
bsp_motor_stop_all();

/* 第二步：清除规划状态，防止重新使能后沿用旧曲线。 */
app_speed_planner_reset(&g_planner, 0.0f);
```

本工程中的 `STOP`、`STOP_ALL`、`DISABLE`、过流停机和控制模式接管均遵循该规则。

## 5. 自动回退机制

规划器检测以下异常：

- 最大加速度或最大 Jerk 非法；
- `dt_s <= 0`；
- 目标、速度或加速度出现 NaN/Inf；
- 内部计算结果异常。

异常发生后，规划器置 `valid=false` 并返回 `0`。Board A 控制任务检测到无效状态后自动使用原线性斜坡，不需要重启。

```c
if (app_speed_planner_is_valid(&planner)) {
    target = app_speed_planner_update(&planner, request, dt_s);
} else {
    target = app_chassis_slew(target, request, linear_step);
}
```

## 6. 参数调整

当前默认参数：

```text
最大加速度 = 400 RPM/s
最大 Jerk  = 1600 RPM/s²
加速度建立时间约为 400/1600 = 0.25s
```

调整规律：

- 起步、停车仍太冲：先降低 `MAX_JERK`，再降低 `MAX_ACCEL`；
- 起步过慢：先提高 `MAX_ACCEL`，再适量提高 `MAX_JERK`；
- 车轮打滑：降低 `MAX_ACCEL`；
- 机械齿隙冲击明显：降低 `MAX_JERK`；
- 不要通过提高 PID 增益来弥补规划过慢，否则低速更容易震荡。

建议首次实车测试顺序：

```text
0 -> 80 -> 130 -> 200 -> 0 RPM
```

先将车轮悬空确认方向和急停，再落地检查起步、直行、反向和制动。

## 7. 末端处理说明

规划器主体限制加速度和 Jerk。由于控制器按离散周期运行，在目标附近可能出现不足一个周期的尾差；实现会在穿越目标的最后一步将速度吸附到目标并清零内部加速度，避免目标附近反复回摆。

该末端吸附：

- 不会越过最终目标；
- 速度修正不会超过当前规划周期可达到的速度步进；
- 不用于急停；
- 内部加速度清零属于末端停止动作，不计入严格的连续 Jerk 约束。

## 8. 与其他控制模式的关系

- `SPEED`：使用 S 型规划或线性回退；
- `POSITION/ANGLE`：保留原有位置规划器，不重复进行速度 S 型规划；
- 循迹/模型辨识接管：清除普通速度规划状态；
- 从其他模式切回 `SPEED`：以当前实测 RPM 为规划起点，降低切换冲击；
- 电机重新使能：以当前实测 RPM 重新开始规划。