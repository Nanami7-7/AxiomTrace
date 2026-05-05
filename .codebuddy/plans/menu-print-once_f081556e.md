---
name: menu-print-once
overview: 主菜单信息只打印一次，添加need_print标志控制，从子状态返回时重新打印
todos:
  - id: add-need-print-flag
    content: menu_ctx_t加need_print字段, menu_state_main条件打印, 返回MAIN时置true
    status: completed
---

## 用户需求

主菜单信息只需显示一次，不要每次100ms轮询循环都重复打印。当前`menu_state_main()`每次进入都调`menu_print_main()`，导致串口不断刷屏。

## 修改方案

在`menu_ctx_t`中增加`bool need_print`标志，仅当为true时打印主菜单，打印后置false；从子状态返回MAIN时置true。

## 实现方案

修改`app_main.c`共4处：

1. **`menu_ctx_t`结构体**(第46-53行)：加`bool need_print`字段
2. **`menu_state_main()`**(第246-295行)：`menu_print_main(ctx)`改为条件调用——仅在`ctx->need_print`为true时打印，打印后置false
3. **7处`ctx->state = MENU_STATE_MAIN`**：在赋值后加`ctx->need_print = true`
4. **`app_menu_task()`初始化**：`ctx`清零后设`ctx.need_print = true`