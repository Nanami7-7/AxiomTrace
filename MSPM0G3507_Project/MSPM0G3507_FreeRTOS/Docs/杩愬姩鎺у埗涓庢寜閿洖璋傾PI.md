# 运动控制与按键回调 API

## 1. 设计边界

本层位于 `Application/Algorithm/app_motion_control.*`，只负责把易用的运动命令接到现有控制器：

- 速度环仍由 `Application/Task/task_control.c` 中现有 PID/PWM 循环执行。
- 角度环仍由 `Application/Algorithm/app_position_control.c` 执行，本层不重写角度误差、差速和到位算法。
- 位置环仍由现有位置控制器执行，本层只完成“距离（米）到编码器脉冲”的换算。
- 按键回调不直接写 PWM；在回调中调用下面的 API 即可。

## 2. 按键回调编辑位置

文件：`Application/app_key_events.c`

可直接在以下三个函数体中写用户动作：

```c
void app_key_short_press_action(...); // 短按
void app_key_long_press_action(...);  // 长按
void app_key_stuck_action(...);       // 卡键/超时
```

回调已经由 `app_key_event_callback()` 分发，不需要再写事件队列或 `post` 函数。

### 短按：单个电机速度

```c
(void)app_motion_speed_motor(APP_MOTION_MOTOR_A, 200.0f);
```

- `A/B/C/D` 对应 `M1/M2/M3/M4` 的应用电机编号。
- `rpm > 0` 使用当前工程定义的正方向，`rpm < 0` 反向。
- 单电机 API 会把其他电机目标设为 0 并禁用其他电机。

### 短按：四轮速度

```c
const float rpm[APP_MOTION_MOTOR_COUNT] = {
    200.0f, 200.0f, 200.0f, 200.0f
};
(void)app_motion_speed_all(rpm);
```

### 长按：相对角度控制

```c
(void)app_motion_angle_start_relative(90.0f, 120.0f, 10000U);
```

参数依次为：目标相对角度（度）、转向巡航 RPM、超时时间（毫秒）。正角度沿当前工程的左转约定执行。

### 长按：绝对角度控制

```c
(void)app_motion_angle_start_absolute(90.0f, 120.0f, 10000U);
```

目标角度是当前 IMU yaw 坐标系下的绝对角度。

### STUCK：距离控制

```c
(void)app_motion_position_start(2.0f, 120.0f, 10000U);
```

参数依次为：距离（米）、巡航 RPM、超时时间（毫秒）。正距离沿当前位置环正方向运动，完成后由位置环保持。

> 默认 STUCK 实现仍采用安全停车。若确实需要“卡键后走 2 m”，再把 `app_key_stuck_action()` 中的 `app_motion_stop()` 替换为上面的距离 API。

## 3. 运动 API

头文件：`Application/Algorithm/app_motion_control.h`

| API | 用途 |
|---|---|
| `app_motion_control_init(ctx)` | 初始化适配层，系统启动时调用一次 |
| `app_motion_speed_motor(motor, rpm)` | 单电机速度维持 |
| `app_motion_speed_all(rpm[4])` | 四电机速度维持 |
| `app_motion_speed_all_timed(rpm[4], timeout_ms)` | 四电机速度维持，超时停车 |
| `app_motion_angle_start_relative(delta, rpm, timeout_ms)` | 相对角度控制并保持 |
| `app_motion_angle_start_absolute(target, rpm, timeout_ms)` | 绝对角度控制并保持 |
| `app_motion_position_start(distance, rpm, timeout_ms)` | 距离控制并保持 |
| `app_motion_stop()` | 取消外环目标并安全停车 |
| `app_motion_control_process(now_ms)` | 在现有控制任务中执行超时监督 |

### 参数约束

- `rpm` 必须是有限值，绝对值不超过 `PRJ_PLANNER_MAX_RPM`。
- 角度/位置的 `cruise_rpm` 必须大于 0。
- `timeout_ms == 0` 表示不设置超时；非零值至少为 20 ms。
- `app_motion_*` 返回 `true` 表示参数和启动条件通过，不代表电机已经达到目标。
- 到达角度/位置后不自动退出控制模式；目标速度归零，由原有外环继续保持。

## 4. 配置项

位于 `Config/project_config.h`：

```c
#define PRJ_KEY_FORWARD_RPM          (200.0f)
#define PRJ_KEY_FORWARD_TIMEOUT_MS   (3000U)
#define PRJ_KEY_TURN_TARGET_DEG      (90.0f)
#define PRJ_KEY_TURN_CRUISE_RPM      (120.0f)
#define PRJ_KEY_TURN_TIMEOUT_MS      (10000U)
#define PRJ_KEY_STUCK_DISTANCE_M     (2.0f)
#define PRJ_KEY_STUCK_CRUISE_RPM     (120.0f)
#define PRJ_KEY_STUCK_TIMEOUT_MS     (10000U)
```

修改这些默认参数不会改变角度环实现。

## 5. 初始化与控制任务接入

`Application/app_main.c` 中通过 `app_key_motion_init(&s_shared_ctx)` 初始化适配层；该函数内部调用 `app_motion_control_init()`。

`Application/Task/task_control.c` 每个控制周期调用兼容入口：

```c
app_key_motion_process(osal_ticks_to_ms(osal_get_tick_count()));
```

该入口内部只转发到 `app_motion_control_process()`，用于超时停车监督；PID、角度环和位置环仍由原控制循环执行。

## 6. 安全建议

1. 第一次测试抬轮进行，先使用低 RPM 和短超时。
2. 用户回调中不要直接调用 `bsp_motor_set_speed()`，也不要阻塞延时。
3. 任何异常分支调用 `app_motion_stop()`。
4. 角度方向与车体实际方向不一致时，只调整回调中传入的角度符号，不修改底层角度环。