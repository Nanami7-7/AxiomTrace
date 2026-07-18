/**
 * @file    mathacl_matrix.c
 * @brief   MATHACL 硬件加速矩阵运算库实现（线程安全）
 */

#include "mathacl_matrix.h"
#include "bsp_mathacl.h"
#include "osal_api.h"
#include <string.h>
#include <math.h>

#if TEST_MATHACL_MATRIX_ENABLE

/* ============================================================================
 * 模块级状态（最小化全局状态，受互斥锁保护）
 * ============================================================================ */

/** @brief 保护 MATHACL 单例硬件的互斥锁 */
static osal_mutex_t s_mathacl_mtx = NULL;

/** @brief 模块初始化标志 */
static volatile bool s_initialized = false;

/** @brief 硬件操作计数（用于验证） */
static volatile uint32_t s_op_count = 0U;

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 获取 MATHACL 硬件锁
 */
static void mathacl_lock(void)
{
    if (s_mathacl_mtx != NULL) {
        osal_mutex_lock(s_mathacl_mtx);
    }
}

/**
 * @brief 释放 MATHACL 硬件锁
 */
static void mathacl_unlock(void)
{
    if (s_mathacl_mtx != NULL) {
        osal_mutex_unlock(s_mathacl_mtx);
    }
}

/**
 * @brief 浮点转 Q24，带范围检查
 */
static int32_t f2q24_clamp(float x)
{
    if (x > MATHACL_MATRIX_Q24_CLAMP_MAX) {
        x = MATHACL_MATRIX_Q24_CLAMP_MAX;
    } else if (x < -MATHACL_MATRIX_Q24_CLAMP_MAX) {
        x = -MATHACL_MATRIX_Q24_CLAMP_MAX;
    }
    return float_to_q24(x);
}

/**
 * @brief 检查矩阵元素是否全部在 Q24 安全范围内
 */
static bool is_in_q24_range(const float *M, int count)
{
    for (int i = 0; i < count; i++) {
        float v = M[i];
        if (v > MATHACL_MATRIX_Q24_CLAMP_MAX ||
            v < -MATHACL_MATRIX_Q24_CLAMP_MAX ||
            isnan(v) || isinf(v)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 内部硬件点积（调用者已持有锁）
 */
static int32_t dot_q24_locked(const int32_t *a, const int32_t *b, int len)
{
    mathacl_mac_init();
    for (int k = 0; k < len; k++) {
        mathacl_mac_acc(a[k], b[k]);
    }
    s_op_count++;
    return mathacl_mac_result();
}

/**
 * @brief 内部硬件除法（调用者已持有锁）
 */
static float div_hw_locked(float num, float den)
{
    if (fabsf(den) < MATHACL_MATRIX_INV_EPS || isnan(num) || isinf(num) ||
        isnan(den) || isinf(den)) {
        return 0.0f;
    }
    if (fabsf(num) > MATHACL_MATRIX_Q24_CLAMP_MAX ||
        fabsf(den) > MATHACL_MATRIX_Q24_CLAMP_MAX) {
        return num / den;
    }
    /* 输出范围保护：商必须仍在 Q24 范围内 */
    if (fabsf(den) * MATHACL_MATRIX_Q24_CLAMP_MAX < fabsf(num)) {
        return num / den;
    }
    int32_t nq = float_to_q24(num);
    int32_t dq = float_to_q24(den);
    int32_t rq = mathacl_div_q24(nq, dq);
    s_op_count++;
    return q24_to_float(rq);
}

/**
 * @brief 软浮点矩阵乘法（用于回退与验证）
 */
static void soft_matmul(float *C, const float *A, const float *B,
                        int rows_a, int cols_b, int inner)
{
    for (int i = 0; i < rows_a; i++) {
        for (int j = 0; j < cols_b; j++) {
            float sum = 0.0f;
            for (int k = 0; k < inner; k++) {
                sum += A[i * inner + k] * B[k * cols_b + j];
            }
            C[i * cols_b + j] = sum;
        }
    }
}

/* ============================================================================
 * 公共 API 实现
 * ============================================================================ */

mathacl_matrix_status_t mathacl_matrix_init(void)
{
    if (s_initialized) {
        return MATHACL_MATRIX_OK;
    }
    s_mathacl_mtx = osal_mutex_create();
    if (s_mathacl_mtx == NULL) {
        return MATHACL_MATRIX_ERR_NOT_INIT;
    }
    s_initialized = true;
    s_op_count = 0U;
    return MATHACL_MATRIX_OK;
}

void mathacl_matrix_deinit(void)
{
    if (s_initialized && s_mathacl_mtx != NULL) {
        osal_mutex_delete(s_mathacl_mtx);
        s_mathacl_mtx = NULL;
    }
    s_initialized = false;
    s_op_count = 0U;
}

bool mathacl_matrix_is_enabled(void)
{
#if defined(MATHACL_MATRIX_ENABLE)
    return s_initialized;
#else
    return false;
#endif
}

mathacl_matrix_status_t mathacl_matrix_to_q24(int32_t *dst,
                                              const float *src,
                                              int rows, int cols)
{
    if (dst == NULL || src == NULL) {
        return MATHACL_MATRIX_ERR_NULL;
    }
    if (rows <= 0 || cols <= 0) {
        return MATHACL_MATRIX_ERR_SIZE;
    }
    int count = rows * cols;
    if (!is_in_q24_range(src, count)) {
        return MATHACL_MATRIX_ERR_RANGE;
    }
    for (int i = 0; i < count; i++) {
        dst[i] = float_to_q24(src[i]);
    }
    return MATHACL_MATRIX_OK;
}

mathacl_matrix_status_t mathacl_matrix_to_float(float *dst,
                                                const int32_t *src,
                                                int rows, int cols)
{
    if (dst == NULL || src == NULL) {
        return MATHACL_MATRIX_ERR_NULL;
    }
    if (rows <= 0 || cols <= 0) {
        return MATHACL_MATRIX_ERR_SIZE;
    }
    int count = rows * cols;
    for (int i = 0; i < count; i++) {
        dst[i] = q24_to_float(src[i]);
    }
    return MATHACL_MATRIX_OK;
}

mathacl_matrix_status_t mathacl_matrix_mul(float *C,
                                           const float *A,
                                           const float *B,
                                           int rows_a,
                                           int cols_b,
                                           int inner)
{
    if (C == NULL || A == NULL || B == NULL) {
        return MATHACL_MATRIX_ERR_NULL;
    }
    if (rows_a <= 0 || cols_b <= 0 || inner <= 0) {
        return MATHACL_MATRIX_ERR_SIZE;
    }
#if !defined(MATHACL_MATRIX_ENABLE)
    soft_matmul(C, A, B, rows_a, cols_b, inner);
    return MATHACL_MATRIX_OK;
#else
    if (!s_initialized) {
        return MATHACL_MATRIX_ERR_NOT_INIT;
    }
    int count_a = rows_a * inner;
    int count_b = inner * cols_b;
    if (!is_in_q24_range(A, count_a) || !is_in_q24_range(B, count_b)) {
        /* 范围超限：安全回退软浮点 */
        soft_matmul(C, A, B, rows_a, cols_b, inner);
        return MATHACL_MATRIX_OK;
    }

    /* 使用栈上临时缓冲区（MATHACL 常见最大 7x7） */
    int32_t A_q24[49];
    int32_t BT_q24[49];
    if (rows_a * inner > 49 || inner * cols_b > 49) {
        /* 超过栈缓冲区容量，回退软浮点 */
        soft_matmul(C, A, B, rows_a, cols_b, inner);
        return MATHACL_MATRIX_OK;
    }

    for (int i = 0; i < count_a; i++) {
        A_q24[i] = float_to_q24(A[i]);
    }
    /* 转置 B 以优化 MAC 行/列访问 */
    for (int i = 0; i < inner; i++) {
        for (int j = 0; j < cols_b; j++) {
            BT_q24[j * inner + i] = float_to_q24(B[i * cols_b + j]);
        }
    }

    mathacl_lock();
    for (int i = 0; i < rows_a; i++) {
        for (int j = 0; j < cols_b; j++) {
            int32_t rq = dot_q24_locked(&A_q24[i * inner],
                                        &BT_q24[j * inner], inner);
            C[i * cols_b + j] = q24_to_float(rq);
        }
    }
    mathacl_unlock();
    return MATHACL_MATRIX_OK;
#endif
}

mathacl_matrix_status_t mathacl_matrix_transpose(float *T,
                                               const float *M,
                                               int rows, int cols)
{
    if (T == NULL || M == NULL) {
        return MATHACL_MATRIX_ERR_NULL;
    }
    if (rows <= 0 || cols <= 0) {
        return MATHACL_MATRIX_ERR_SIZE;
    }
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            T[j * rows + i] = M[i * cols + j];
        }
    }
    return MATHACL_MATRIX_OK;
}

mathacl_matrix_status_t mathacl_matrix_inv2x2(float inv[2][2],
                                              const float M[2][2])
{
    if (inv == NULL || M == NULL) {
        return MATHACL_MATRIX_ERR_NULL;
    }
    float a = M[0][0], b = M[0][1];
    float c = M[1][0], d = M[1][1];
    float det = a * d - b * c;
    if (fabsf(det) < MATHACL_MATRIX_INV_EPS) {
        return MATHACL_MATRIX_ERR_SINGULAR;
    }

#if defined(MATHACL_MATRIX_ENABLE)
    if (s_initialized) {
        mathacl_lock();
        float inv_det = div_hw_locked(1.0f, det);
        mathacl_unlock();
        inv[0][0] =  d * inv_det;
        inv[0][1] = -b * inv_det;
        inv[1][0] = -c * inv_det;
        inv[1][1] =  a * inv_det;
        return MATHACL_MATRIX_OK;
    }
#endif

    float inv_det = 1.0f / det;
    inv[0][0] =  d * inv_det;
    inv[0][1] = -b * inv_det;
    inv[1][0] = -c * inv_det;
    inv[1][1] =  a * inv_det;
    return MATHACL_MATRIX_OK;
}

mathacl_matrix_status_t mathacl_matrix_inv3x3(float inv[3][3],
                                              const float M[3][3])
{
    if (inv == NULL || M == NULL) {
        return MATHACL_MATRIX_ERR_NULL;
    }
    /* 计算伴随矩阵元素 */
    float c00 = M[1][1] * M[2][2] - M[1][2] * M[2][1];
    float c01 = M[1][2] * M[2][0] - M[1][0] * M[2][2];
    float c02 = M[1][0] * M[2][1] - M[1][1] * M[2][0];
    float c10 = M[0][2] * M[2][1] - M[0][1] * M[2][2];
    float c11 = M[0][0] * M[2][2] - M[0][2] * M[2][0];
    float c12 = M[0][1] * M[2][0] - M[0][0] * M[2][1];
    float c20 = M[0][1] * M[1][2] - M[0][2] * M[1][1];
    float c21 = M[0][2] * M[1][0] - M[0][0] * M[1][2];
    float c22 = M[0][0] * M[1][1] - M[0][1] * M[1][0];

    float det = M[0][0] * c00 + M[0][1] * c01 + M[0][2] * c02;
    if (fabsf(det) < MATHACL_MATRIX_INV_EPS) {
        return MATHACL_MATRIX_ERR_SINGULAR;
    }

    float inv_det;
#if defined(MATHACL_MATRIX_ENABLE)
    if (s_initialized) {
        mathacl_lock();
        inv_det = div_hw_locked(1.0f, det);
        mathacl_unlock();
    } else
#endif
    {
        inv_det = 1.0f / det;
    }

    inv[0][0] = c00 * inv_det;
    inv[0][1] = c10 * inv_det;
    inv[0][2] = c20 * inv_det;
    inv[1][0] = c01 * inv_det;
    inv[1][1] = c11 * inv_det;
    inv[1][2] = c21 * inv_det;
    inv[2][0] = c02 * inv_det;
    inv[2][1] = c12 * inv_det;
    inv[2][2] = c22 * inv_det;
    return MATHACL_MATRIX_OK;
}

bool mathacl_matrix_verify_mul(const float *A, const float *B,
                               int rows_a, int cols_b, int inner,
                               float tol)
{
    if (A == NULL || B == NULL || rows_a <= 0 ||
        cols_b <= 0 || inner <= 0) {
        return false;
    }
    float C_hw[49];
    float C_sw[49];
    int out_count = rows_a * cols_b;
    if (out_count > 49) {
        return false;
    }

    (void)mathacl_matrix_mul(C_hw, A, B, rows_a, cols_b, inner);
    soft_matmul(C_sw, A, B, rows_a, cols_b, inner);

    for (int i = 0; i < out_count; i++) {
        float err = fabsf(C_hw[i] - C_sw[i]);
        float ref = fabsf(C_sw[i]) + 1e-6f;
        if (err > tol * ref) {
            return false;
        }
    }
    return true;
}

uint32_t mathacl_matrix_get_op_count(void)
{
    return s_op_count;
}

void mathacl_matrix_reset_op_count(void)
{
    s_op_count = 0U;
}

#endif /* TEST_MATHACL_MATRIX_ENABLE */
