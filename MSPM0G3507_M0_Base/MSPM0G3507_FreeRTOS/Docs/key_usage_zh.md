# M0_Base PA13 单键十模式使用说明

适用工程：`MSPM0G3507_FreeRTOS`

当前菜单只使用 **PA13 一个按键**，不新增按键，不保留 `STATUS`、`TEST`、`CUSTOM`、`MY_APP` 等示例名称。
系统固定提供 `USER01`~`USER10` 共 10 个用户模式，所有模式接口已经暴露，二次开发只需要填写对应函数体。

## 1. 按键行为

| 当前界面 | PA13 短按 | PA13 长按 |
|---|---|---|
| 模式列表 | 选择下一个模式，USER10 后回到 USER01 | 进入当前选中的模式 |
| 模式运行中 | 调用当前模式的 `app_mode_userXX_short()` | 退出当前模式，返回模式列表 |

长按阈值、消抖和 PA13 的有效电平仍由原有按键库配置，未在本次改动中改变。

```text
上电
  |
  v
模式列表：短按循环选择 USER01~USER10
  |
  +-- 长按 --> 进入选中的 USER 模式
                    |
                    +-- 每 100 ms 调用 app_mode_userXX_run()
                    +-- 短按调用 app_mode_userXX_short()
                    +-- 长按返回模式列表
```

## 2. 只需要修改的文件

主要文件：

```text
Application/app_mode_menu.c
Application/app_mode_menu.h
```

### 2.1 每个模式的 3 个接口

以 USER01 为例：

```c
void app_mode_user01_enter(void); /* 进入模式时调用一次 */
void app_mode_user01_run(void);   /* 模式运行中每 100 ms 调用一次 */
void app_mode_user01_short(void); /* 模式运行中 PA13 短按时调用一次 */
```

USER02~USER10 的命名规则完全相同：

```text
app_mode_user02_enter/run/short()
...
app_mode_user10_enter/run/short()
```

函数声明位于 `Application/app_mode_menu.h`，函数实现位于 `Application/app_mode_menu.c` 底部。

## 3. 如何填写自己的模式

### 3.1 在进入接口中初始化

进入模式时，长按只触发一次 `enter()`，适合清零计数器、初始化状态机或设置初始参数：

```c
void app_mode_user01_enter(void)
{
    /* 示例：每次进入 USER01 时重新开始。 */
    /* user01_counter = 0U; */
    /* user01_running = false; */
}
```

不要在这里使用长时间阻塞延时。

### 3.2 在 run 接口中写周期业务

菜单任务进入模式后每 100 ms 调用一次 `run()`。函数应保持非阻塞：

```c
static uint32_t s_user01_counter;

void app_mode_user01_run(void)
{
    char text[24];

    s_user01_counter++;

    OLED_PrintASCIIString(0U, 0U, "USER01", &afont16x8,
                          OLED_COLOR_NORMAL);
    snprintf(text, sizeof(text), "CNT:%lu", s_user01_counter);
    OLED_PrintASCIIString(0U, 16U, text, &afont16x8,
                          OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 48U, "LONG:BACK", &afont16x8,
                          OLED_COLOR_NORMAL);
}
```

`OLED_NewFrame()` 和 `OLED_ShowFrame()` 已由菜单框架统一管理，模式函数中不要重复调用这两个函数。

如果使用 `snprintf()`，请确认当前工程的菜单任务栈空间足够；如果出现栈不足，应增大 `Application/app_main.c` 中的 `APP_MENU_TASK_STACK_WORDS`。

### 3.3 在 short 接口中写短按动作

模式内短按只进入当前模式自己的接口，不需要判断模式编号，也不需要处理 `key_t`、事件类型或时间参数：

```c
static bool s_user01_running;

void app_mode_user01_short(void)
{
    /* 示例：每次短按在“运行/暂停”之间切换。 */
    s_user01_running = !s_user01_running;
}
```

然后在 `app_mode_user01_run()` 中读取 `s_user01_running` 并执行非阻塞业务。

对于电机、继电器等高风险外设，建议短按函数只设置请求标志，实际动作交给控制任务：

```c
static volatile bool s_user01_start_request;

void app_mode_user01_short(void)
{
    s_user01_start_request = true;
}

void app_mode_user01_run(void)
{
    /* 显示和状态维护。 */
}
```

不要在按键函数里调用 `delay`、等待队列/信号量或执行长时间电机控制。

## 4. 十个模式对应关系

| 选中序号 | 菜单显示 | 进入接口 | 周期接口 | 模式内短按接口 |
|---:|---|---|---|---|
| 0 | USER01 | `app_mode_user01_enter()` | `app_mode_user01_run()` | `app_mode_user01_short()` |
| 1 | USER02 | `app_mode_user02_enter()` | `app_mode_user02_run()` | `app_mode_user02_short()` |
| 2 | USER03 | `app_mode_user03_enter()` | `app_mode_user03_run()` | `app_mode_user03_short()` |
| 3 | USER04 | `app_mode_user04_enter()` | `app_mode_user04_run()` | `app_mode_user04_short()` |
| 4 | USER05 | `app_mode_user05_enter()` | `app_mode_user05_run()` | `app_mode_user05_short()` |
| 5 | USER06 | `app_mode_user06_enter()` | `app_mode_user06_run()` | `app_mode_user06_short()` |
| 6 | USER07 | `app_mode_user07_enter()` | `app_mode_user07_run()` | `app_mode_user07_short()` |
| 7 | USER08 | `app_mode_user08_enter()` | `app_mode_user08_run()` | `app_mode_user08_short()` |
| 8 | USER09 | `app_mode_user09_enter()` | `app_mode_user09_run()` | `app_mode_user09_short()` |
| 9 | USER10 | `app_mode_user10_enter()` | `app_mode_user10_run()` | `app_mode_user10_short()` |

模式表 `s_modes[]` 已经固定注册这 10 个模式，通常不需要修改。

## 5. 调用链与职责

```text
PA13 GPIO
   |
   v
BSP/Peripherals/bsp_key.c
   |  GPIO 读取、消抖适配
   v
Lib/Key/key.c
   |  短按/长按状态机
   v
Application/app_key_events.c
   |  事件桥接
   v
Application/app_key_actions.c
   |  PA13 短按/长按转成菜单事件
   v
Application/app_mode_menu.c
   |  列表切换、模式进入、模式退出、模式回调
   +--> USER01~USER10 的 enter/run/short 接口
```

本次改动没有修改 PA13 的 GPIO、消抖、长按阈值，也没有新增按键。

## 6. 当前按键配置位置

| 配置 | 文件 | 说明 |
|---|---|---|
| 按键总开关 | `Config/key_config.h` | `PRJ_KEY_ENABLE` |
| 按键数量 | `Config/key_config.h` | 当前只注册 PA13 一个按键 |
| PA13 按键 ID 与配置 | `Config/key_config.h` | `PRJ_KEY_CONFIG_MENU` |
| 短按/长按路由 | `Application/app_key_actions.c` | PA13 映射到菜单事件 |
| 十模式接口 | `Application/app_mode_menu.c` | 修改 USER01~USER10 函数体 |
| 十模式 API 声明 | `Application/app_mode_menu.h` | 对外暴露的 30 个函数 |

## 7. 常见修改示例

### 示例 A：USER02 短按切换一个软件状态

```c
static bool s_user02_enabled;

void app_mode_user02_enter(void)
{
    s_user02_enabled = false;
}

void app_mode_user02_short(void)
{
    s_user02_enabled = !s_user02_enabled;
}

void app_mode_user02_run(void)
{
    OLED_PrintASCIIString(0U, 0U, "USER02", &afont16x8,
                          OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0U, 16U,
                          s_user02_enabled ? "ON" : "OFF",
                          &afont16x8, OLED_COLOR_NORMAL);
}
```

### 示例 B：USER03 短按只发出控制请求

```c
static volatile bool s_user03_request;

void app_mode_user03_short(void)
{
    /* 只发请求，不在按键上下文直接启动电机。 */
    s_user03_request = true;
}

void app_mode_user03_run(void)
{
    OLED_PrintASCIIString(0U, 0U, "USER03", &afont16x8,
                          OLED_COLOR_NORMAL);
}
```

控制任务应在自己的周期中读取并清除请求，再调用实际控制接口。

## 8. 安全约定

1. 模式 `run()` 每 100 ms 执行，不能写阻塞循环。
2. `short()` 只做轻量状态变更或设置请求标志。
3. 电机测试必须使用低速、悬空车轮和硬件急停。
4. 模式长按退出由框架统一处理，不要在 `short()` 中模拟长按退出。
5. 不要修改 `s_modes[]`，除非确实需要改变模式数量或模式显示名。
6. 本次重构前的回滚备份位于：

```text
D:\msp_project\temp2\MSPM0G3507_Project_temp\MSPM0G3507_M0_Base\MSPM0G3507_FreeRTOS\bak_fix\before_user10_mode_20260727_204706
```

## 9. 验证步骤

1. Keil 全量编译 `MSPM0G3507_M0_Base`。
2. 烧录后上电，OLED 应显示 USER01~USER10 列表。
3. 在列表短按 PA13，光标应逐项循环移动。
4. 长按 PA13，应进入当前模式并显示该模式内容。
5. 模式内短按 PA13，应只调用对应的 `app_mode_userXX_short()`。
6. 模式内长按 PA13，应返回列表。
7. 修改某个模式函数后，只需重新编译，不需要改按键库或新增按键。
