#ifndef APP_TEST_RUNNER_H
#define APP_TEST_RUNNER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file app_test_runner.h
 * @brief EKF 测试运行器 - 全面的性能评估与故障检测
 *
 * 在 IMU 任务中调用 app_test_runner_feed() 喂入每帧姿态数据,
 * 测试超时后自动停止并输出统计摘要。
 *
 * 测试模式:
 *   1. ZUPT 静态漂移测试 (zutptest N)
 *      - yaw drift/std/range/max_jump
 *      - bias_z 跟踪 (起止值/漂移/极值/单帧最大变化)
 *      - 故障检测 (NaN/Inf, 发散, 偏置突变, acc异常)
 *      - 综合判定 (PASS/FAIL/WARN)
 *
 *   2. 动态转动精度测试 (turndtest ANG [T])
 *      - 角度误差 (actual vs target)
 *      - 分段统计:
 *        TURN 阶段: yaw 增长率, maneuver_level 分布 (验证慢转 yaw 正常增长)
 *        STABLE 阶段: yaw 漂移, drift_rate (验证"停下后yaw变动"修复)
 *      - bias_z 门控验证 (转动中偏置应保持不变)
 *      - 故障检测 + 综合判定
 *
 * 启动方式:
 *   1. 串口发送 "zutptest 60" 启动 60 秒静态测试
 *   2. 串口发送 "turndtest 90" 启动 90° 转动测试, 转到位后发送 "turnend"
 */

/**
 * @brief  EKF 诊断上下文 (用于跳变根因定位)
 * @note   仅在测试活跃时由 feed_diag 填充, 字段为 0 表示无效
 */
typedef struct {
    float    gyro_mag_dps;    /**< 陀螺幅值 (dps) */
    float    acc_norm_err;    /**< ACC 幅值偏差 (g) */
    float    acc_norm;        /**< ACC 幅值 (g), 用于门限诊断 */
    float    kf_bias_x;       /**< KF X轴偏置估计 (dps) */
    float    kf_bias_y;       /**< KF Y轴偏置估计 (dps) */
    float    kf_bias_z;       /**< KF Z轴偏置估计 (dps) */
    float    kf_p00_x;        /**< KF X轴角度协方差 */
    float    kf_p00_y;        /**< KF Y轴角度协方差 */
    float    kf_p00_z;        /**< KF Z轴角度协方差 */
    float    kf_p11_x;        /**< KF X轴偏置协方差 */
    float    kf_p11_y;        /**< KF Y轴偏置协方差 */
    float    kf_p11_z;        /**< KF Z轴偏置协方差 */
} test_diag_ctx_t;

/**
 * @brief  启动静态 ZUPT 漂移测试
 * @param  duration_sec  测试时长 (秒)
 * @return 0=成功, -1=失败
 */
int app_test_runner_start(uint32_t duration_sec);

/**
 * @brief  启动动态转动精度测试
 * @param  target_angle  目标转动角度 (度, 正=逆时针/CCW)
 * @param  timeout_sec   超时 (秒, 默认建议 60)
 * @return 0=成功, -1=失败
 * @note   测试流程:
 *           1. 静止参考阶段 (2s): 记录起始 yaw, 等待 ZUPT 稳定
 *           2. 转动检测: 检测 gyro_mag > 阈值 → 转动开始
 *           3. 手动结束: 用户发送 "turnend" 命令确认转动结束
 *           4. 稳定阶段 (2s): 等待 ZUPT 重新收敛
 *           5. 输出: 实际转动角度 vs 目标角度, 误差, 判定 PASS/FAIL
 *         使用方式: 串口 "turndtest 90" 启动, 转到目标角度后发送 "turnend"
 */
int app_test_runner_start_turn(float target_angle, uint32_t timeout_sec);

/**
 * @brief  手动确认转动结束 (TURN 模式专用)
 * @note   在 turndtest 启动后, 转到目标角度时发送此命令
 *         如果当前不在 TURNING 状态, 忽略
 */
void app_test_runner_end_turn(void);

/**
 * @brief  启动 KF 参数扫描测试
 * @param  duration_per_set  每组参数测试时长 (秒, 建议 10-30)
 * @return 0=成功, -1=失败
 * @note   扫描 Q_angle × Q_bias 组合 (共 9 组), 固定 R_measure=0.03, R_zupt=0.04
 *         每组测试后输出 yaw drift/std/max_jump, 最终输出汇总表和最优参数
 *         测试期间保持设备静止!
 *         使用方式: 串口 "kftune 10"
 */
int app_test_runner_start_kftune(uint32_t duration_per_set);

/**
 * @brief  主动停止测试 (提前结束)
 */
void app_test_runner_stop(void);

/**
 * @brief  喂入每帧姿态数据 (带 EKF 诊断上下文)
 * @param  pitch  俯仰角 (度)
 * @param  roll   横滚角 (度)
 * @param  yaw    偏航角 (度)
 * @param  diag   EKF 诊断上下文 (可为 NULL, 仅 pitch/roll/yaw 时不诊断)
 * @note   仅在测试活跃时记录数据, 非活跃时直接返回
 *         当 max_jump 发生时, 会记录该帧的诊断上下文用于根因定位
 */
void app_test_runner_feed_diag(float pitch, float roll, float yaw,
                                const test_diag_ctx_t *diag);

/**
 * @brief  喂入每帧姿态数据 (兼容旧接口, 无诊断)
 * @param  pitch  俯仰角 (度)
 * @param  roll   横滚角 (度)
 * @param  yaw    偏航角 (度)
 * @note   仅在测试活跃时记录数据, 非活跃时直接返回
 */
void app_test_runner_feed(float pitch, float roll, float yaw);

/**
 * @brief  轮询检查是否超时 (在 IMU 任务主循环中调用)
 * @note   超时后自动调用 stop() 输出统计
 */
void app_test_runner_poll(void);

/**
 * @brief  检查测试是否活跃
 * @return true=测试进行中
 */
bool app_test_runner_is_active(void);

#endif /* APP_TEST_RUNNER_H */
