/**
 * @file    app_key_actions.c
 * @brief   应用层按键动作路由。
 *
 * @details
 * 本文件只负责把“按键 ID + 按键事件”转换为应用动作：
 *
 *   BSP/Peripherals/bsp_key.c  ->  Lib/Key/key.c
 *                                  ->  app_key_events.c
 *                                  ->  本文件
 *
 * 按键扫描、消抖和短按/长按判定由通用按键库完成；本文件不读取 GPIO，
 * 也不应该在回调中执行长时间阻塞操作。
 */
#include "app_key_actions.h"
#include "app_mode_menu.h"
#include "key_config.h"

/* -------------------- 当前菜单按键动作 -------------------- */

/**
 * @brief 菜单键短按动作。
 * @note 当前逻辑由菜单任务异步消费，不在按键回调中直接绘制 OLED。
 */
static void app_key_action_menu_short(void)
{
    app_mode_menu_on_key_event(KEY_EVENT_SHORT_PRESS);
}

/**
 * @brief 菜单键长按动作。
 * @note 当前逻辑由菜单任务异步消费，不在按键回调中直接绘制 OLED。
 */
static void app_key_action_menu_long(void)
{
    app_mode_menu_on_key_event(KEY_EVENT_LONG_PRESS);
}

/*
 * ============================================================================
 * 可复制的“用户按键：短按/长按控制”完整示例（默认关闭）
 * ============================================================================
 *
 * 使用方法：
 *   1. 在 Config/key_config.h 中增加：
 *        #define PRJ_KEY_ID_USER (1U)
 *      并增加一个 PRJ_KEY_CONFIG_USER，最后把 PRJ_KEY_COUNT 改为 2U。
 *   2. 将下面完整示例处的 #if 0 改为 #if 1。
 *   3. 在 app_key_actions_dispatch() 中启用下面标记为“复制到路由函数”的代码。
 *   4. 在控制任务中读取 app_key_example_get_run_request() 和
 *      app_key_example_take_stop_request()，再调用项目实际的电机/模式接口。
 *
 * 示例动作：
 *   - 短按：切换“运行请求”状态；
 *   - 长按：清除运行请求，并产生一次“立即停止”请求。
 *
 * 这样做比在按键回调中直接调用电机 PWM 更安全：按键回调只置位请求，
 * 由控制任务在固定周期内执行实际动作。长按一般用于停止、复位或安全退出。
 * 回调中不要调用 delay、printf 大量输出、等待信号量或执行耗时控制算法。
 */
#if 0

#include <stdbool.h>

/* 运行状态是“期望状态”，由控制任务周期性读取。 */
static volatile bool s_example_run_request = false;

/* 长按停止是一次性事件，读取后自动清除。 */
static volatile bool s_example_stop_request = false;

/**
 * @brief 用户键短按：切换运行请求。
 * @note 这里只改状态，不直接写 PWM。
 */
static void app_key_action_user_short(void)
{
    s_example_run_request = !s_example_run_request;
}

/**
 * @brief 用户键长按：清除运行请求，并请求控制任务立即停止。
 * @note 长按建议作为安全动作，不要设计成“继续加速”。
 */
static void app_key_action_user_long(void)
{
    s_example_run_request = false;
    s_example_stop_request = true;
}

/**
 * @brief 查询用户键短按形成的运行请求。
 * @return true 表示允许控制任务执行示例运行逻辑。
 */
bool app_key_example_get_run_request(void)
{
    return s_example_run_request;
}

/**
 * @brief 取出一次长按停止请求。
 * @return true 表示刚刚收到停止请求；读取后该请求被清除。
 */
bool app_key_example_take_stop_request(void)
{
    bool request = s_example_stop_request;
    s_example_stop_request = false;
    return request;
}

/*
 * 复制到 app_key_actions_dispatch() 的路由函数中：
 *
 * if (key->id == PRJ_KEY_ID_USER) {
 *     if (event == KEY_EVENT_SHORT_PRESS) {
 *         app_key_action_user_short();
 *         return true;
 *     }
 *     if (event == KEY_EVENT_LONG_PRESS) {
 *         app_key_action_user_long();
 *         return true;
 *     }
 * }
 */

/*
 * 复制到控制任务固定周期循环中的业务示例：
 *
 * if (app_key_example_take_stop_request()) {
 *     // 替换为项目实际的安全停止接口：
 *     // app_motor_request_stop_all();
 *     // bsp_motor_emergency_stop();
 * }
 * else if (app_key_example_get_run_request()) {
 *     // 替换为项目实际的运行接口：
 *     // app_motor_request_target_rpm(200);
 * }
 * else {
 *     // 替换为项目实际的停止/空闲接口：
 *     // app_motor_request_stop_all();
 * }
 */

#endif /* 0：用户复制示例，默认不参与编译 */

bool app_key_actions_dispatch(const key_t *key,
                              key_event_type_t event,
                              uint32_t timestamp_ms,
                              uint32_t pressed_duration_ms,
                              void *user_data)
{
    /* 当前路由不依赖时间和用户上下文；保留参数便于后续扩展。 */
    (void)timestamp_ms;
    (void)pressed_duration_ms;
    (void)user_data;

    if (key == NULL) {
        return false;
    }

    /* 菜单键：短按切换菜单项，长按进入/退出菜单。 */
    if (key->id == PRJ_KEY_ID_MENU) {
        if (event == KEY_EVENT_SHORT_PRESS) {
            app_key_action_menu_short();
            return true;
        }

        if (event == KEY_EVENT_LONG_PRESS) {
            app_key_action_menu_long();
            return true;
        }
    }

#if 0
    /*
     * 用户按键路由示例。启用上方完整示例后，把这里改为 #if 1，
     * 或直接删除 #if/#endif 后复制到实际工程中。
     */
    if (key->id == PRJ_KEY_ID_USER) {
        if (event == KEY_EVENT_SHORT_PRESS) {
            app_key_action_user_short();
            return true;
        }

        if (event == KEY_EVENT_LONG_PRESS) {
            app_key_action_user_long();
            return true;
        }
    }
#endif

    /* 返回 false 表示该按键事件没有在本层处理。 */
    return false;
}
