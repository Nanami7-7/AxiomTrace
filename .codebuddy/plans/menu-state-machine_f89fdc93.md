---
name: menu-state-machine
overview: 将监控任务重构为菜单交互任务，采用状态机架构实现5个状态（主菜单/电机选择/设RPM/调PID/运行），替代原来的自动阶跃调参。
todos:
  - id: modify-app-main-h
    content: 更新app_main.h常量定义和函数声明
    status: completed
  - id: implement-menu-state-machine
    content: 实现菜单状态机(枚举+上下文+行输入+5个handler+函数指针表)
    status: completed
    dependencies:
      - modify-app-main-h
  - id: modify-control-task
    content: 控制任务添加motor_enabled检查跳过disabled电机
    status: completed
    dependencies:
      - modify-app-main-h
  - id: integrate-menu-task
    content: 替换app_main_init中监控任务为菜单任务,删除pid_tuning_step_2s
    status: completed
    dependencies:
      - implement-menu-state-machine
      - modify-control-task
---

## 产品概述

嵌入式电机PID速度控制系统(MSPM0G3507 + FreeRTOS)的串口菜单交互界面。替换当前自动阶跃调参的监控任务，改为基于状态机的交互式菜单任务，用户通过串口手动选择电机、设定目标RPM、调PID参数、启停电机并实时观察RPM。

## 用户需求

重构app_main.c的监控任务为菜单状态机任务，串口循环打印主菜单(含当前选中电机和所有电机PID参数)："1:选择电机 2:设定RPM 3:调参(KP KI KD) 4:启动电机(每10ms输出RPM) 0:退出当前模式"。根据输入进入子模式：

- **选项1 - 选择电机**：输入1~4选择对应电机(A/B/C/D)，输入0返回主菜单
- **选项2 - 设定RPM**：输入整数RPM值(如300)，作用于当前选中电机，输入0返回
- **选项3 - 调参**：一行输入三个浮点值空格分隔(如"1.5 0.3 0.1")，设置当前选中电机的KP/KI/KD，输入0返回
- **选项4 - 启动电机**：启动当前选中电机并以10ms间隔输出该电机RPM值，输入0退出时电机自动刹车停转
- **选项0**：退出当前子模式返回主菜单

## 补充澄清

- RPM输出间隔：10ms(非1ms，避免UART带宽不足)
- RPM/PID参数仅作用于当前选中的单台电机(非全部4路)
- PID输入格式：一行三个浮点值空格分隔
- 完全替代监控任务，删除自动阶跃调参(pid_tuning_step_2s)

## 技术栈

嵌入式C (MSPM0G3507 + FreeRTOS + OSAL)，与现有项目一致。

## 实现方案

### 核心策略：状态机 + 函数指针表

将监控任务替换为菜单任务，采用函数指针表驱动状态机(遵循design-patterns.md的状态机模式)：

```
5个状态: MAIN → MOTOR_SELECT / SET_RPM / TUNING_PID / RUNNING
         ↑_________0键返回_________|
```

每个状态对应一个handler函数，通过`s_menu_handlers[state]`查表调用，避免冗长switch-case。

### 数据流设计

```
菜单任务(低优先级)          控制任务(高优先级,5ms)
─────────────────          ──────────────────────
选项2:存target_rpm          读编码器→算RPM
选项4:设setpoint+使能  ───→  PID计算→电机输出
       →写s_motor_enabled    →写s_control_status.rpm
       →读s_control_status.rpm
退出4:清使能+刹车+reset PID  跳过disabled电机
```

**线程安全关键点**：

- `s_motor_enabled[]`(bool)：菜单任务写、控制任务读，bool写原子，可接受1周期延迟
- `app_pid_set_setpoint()`：写float在Cortex-M0+ 32位对齐上是原子的
- `app_pid_set_params()`：写3个float非原子，但调参是非实时用户操作，短暂不一致可接受
- `s_control_status.rpm[]`：已有OSAL_CRITICAL_SECTION保护

### 必须引入motor_enabled标志

退出运行模式后，控制任务仍以5ms周期调用`bsp_motor_set_speed()`。若仅设setpoint=0：

1. PID reset后积分清零，但编码器仍有残余脉冲
2. PID计算出非零输出 → `bsp_motor_set_speed(小正值)` 覆盖刹车状态 → 电机微动

解决方案：添加`s_motor_enabled[motor]`标志，控制任务跳过disabled电机的PID计算和输出。

### UART输入策略

- `bsp_uart_getc()`非阻塞，每次任务循环poll所有可用字节
- 行缓冲区累积输入，遇到`\r`或`\n`视为一行完成
- 支持回显(可打印字符回显)和退格(0x7F删除末字符)
- 用`atoi()`解析RPM整数，用`sscanf("%f %f %f")`解析PID三浮点

### 状态相关延时

- MAIN/MOTOR_SELECT/SET_RPM/TUNING_PID: 100ms轮询(足够响应人类输入)
- RUNNING: 10ms延时(匹配RPM输出间隔)

## 实现备注

- **PID setpoint单位**：用户输入RPM，需调用`bsp_encoder_rpm_to_pulse(rpm, APP_CONTROL_PERIOD_MS)`转换为脉冲增量作为PID setpoint
- **主菜单显示**：打印当前选中电机编号 + 所有4路电机的KP/KI/KD参数表，格式如：

```
=== Motor: A ===
A: KP=0.80 KI=0.30 KD=0.00
B: KP=0.80 KI=0.30 KD=0.00
C: KP=0.80 KI=0.30 KD=0.00
D: KP=0.80 KI=0.30 KD=0.00
1:Motor 2:RPM 3:PID 4:Run 0:Back
```

PID参数直接读`s_pid[i]`成员(菜单任务单写，无竞态)

- **RPM输出格式**：`printf("%ld\n", rpm)`，纯数值每行一个，便于串口绘图工具解析
- **LED心跳**：菜单任务中用计数器分频实现约500ms翻转，避免10ms运行状态下LED闪烁过快
- **输入校验**：电机编号1~4、RPM范围、PID参数范围需做基本校验，无效输入打印错误提示
- **栈大小**：菜单任务需384字(原监控128字)，因行缓冲区(64B)+printf内部缓冲+sscanf

## 目录结构

```
MSPM0G3507_FreeRTOS/
├── App/
│   ├── app_main.c   # [MODIFY] 删除监控任务+自动阶跃, 新增菜单状态机
│   └── app_main.h   # [MODIFY] 更新常量定义和函数声明
```

### app_main.h 修改详情

- **删除**: `APP_MONITOR_PERIOD_MS`, `APP_TASK_PRIORITY_MONITOR`, `APP_TASK_STACK_MONITOR`
- **新增**: `APP_MENU_POLL_PERIOD_MS (100U)`, `APP_RPM_OUTPUT_PERIOD_MS (10U)`, `MENU_LINE_BUF_SIZE (64U)`, `APP_TASK_PRIORITY_MENU (2U)`, `APP_TASK_STACK_MENU (384U)`
- **替换**: `app_monitor_task` → `app_menu_task`
- **更新**: 文件头注释

### app_main.c 修改详情

**删除**:

- `pid_tuning_step_2s()`函数和`s_pid_tune_rpm_table[]`
- `app_monitor_task()`函数
- 监控任务句柄和创建代码

**新增**:

- 菜单状态枚举`menu_state_t`(MAIN/MOTOR_SELECT/SET_RPM/TUNING_PID/RUNNING/COUNT)
- 菜单上下文结构体`menu_ctx_t`(state, selected_motor, target_rpm, line_buf, line_pos)
- 电机使能标志`static bool s_motor_enabled[BSP_MOTOR_COUNT]`
- 行输入函数`menu_read_line()`：非阻塞逐字节读取+回显+退格+行完成检测
- 5个状态处理函数：`menu_state_main()`, `menu_state_motor_select()`, `menu_state_set_rpm()`, `menu_state_tuning_pid()`, `menu_state_running()`
- 状态处理函数表`static const menu_handler_t s_menu_handlers[]`
- `app_menu_task()`：状态机主循环(poll输入→查表调handler→状态相关延时)

**修改**:

- `app_control_task()`: PID循环中检查`s_motor_enabled[i]`，disabled电机跳过PID计算和输出
- `app_main_init()`: 创建菜单任务替代监控任务
- 新增`#include <stdlib.h>`(atoi)和`#include <string.h>`(memset)