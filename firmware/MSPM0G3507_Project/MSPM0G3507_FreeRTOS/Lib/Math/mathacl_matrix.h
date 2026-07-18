/**
 * @file    mathacl_matrix.h
 * @brief   MATHACL 硬件加速矩阵运算库（线程安全）
 *
 * 为卡尔曼滤波（KF/EKF）提供基于 MATHACL 的定点矩阵运算加速：
 *   - 矩阵乘法 C = A × B（Q24 定点 MAC）
 *   - 矩阵转置
 *   - 2×2 / 3×3 矩阵解析求逆（Q24 定点 DIV）
 *   - 批量 float ↔ Q24 转换与精度验证
 *
 * 线程安全模型：
 *   MATHACL 为全局单例硬件，所有操作通过内部互斥锁序列化。
 *   调用者负责滤波器实例级并发（每个实例的工作缓冲区独立）。
 *   本模块既可在 FreeRTOS 任务中调用，也可在裸机中断中调用（建议 ISR 仅使用软浮点）。
 *
 * @note 当前状态: 矩阵库实现被 TEST_MATHACL_MATRIX_ENABLE=0 条件编译排除。
 *       filter.c 仅包含本头文件引入声明，未调用任何 mathacl_matrix_* 函数。
 *       如需启用硬件矩阵加速，请在工程宏中设置 TEST_MATHACL_MATRIX_ENABLE=1，
 *       并确保 mathacl_matrix_init() 在使用前调用。
 */

#ifndef MATHACL_MATRIX_H
#define MATHACL_MATRIX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "bsp_mathacl.h"  /* 提供 BSP_MATHACL_ENABLE 开关 */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 编译开关与配置
 * ============================================================================ */

/** @brief 启用 MATHACL 矩阵加速（依赖 BSP_MATHACL_ENABLE） */
#if defined(BSP_MATHACL_ENABLE)
#define MATHACL_MATRIX_ENABLE
#endif

/** @brief Q24 表示范围上限（绝对值），避免 float_to_q24 溢出 */
#define MATHACL_MATRIX_Q24_CLAMP_MAX   120.0f

/** @brief 矩阵求逆的奇异性阈值 */
#define MATHACL_MATRIX_INV_EPS         1e-12f

/** @brief 精度验证默认容差（相对软浮点结果） */
#define MATHACL_MATRIX_TOLERANCE       0.01f

/* ============================================================================
 * 错误码
 * ============================================================================ */

typedef enum {
    MATHACL_MATRIX_OK = 0,           /**< 成功 */
    MATHACL_MATRIX_ERR_NULL,         /**< 空指针 */
    MATHACL_MATRIX_ERR_RANGE,        /**< 输入超出 Q24 范围 */
    MATHACL_MATRIX_ERR_SINGULAR,     /**< 矩阵奇异 */
    MATHACL_MATRIX_ERR_SIZE,         /**< 矩阵尺寸非法 */
    MATHACL_MATRIX_ERR_NOT_INIT,     /**< 模块未初始化 */
    MATHACL_MATRIX_ERR_FAIL,         /**< 通用失败 */
} mathacl_matrix_status_t;

/* ============================================================================
 * 公共 API
 * ============================================================================ */

/**
 * @brief 初始化矩阵加速模块
 *
 * 创建 MATHACL 硬件互斥锁。必须在调度器启动前或首个任务创建后调用一次。
 * 重复调用安全（返回 OK）。
 *
 * @return MATHACL_MATRIX_OK 成功
 * @return MATHACL_MATRIX_ERR_FAIL 互斥锁创建失败
 */
mathacl_matrix_status_t mathacl_matrix_init(void);

/**
 * @brief 反初始化矩阵加速模块
 */
void mathacl_matrix_deinit(void);

/**
 * @brief 批量将浮点矩阵转换为 Q24 定点矩阵
 *
 * @param dst   输出 Q24 矩阵（行优先）
 * @param src   输入 float 矩阵（行优先）
 * @param rows  行数
 * @param cols  列数
 * @return MATHACL_MATRIX_OK 成功
 * @return MATHACL_MATRIX_ERR_NULL 空指针
 * @return MATHACL_MATRIX_ERR_RANGE 元素超出 [-128, 128)
 */
mathacl_matrix_status_t mathacl_matrix_to_q24(int32_t *dst,
                                              const float *src,
                                              int rows, int cols);

/**
 * @brief 批量将 Q24 定点矩阵转换为浮点矩阵
 *
 * @param dst   输出 float 矩阵（行优先）
 * @param src   输入 Q24 矩阵（行优先）
 * @param rows  行数
 * @param cols  列数
 * @return MATHACL_MATRIX_OK 成功
 * @return MATHACL_MATRIX_ERR_NULL 空指针
 */
mathacl_matrix_status_t mathacl_matrix_to_float(float *dst,
                                                const int32_t *src,
                                                int rows, int cols);

/**
 * @brief 浮点矩阵乘法 C = A × B（内部使用 Q24 定点 MAC 加速）
 *
 * @param C      输出 float 矩阵（行优先）
 * @param A      输入 float 矩阵 A
 * @param B      输入 float 矩阵 B
 * @param rows_a A 的行数
 * @param cols_b B 的列数
 * @param inner  A 的列数 / B 的行数
 * @return MATHACL_MATRIX_OK 成功
 * @return MATHACL_MATRIX_ERR_NULL 空指针
 * @return MATHACL_MATRIX_ERR_RANGE 元素超出 Q24 范围
 */
mathacl_matrix_status_t mathacl_matrix_mul(float *C,
                                           const float *A,
                                           const float *B,
                                           int rows_a,
                                           int cols_b,
                                           int inner);

/**
 * @brief 浮点矩阵转置 T = M^T
 *
 * @param T      输出 float 矩阵（行优先）
 * @param M      输入 float 矩阵
 * @param rows   输入行数
 * @param cols   输入列数
 * @return MATHACL_MATRIX_OK 成功
 * @return MATHACL_MATRIX_ERR_NULL 空指针
 */
mathacl_matrix_status_t mathacl_matrix_transpose(float *T,
                                                 const float *M,
                                                 int rows, int cols);

/**
 * @brief 2×2 浮点矩阵求逆（使用 MATHACL DIV 加速）
 *
 * @param inv    输出 2×2 逆矩阵
 * @param M      输入 2×2 矩阵
 * @return MATHACL_MATRIX_OK 成功
 * @return MATHACL_MATRIX_ERR_NULL 空指针
 * @return MATHACL_MATRIX_ERR_SINGULAR 矩阵奇异
 */
mathacl_matrix_status_t mathacl_matrix_inv2x2(float inv[2][2],
                                              const float M[2][2]);

/**
 * @brief 3×3 浮点矩阵求逆（使用 MATHACL DIV 加速）
 *
 * @param inv    输出 3×3 逆矩阵
 * @param M      输入 3×3 矩阵
 * @return MATHACL_MATRIX_OK 成功
 * @return MATHACL_MATRIX_ERR_NULL 空指针
 * @return MATHACL_MATRIX_ERR_SINGULAR 矩阵奇异
 */
mathacl_matrix_status_t mathacl_matrix_inv3x3(float inv[3][3],
                                              const float M[3][3]);

/**
 * @brief 验证硬件矩阵乘法结果与软浮点结果的一致性
 *
 * @param A        输入 A
 * @param B        输入 B
 * @param rows_a   A 行数
 * @param cols_b   B 列数
 * @param inner    内维
 * @param tol      容差
 * @return true 一致
 * @return false 不一致
 */
bool mathacl_matrix_verify_mul(const float *A, const float *B,
                               int rows_a, int cols_b, int inner,
                               float tol);

/**
 * @brief 获取最近一次 MATHACL 硬件操作计数（调试用）
 */
uint32_t mathacl_matrix_get_op_count(void);

/**
 * @brief 重置 MATHACL 硬件操作计数（调试用）
 */
void mathacl_matrix_reset_op_count(void);

/**
 * @brief 判断当前是否启用 MATHACL 矩阵加速
 */
bool mathacl_matrix_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* MATHACL_MATRIX_H */
