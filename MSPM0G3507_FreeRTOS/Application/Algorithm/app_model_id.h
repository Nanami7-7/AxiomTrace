/**
 * @file    app_model_id.h
 * @brief   电机模型参数辨识接口(纯算法,无硬件依赖)
 * @note    支持阶跃响应法辨识直流电机一阶模型:
 *          G(s) = K / (T*s + 1)
 *
 *          Phase 1: 阶跃响应法 (Step Response)
 *            - 63.2% 穿越点法 + 线性插值
 *            - 拟合质量评估 (NRMSE)
 *          Phase 2: ARX + PRBS 最小二乘法 (预留)
 *
 *          应用场景:
 *          - 自动辨识 K 和 T
 *          - 极点配置 PI 控制器自动整定
 *          - 动态前馈控制器设计
 */
#ifndef APP_MODEL_ID_H
#define APP_MODEL_ID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ======================== 常量 ======================== */

/** 阶跃响应最大采样数(5ms周期×400=2000ms, 覆盖约10倍T) */
#define ID_STEP_MAX_SAMPLES     (400U)

/** ARX/PRBS 最大采样数(5ms周期×600=3000ms) */
#define ID_PRBS_MAX_SAMPLES     (600U)

/** 预停等待: 电机静止判定阈值(RPM) */
#define ID_STOP_RPM_THRESH      (5.0f)

/** 预停等待: 静止确认周期数(5ms×20=100ms) */
#define ID_STOP_CONFIRM_CYCLES  (20U)

/**
 * 默认模型辨识阶跃命令。
 * 该值是bsp_drv8870_set_speed()的有符号业务命令，不是0~1000绝对compare。
 * 300在当前40%~55%死区映射下约对应正向82%或反向16%的绝对占空比。
 */
#define ID_DEFAULT_PWM_STEP     (300)

/** 最小有效有符号命令；驱动会自动将其映射到机械死区之外。 */
#define ID_MIN_PWM              (80)

/**
 * 有符号速度命令范围，必须与PRJ_DRV8870_SPEED_COMMAND_MAX保持一致。
 * 本模块保持纯算法依赖，因此由app_main.c的编译期检查防止配置漂移。
 */
#define ID_PWM_MAX              (500)
#define ID_PWM_MIN              (-500)

/** 电机数量(必须与 bsp_drv8870.h 中 BSP_DRV8870_COUNT 保持一致)
 *  本模块设计为纯算法、无硬件依赖，故独立定义而非引用 bsp_drv8870.h */
#define ID_MOTOR_COUNT          (4)

/* ======================== 类型定义 ======================== */

/** ID 状态机(控制任务与菜单任务共享) */
typedef enum {
    ID_STATE_IDLE = 0,          /**< 空闲: 正常PID控制 */
    ID_STATE_STEP_PRE_STOP,     /**< 阶跃前: 等待电机停止 */
    ID_STATE_STEP_RECORDING,    /**< 阶跃中: 记录响应数据 */
    ID_STATE_PRBS_RECORDING,    /**< PRBS中: 预留, Phase 2 */
} app_id_state_t;

/** 控制任务周期输出(告诉调用者该做什么动作) */
typedef enum {
    ID_ACTION_NONE = 0,      /**< 无特殊动作(返回正常PID) */
    ID_ACTION_BRAKE,         /**< 需要制动电机 */
    ID_ACTION_APPLY_PWM,     /**< 需要施加指定PWM */
} app_id_action_t;

/** 控制任务周期输出参数 */
typedef struct {
    app_id_action_t action;   /**< 执行动作 */
    int32_t         pwm;      /**< 要施加的PWM值 (action==APPLY_PWM时有效) */
    uint32_t        motor_id; /**< 目标电机索引 */
} app_id_cycle_out_t;

/** 阶跃响应辨识结果 */
typedef struct {
    float K;               /**< 直流增益 (RPM/PWM) */
    float T_s;             /**< 机电时间常数 (秒) */
    float omega_ss;        /**< 稳态转速 (RPM) */
    float fit_quality;     /**< 拟合质量 0~1 (1=完美一阶拟合) */
    uint32_t sample_count; /**< 有效采样点数 */
} app_id_step_result_t;

/** ARX 辨识结果(预留 Phase 2) */
typedef struct {
    float a1;
    float b1;
    float K;
    float T_s;
    float fit_quality;
    uint32_t sample_count;
} app_id_arx_result_t;

/* ======================== 全局变量 ======================== */

/** 阶跃响应数据缓冲区(控制任务写入, 菜单任务读取) */
typedef struct {
    volatile uint32_t write_idx;     /**< 已写入样本数, ==最大值时采集完成 */
    volatile bool     done;         /**< 数据采集完成标志(控制任务置位, 菜单任务读取) */
    float             rpm_buf[ID_STEP_MAX_SAMPLES];
    int32_t           pwm_buf[ID_STEP_MAX_SAMPLES];
    app_id_step_result_t result;
} app_id_data_t;

extern app_id_data_t g_id_data;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化ID模块(清零缓冲区, 重置状态)
 */
void app_id_init(void);

/**
 * @brief  启动阶跃响应测试(由菜单任务调用)
 * @param  motor_id   目标电机索引
 * @param  pwm_step   阶跃PWM值(0~1000)
 * @note   设置内部状态为 PRE_STOP, 控制任务将接管电机
 */
void app_id_start_step(uint32_t motor_id, int32_t pwm_step);

/**
 * @brief  中止正在进行的辨识测试
 */
void app_id_abort(void);

/**
 * @brief  控制任务周期调用(纯算法, 不操作硬件)
 * @param  rpm  当前4路电机RPM数组
 * @param  out  输出: 本周期应执行的动作
 *
 * @note   调用者(task_control.c)根据 out 的内容决定:
 *         - ID_ACTION_NONE:    保持正常PID控制
 *         - ID_ACTION_BRAKE:   调用 bsp_drv8870_stop(motor_id, BRAKE)
 *         - ID_ACTION_APPLY_PWM: 调用 bsp_drv8870_set_speed(motor_id, pwm)
 *         同时跳过目标电机的PID计算
 */
void app_id_control_cycle(const int32_t *rpm, app_id_cycle_out_t *out);

/**
 * @brief  处理阶跃响应数据, 提取 K 和 T
 * @param  rpm_buf   转速样本数组
 * @param  count     有效样本数
 * @param  pwm_step  施加的阶跃PWM值
 * @param  dt_s      采样周期(秒)
 * @param  result    输出: 辨识结果
 * @retval true      辨识成功
 * @retval false     数据不足或异常
 *
 * @note   算法步骤:
 *         1. 稳态转速 = 最后25%样本均值
 *         2. K = omega_ss / pwm_step
 *         3. T = 线性插值找到 0.632*omega_ss 的穿越时间
 *         4. 拟合质量 = 1 - NRMSE
 */
bool app_id_process_step(const float *rpm_buf, uint32_t count,
                          int32_t pwm_step, float dt_s,
                          app_id_step_result_t *result);

/**
 * @brief  极-零对消 PI 控制器整定
 * @param  id           辨识结果
 * @param  bandwidth_hz 期望闭环带宽(Hz)
 * @param  damping      阻尼比(预留, 当前使用临界阻尼)
 * @param  kp_out       输出: 比例增益
 * @param  ki_out       输出: 积分增益
 * @retval true         整定成功
 * @retval false        参数无效
 *
 * @note   公式: Ti = T (零极点对消)
 *         tau_cl = 1/(2*pi*bw) (期望闭环时间常数)
 *         Kp = T/(K * tau_cl)
 *         Ki = 1/(K * tau_cl)
 *         tau_cl 自动限幅: [10ms, T/2]
 */
bool app_id_pole_placement(const app_id_step_result_t *id,
                            float bandwidth_hz, float damping,
                            float *kp_out, float *ki_out);

#ifdef __cplusplus
}
#endif

#endif /* APP_MODEL_ID_H */
