# 开发指南 / Development Guide

## 目录 / Table of Contents

1. [开发环境 / Development Environment](#1-开发环境--development-environment)
2. [工程结构 / Project Structure](#2-工程结构--project-structure)
3. [编码规范 / Coding Conventions](#3-编码规范--coding-conventions)
4. [新增 BSP 模块指南 / Adding BSP Modules](#4-新增-bsp-模块指南--adding-bsp-modules)
5. [新增任务指南 / Adding Tasks](#5-新增任务指南--adding-tasks)
6. [OSAL API 参考 / OSAL API Reference](#6-osal-api-参考--osal-api-reference)
7. [AxiomTrace 集成 / AxiomTrace Integration](#7-axiomtrace-集成--axiomtrace-integration)
8. [版本控制 / Version Control](#8-版本控制--version-control)

---

## 1. 开发环境 / Development Environment

### 工具链 / Toolchain

| 工具 | 版本要求 | 说明 |
|---|---|---|
| Keil MDK | v5.38+ | ARM 编译器 v6 (armclang) |
| TI MSPM0 SDK | v2.05.00.05 | 芯片驱动库 |
| SysConfig | 随 SDK | 外设配置工具 |

### 依赖项 / Dependencies

- FreeRTOS Kernel（位于 `kernel/freertos/`）
- TI MSPM0 DriverLib（位于 `source/ti/`）
- AxiomTrace（位于 `lib/axiomtrace/`）

---

## 2. 工程结构 / Project Structure

```
MSPM0G3507_FreeRTOS/
├── main.c                    FreeRTOS 入口
├── board.c/h                 Board 初始化
├── ti_msp_dl_config.c/h      SysConfig 生成
├── FreeRTOSConfig.h          FreeRTOS 配置
├── empty.syscfg              SysConfig 工程
│
├── App/                      应用层
│   ├── app_main.c/h          主入口 + 公共函数
│   ├── app_pid.c/h           PID 控制器
│   ├── app_vofa.c/h          VOFA+ 协议
│   └── Task/
│       ├── task_control.c/h  控制任务
│       └── task_menu.c/h     菜单任务
│
├── BSP/                      板级支持包
├── Config/                   工程配置
├── HAL/                      硬件抽象层
├── OSAL/                     操作系统抽象层
├── port/                     AxiomTrace 移植
├── lib/                      第三方库
├── docs/                     项目文档
└── keil/                     Keil 工程文件
```

---

## 3. 编码规范 / Coding Conventions

### 命名规则 / Naming Rules

| 类型 | 规则 | 示例 |
|---|---|---|
| 文件名 | 小写 + 下划线 | `app_pid.c`, `bsp_motor.h` |
| 函数名 | `模块_动作_对象` | `app_pid_compute()`, `bsp_motor_set_speed()` |
| 宏定义 | `模块_描述` 大写 | `APP_PID_DEFAULT_KP`, `BSP_MOTOR_COUNT` |
| 类型名 | `模块_描述_t` | `app_pid_t`, `bsp_motor_config_t` |
| 枚举值 | `模块_值` 大写 | `APP_PID_MODE_INCREMENT`, `BSP_OK` |
| 私有变量 | `s_` 前缀 | `s_shared_ctx`, `s_motor_cfg` |
| 私有函数 | 模块内 `static` | `static float clamp_f(...)` |

### 注释风格 / Comment Style

```c
/**
 * @brief  函数简述
 * @note   详细说明（可选）
 * @param  param1  参数说明
 * @retval 返回值说明
 */
```

### 头文件保护 / Header Guard

```c
#ifndef APP_PID_H
#define APP_PID_H

#ifdef __cplusplus
extern "C" {
#endif

// ... 内容 ...

#ifdef __cplusplus
}
#endif

#endif /* APP_PID_H */
```

### 错误处理 / Error Handling

- BSP 函数返回 `bsp_status_t`（BSP_OK / BSP_ERR_*）
- 初始化函数返回 `int32_t`（0=成功，负数=错误码）
- 所有 `printf` 返回值强制 `(void)` 转换（消除警告）
- 指针参数检查 NULL

---

## 4. 新增 BSP 模块指南 / Adding BSP Modules

### 步骤 / Steps

1. **HAL 层**（如需）：在 `HAL/` 创建 `hal_xxx.c/h`
2. **BSP 层**：在 `BSP/` 创建 `bsp_xxx.c/h`
3. **头文件**：定义配置结构体、状态枚举、函数接口
4. **实现**：通过 HAL 或直接操作寄存器
5. **初始化**：在 `app_main.c` 的 `bsp_modules_init()` 中调用
6. **Keil 工程**：将文件添加到 Keil 工程的对应 Group

### 示例模板 / Template

```c
// bsp_xxx.h
#ifndef BSP_XXX_H
#define BSP_XXX_H

#include <stdint.h>
#include "bsp_common.h"

typedef struct {
    // 配置参数
} bsp_xxx_config_t;

bsp_status_t bsp_xxx_init(const bsp_xxx_config_t *cfg, uint32_t count);
bsp_status_t bsp_xxx_do_something(uint32_t id);

#endif
```

---

## 5. 新增任务指南 / Adding Tasks

### 步骤 / Steps

1. 在 `App/Task/` 创建 `task_xxx.c/h`
2. 定义任务函数 `void app_xxx_task(void *param)`
3. 在 `app_main.h` 定义优先级和栈大小宏
4. 在 `app_main.c` 的 `app_main_init()` 中调用 `osal_task_create()`
5. 将文件添加到 Keil 工程

### 模板 / Template

```c
// task_xxx.h
#ifndef TASK_XXX_H
#define TASK_XXX_H

void app_xxx_task(void *param);

#endif

// task_xxx.c
#include "task_xxx.h"
#include "app_main.h"
#include "osal_api.h"

void app_xxx_task(void *param)
{
    app_shared_ctx_t *ctx = (app_shared_ctx_t *)param;

    for (;;) {
        // 任务逻辑
        osal_task_delay_ms(APP_XXX_PERIOD_MS);
    }
}
```

### 共享数据访问 / Shared Data Access

- 读写 `app_shared_ctx_t` 中的数据必须在 `OSAL_CRITICAL_SECTION` 内
- 尽量减少临界区内的计算量
- 复制到局部变量后再处理

---

## 6. OSAL API 参考 / OSAL API Reference

### 任务 / Tasks

```c
osal_task_handle_t osal_task_create(
    void (*func)(void *),  // 任务函数
    const char *name,       // 任务名称
    uint32_t stack_words,   // 栈大小（字）
    void *param,            // 参数
    uint32_t priority       // 优先级
);

void osal_task_delay_ms(uint32_t ms);  // 延时
void osal_scheduler_start(void);        // 启动调度器
```

### 临界区 / Critical Section

```c
OSAL_CRITICAL_SECTION {
    // 临界区代码（关闭中断）
    // 可重入（嵌套安全）
}
```

映射到 FreeRTOS `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()`。

### 互斥锁 / Mutex

```c
osal_mutex_t mtx = osal_mutex_create();
osal_mutex_lock(mtx);
// 临界代码
osal_mutex_unlock(mtx);
osal_mutex_delete(mtx);
```

### 信号量 / Semaphore

```c
osal_sem_t sem = osal_sem_create(0);  // 初始值=0
osal_sem_post(sem);    // 释放（+1）
osal_sem_wait(sem);    // 获取（-1，阻塞）
osal_sem_delete(sem);
```

### 消息队列 / Message Queue

```c
osal_queue_t q = osal_queue_create(10, sizeof(msg_t));
osal_queue_send(q, &msg, 0);     // 发送
osal_queue_recv(q, &msg, 100);   // 接收（带超时）
osal_queue_delete(q);
```

---

## 7. AxiomTrace 集成 / AxiomTrace Integration

### 使用方式 / Usage

```c
#include "axiomtrace.h"

AX_LOG_INFO("系统启动");
AX_LOG_WARN("电机 %lu NaN/Inf!", (unsigned long)motor_id);
AX_LOG_ERROR("初始化失败: %d", err);
```

### Profile 配置 / Profile Configuration

在 `axiom_config.h` 中定义 profile：

```c
#define AXIOM_PROFILE_DEV   // 开发模式：所有级别输出
// #define AXIOM_PROFILE_PROD  // 生产模式：仅 ERROR
```

### 输出方式 / Output Method

AxiomTrace 通过 `bsp_uart_putc()` 输出字符，共享 UART 串口。

---

## 8. 版本控制 / Version Control

### Git Commit 规范 / Commit Convention

```
<type>: <description>

type:
  feat     新功能
  fix      修复
  refactor 重构
  docs     文档
  style    代码风格
  test     测试
  chore    构建/工具
```

### 示例 / Examples

```
feat: add VOFA+ remote tuning protocol
fix: correct FireWater format to comma-separated
refactor: extract common motor stop logic
docs: rewrite README with architecture guide
```

---