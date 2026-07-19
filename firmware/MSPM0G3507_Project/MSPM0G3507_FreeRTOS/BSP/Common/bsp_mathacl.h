/**
 * @file    bsp_mathacl.h
 * @brief   MATHACL 硬件加速器 BSP 封装 (使用 TI driverlib)
 *
 * 封装 MSPM0G3507 的 MATHACL 硬件加速器，
 * 提供 SQRT、SINCOS、ATAN2、MAC 等硬件加速函数。
 *
 * 特点：
 *   - 使用 TI driverlib 确保寄存器访问正确
 *   - 支持 Keil ARMCC 工具链
 *   - 可选编译开关 BSP_MATHACL_ENABLE
 *   - 提供软浮点回退路径
 */

#ifndef BSP_MATHACL_H
#define BSP_MATHACL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 编译开关
 * ============================================================================ */

/**
 * @brief MATHACL 加速使能开关
 * 
 * 定义此宏以启用 MATHACL 硬件加速。
 * 未定义时使用软浮点回退路径。
 */
#ifndef BSP_MATHACL_ENABLE
#define BSP_MATHACL_ENABLE   /* 启用 MATHACL 硬件加速 */
#endif

/**
 * @brief 选择性硬件加速开关
 * 
 * 性能测试结果:
 *   SQRT:  hw 0.86x (慢) → 默认用软件
 *   ATAN2: hw 1.26x (快) → 默认用硬件
 *   SINCOS: hw 2.51x (快) → 默认用硬件
 *   ASIN:  hw 0.84x (慢) → 默认用软件 (内部调用 SQRT+ATAN2)
 */
#ifdef BSP_MATHACL_ENABLE
  #define BSP_MATHACL_ATAN2_HW    /* ATAN2 用硬件 (1.26x) */
  #define BSP_MATHACL_SINCOS_HW   /* SINCOS 用硬件 (2.51x) */
  /* SQRT 和 ASIN 用软件 (硬件更慢) */
#endif

/**
 * @brief 线程安全开关
 * 
 * 定义此宏以启用 MATHACL 硬件操作的线程安全保护。
 * 需要包含 OSAL (osal_api.h)。使用临界区保护寄存器访问。
 * 未定义时: 仅单任务安全 (当前默认行为, 兼容裸机初始化)
 * 定义后: 多任务安全 (临界区开销约 10-20 cycles)
 */
/* #define BSP_MATHACL_THREAD_SAFE */

/* ============================================================================
 * MATHACL 寄存器定义 (使用 TI driverlib)
 * ============================================================================ */

#include "ti/devices/msp/peripherals/hw_mathacl.h"

/** @brief MATHACL 基址 (可能已由 TI 设备头文件定义) */
#ifndef MATHACL_BASE
#define MATHACL_BASE            0x40410000UL
#endif

/** @brief MATHACL 寄存器指针 (使用 TI 类型)
 *  注意: TI 设备头文件 (mspm0g350x.h) 会将 MATHACL 定义为 static 变量。
 *  如果 TI 头文件已定义，此处跳过以避免冲突。 */
#ifndef MATHACL
#define MATHACL                 ((MATHACL_Regs *)MATHACL_BASE)
#endif

/* ============================================================================
 * 功能码定义 (使用 TI driverlib)
 * ============================================================================ */

/* 直接使用 hw_mathacl.h 中的定义，无需重复定义 */

/* ============================================================================
 * Q 格式定义 (使用 TI driverlib)
 * ============================================================================ */

/* 直接使用 hw_mathacl.h 中的定义，无需重复定义 */

/* ============================================================================
 * 硬件操作计数器（调试用，可选）
 * 
 * 注意: 由于本库为 header-only, g_mathacl_op_count 为 static 变量,
 *       每个包含本头文件的编译单元独立一份。无法跨文件聚合统计。
 *       如需全局统计, 请使用 mathacl_get_op_count() 在各编译单元分别查询。
 * ============================================================================ */

#ifdef BSP_MATHACL_ENABLE
/** @brief 当前编译单元内 MATHACL 启动次数计数器 (每编译单元独立) */
static volatile uint32_t g_mathacl_op_count = 0U;

/**
 * @brief 获取当前编译单元内 MATHACL 启动次数
 */
static inline uint32_t mathacl_get_op_count(void)
{
    return g_mathacl_op_count;
}

/**
 * @brief 重置当前编译单元内 MATHACL 启动次数
 */
static inline void mathacl_reset_op_count(void)
{
    g_mathacl_op_count = 0U;
}
#endif /* BSP_MATHACL_ENABLE */


/** @brief SQRT 配置: 31次迭代, QVAL=0 (TI 官方: QVAL 仅 DIV 使用) */
#define MATHACL_SQRT_CFG        (MATHACL_CTL_FUNC_SQRT | (31U << 24))

/* ============================================================
 * Q 定点格式缩放因子(浮点常量, 用于 float↔Q24/Q31/Q32 转换)
 * ============================================================ */

/** Q24 缩放因子: 2^24 = 16777216.0 */
#define MATHACL_Q24_SCALE_F     (16777216.0f)
/** Q31 缩放因子: 2^31 = 2147483648.0 */
#define MATHACL_Q31_SCALE_F     (2147483648.0f)
/** Q31 上限(避免 INT32_MIN): 2147483647.0 */
#define MATHACL_Q31_MAX_F       (2147483647.0f)
/** Q32 缩放因子: 2^32 = 4294967296.0 */
#define MATHACL_Q32_SCALE_F     (4294967296.0f)

/** @brief SINCOS 配置: 31次迭代, QVAL=0 (TI 官方: QVAL 仅 DIV 使用) */
#define MATHACL_SINCOS_CFG      (MATHACL_CTL_FUNC_SINCOS | (31U << 24))

/** @brief ATAN2 配置: 31次迭代, QVAL=0 (TI 官方: QVAL 仅 DIV 使用) */
#define MATHACL_ATAN2_CFG       (MATHACL_CTL_FUNC_ATAN2 | (31U << 24))

/** @brief MAC 配置: Q24, 有符号, 饱和使能 */
#define MATHACL_MAC_CFG         (MATHACL_CTL_FUNC_MAC | MATHACL_CTL_QVAL_Q24 | \
                                 MATHACL_CTL_OPTYPE_SIGNED | MATHACL_CTL_SATEN_ENABLE)

/** @brief DIV 配置: Q24, 有符号 */
#define MATHACL_DIV_CFG         (MATHACL_CTL_FUNC_DIV | MATHACL_CTL_QVAL_Q24 | \
                                 MATHACL_CTL_OPTYPE_SIGNED)

/* ============================================================================
 * 工具函数 (必须在核心函数之前定义)
 * ============================================================================ */

/**
 * @brief 浮点数转 Q24 定点数
 */
static inline int32_t float_to_q24(float x)
{
    return (int32_t)(x * MATHACL_Q24_SCALE_F);
}

/**
 * @brief Q24 定点数转浮点数
 */
static inline float q24_to_float(int32_t x)
{
    return (float)x / MATHACL_Q24_SCALE_F;
}

/**
 * @brief 浮点数转 Q31 定点数
 * 
 * 注意: Q31 范围为 [-1.0, 1.0)，当 x = ±1.0 时会溢出 INT32。
 * 本函数会钳位到 INT32_MIN+1/INT32_MAX 避免未定义行为。
 */
static inline int32_t float_to_q31(float x)
{
    float scaled = x * MATHACL_Q31_SCALE_F;
    if (scaled >= MATHACL_Q31_MAX_F) return 0x7FFFFFFF;   /* +1.0 - ε */
    if (scaled <= -MATHACL_Q31_MAX_F) return 0x80000001;   /* -1.0 + ε, 避免 INT32_MIN */
    return (int32_t)scaled;
}

/**
 * @brief Q31 定点数转浮点数
 */
static inline float q31_to_float(int32_t x)
{
    return (float)x / MATHACL_Q31_SCALE_F;
}

/* ============================================================================
 * 内联辅助函数
 * ============================================================================ */

/**
 * @brief 等待 MATHACL 操作完成
 */
static inline void mathacl_wait_done(void)
{
    while (MATHACL->STATUS & MATHACL_STATUS_BUSY_MASK) {
        /* 等待硬件完成 */
    }
}

/**
 * @brief 检查 MATHACL 运算状态
 * @return 0=无错误, 非0=错误位掩码(OVF/UF/ERR/DIVBY0)
 */
static inline uint32_t mathacl_get_status(void)
{
    return MATHACL->STATUS & (MATHACL_STATUS_OVF_MASK |
                              MATHACL_STATUS_UF_MASK |
                              MATHACL_STATUS_ERR_MASK);
}

/**
 * @brief 清除 MATHACL 状态标志
 */
static inline void mathacl_clear_status(void)
{
    MATHACL->STATUSCLR = MATHACL_STATUSCLR_CLR_OVF_CLR | 
                         MATHACL_STATUSCLR_CLR_UF_CLR | 
                         MATHACL_STATUSCLR_CLR_ERR_CLR;
}

/**
 * @brief 等待完成并检查错误状态
 * @return 0=成功, -1=检测到硬件错误(OVF/UF/ERR)
 */
static inline int mathacl_wait_and_check(void)
{
    mathacl_wait_done();
    uint32_t err = mathacl_get_status();
    if (err) {
        mathacl_clear_status();
        return -1;
    }
    return 0;
}

/* ============================================================================
 * 线程安全支持 (可选)
 * 
 * 当定义 BSP_MATHACL_THREAD_SAFE 时, 使用临界区保护硬件寄存器访问。
 * 临界区保护范围: CTL/OP2/OP1 写入 → STATUS 读取 → RES 读取的完整序列。
 * 注意: mathacl_init() 不需要保护 (在调度器启动前调用)。
 * ============================================================================ */

#ifdef BSP_MATHACL_THREAD_SAFE
#include "osal_api.h"

/** @brief 进入 MATHACL 临界区 */
#define MATHACL_LOCK()   osal_critical_enter()
/** @brief 退出 MATHACL 临界区 */
#define MATHACL_UNLOCK() osal_critical_exit()
#else
/** @brief 无线程安全保护 (单任务模式) */
#define MATHACL_LOCK()   ((void)0)
/** @brief 无线程安全保护 (单任务模式) */
#define MATHACL_UNLOCK() ((void)0)
#endif

/* ============================================================================
 * 核心运算函数
 * ============================================================================ */

#ifdef BSP_MATHACL_ENABLE

/**
 * @brief 初始化 MATHACL 外设
 * 
 * 参照 TI SysConfig 生成的标准 MSPM0 GPRCM 初始化序列：
 *   1. 先复位 (外设未上电时清空残留状态)
 *   2. 再使能电源
 *   3. 等待电源稳定
 * 
 * 关键：RESETASSERT 必须在 PWREN 之前写入！
 * 错误顺序（先PWREN后RSTCTL）会导致外设无法正常工作。
 * 
 * 应在系统初始化时调用一次。
 */
static inline void mathacl_init(void)
{
    /* 步骤1: 复位 (外设未上电时，清空残留状态) */
    MATHACL->GPRCM.RSTCTL = MATHACL_RSTCTL_RESETASSERT_ASSERT | MATHACL_RSTCTL_KEY_UNLOCK_W;

    /* 步骤2: 使能电源 */
    MATHACL->GPRCM.PWREN = MATHACL_PWREN_KEY_UNLOCK_W | MATHACL_PWREN_ENABLE_ENABLE;

    /* 步骤3: 等待电源稳定 (POWER_STARTUP_DELAY = 16 cycles on MSPM0)
     * 保守取 32 次循环 (远超 16 cycles, 确保电源稳定) */
    for (volatile int i = 0; i < 32; i++) {
        __asm volatile("nop");
    }
}

/**
 * @brief 检查 MATHACL 是否能正常响应
 *
 * 通过一次简单的有符号 MPY32 运算 (1.0 * 1.0 = 1.0) 探测硬件是否存活。
 *
 * @return true  硬件响应正常
 * @return false 硬件无响应
 */
static inline bool mathacl_is_responding(void)
{
    MATHACL->CTL = MATHACL_CTL_FUNC_MPY32 | MATHACL_CTL_QVAL_Q24 |
                   MATHACL_CTL_OPTYPE_SIGNED;
    MATHACL->OP2 = (uint32_t)float_to_q24(1.0f);
    MATHACL->OP1 = (uint32_t)float_to_q24(1.0f);
    mathacl_wait_done();

    return (MATHACL->RES1 == (uint32_t)float_to_q24(1.0f));
}

/**
 * @brief 硬件平方根 (Q24 输入，Q24 输出)
 * 
 * MATHACL SQRT 硬件固定在 IQ30 模式运行：
 *   - 输入必须归一化到 IQ30 [1.0, 2.0) 范围 (bit 30 必须为 1)
 *   - 输出固定为 IQ16 格式 (SFACTOR 仅影响内部 CORDIC，不影响输出 Q 格式)
 *   - SFACTOR 字段告知硬件原始缩放因子，用于内部补偿
 * 
 * 实现逻辑 (参照 TI __IQNsqrt_MathACL)：
 *   1. 特殊情况: bit 31 已置位 → scale_factor=0，直接传入
 *   2. 否则: CLZ 归一化到 IQ30，计算 scale_factor = 6 - n
 *   3. 硬件输出 IQ16 格式，左移 (q_value - 16) = 8 位转为 Q24
 * 
 * @param val_q24  Q24 格式的输入值
 * @return Q24 格式的平方根结果
 */
static inline int32_t mathacl_sqrt_q24(int32_t val_q24)
{
    if (val_q24 <= 0) return 0;
    g_mathacl_op_count++;
    
    uint32_t uval = (uint32_t)val_q24;
    uint32_t iq30_val;
    uint8_t scale_factor;
    
    /* 特殊情况: bit 31 已置位，值已在 IQ30 范围内 [2.0, 4.0) */
    if (uval & 0x80000000U) {
        scale_factor = 0;
        iq30_val = uval;
    } else {
        /* 归一化: 左移直到 bit 30 = 1 */
        int n = 0;
#if defined(__GNUC__) || defined(__clang__)
        n = __builtin_clz(uval) - 1;  /* GCC/Clang 内建函数 */
#else
        /* Keil ARMCC: 使用 CLZ 指令 */
        n = (int)__clz(uval) - 1;
#endif
        iq30_val = uval << n;
        /* SFACTOR = (30 - q_value) - n = (30 - 24) - n = 6 - n */
        scale_factor = (uint8_t)((6 - n) & 0x3F);
    }
    
    MATHACL_LOCK();
    /* 写入 CTL，包含 SFACTOR (bits [21:16])
     * SFACTOR 告诉硬件原始缩放因子，用于内部 CORDIC 补偿 */
    MATHACL->CTL = MATHACL_SQRT_CFG | ((uint32_t)scale_factor << 16);
    MATHACL->OP1 = iq30_val;
    mathacl_wait_done();
    
    /* 硬件输出固定 IQ16 格式 (与 SFACTOR 无关)
     * 参照 TI __IQNsqrt_MathACL: result = RES1 << (q_value - 16)
     * 对于 Q24: shift = 24 - 16 = 8 */
    int32_t result = (int32_t)((uint32_t)MATHACL->RES1 << (24 - 16));
    MATHACL_UNLOCK();
    return result;
}

/**
 * @brief 硬件平方根 (float 封装)
 * 
 * @param x  输入浮点数 (必须 >= 0)
 * @return 平方根结果
 */
static inline float hw_sqrtf(float x)
{
    if (x <= 0.0f) return 0.0f;
    
    /* Q24 范围: [-128, 128)，钳位防止 int32 溢出 */
    if (x >= 127.999f) x = 127.999f;
    
    /* 转换为 Q24 */
    int32_t val_q24 = (int32_t)(x * MATHACL_Q24_SCALE_F);
    
    /* 硬件 SQRT (输出 Q24) */
    int32_t result_q24 = mathacl_sqrt_q24(val_q24);
    
    /* 转换回 float (Q24 → float) */
    return (float)result_q24 / MATHACL_Q24_SCALE_F;
}

/**
 * @brief MATHACL 硬件自检: 测试 SQRT(4.0) 是否返回 ≈2.0
 * @return true=硬件正常, false=硬件异常(使用软浮点回退)
 */
static inline bool mathacl_selftest(void)
{
    int32_t val_q24 = float_to_q24(4.0f);
    int32_t res_q24 = mathacl_sqrt_q24(val_q24);
    float result = q24_to_float(res_q24);
    return (result > 1.5f && result < 2.5f);
}

/**
 * @brief 硬件正弦/余弦 (Q32 角度输入)
 * 
 * 根据 TI 官方实现，SINCOS 操作数格式为：
 *   OP1 = Q32 格式角度 ([0, 2π) → [0, 2^32))
 *   需要 LSB=1 workaround 避免 0xC0000000/0x80000000 输入错误
 * 
 * @param angle_q32  Q32 格式的角度
 * @param sin_out    指向 sin 结果的指针 (Q31)
 * @param cos_out    指向 cos 结果的指针 (Q31)
 */
static inline void mathacl_sincos_q32(uint32_t angle_q32, int32_t *sin_out, int32_t *cos_out)
{
    /* TI bug workaround: 设置 LSB=1 避免 0xC0000000/0x80000000 输入错误 */
    angle_q32 |= 0x1;
    g_mathacl_op_count++;
    g_mathacl_op_count++;
    
    MATHACL_LOCK();
    MATHACL->CTL = MATHACL_SINCOS_CFG;
    MATHACL->OP1 = angle_q32;
    mathacl_wait_done();
    *cos_out = (int32_t)MATHACL->RES1;
    *sin_out = (int32_t)MATHACL->RES2;
    MATHACL_UNLOCK();
}

/**
 * @brief 硬件正弦/余弦 (float 封装)
 * 
 * @param angle_rad  弧度值
 * @param sin_out    指向 sin 结果的指针
 * @param cos_out    指向 cos 结果的指针
 */
static inline void hw_sincosf(float angle_rad, float *sin_out, float *cos_out)
{
    /* 归一化到 [0, 2π) */
    float angle_norm = angle_rad;
    while (angle_norm < 0.0f) angle_norm += 6.283185307f;
    while (angle_norm >= 6.283185307f) angle_norm -= 6.283185307f;
    
    /* 转换为 Q32 格式（[0, 2π) → [0, 2^32)）*/
    uint32_t angle_q32 = (uint32_t)(angle_norm / 6.283185307f * MATHACL_Q32_SCALE_F);
    
    /* 硬件 SINCOS */
    int32_t sin_q31, cos_q31;
    mathacl_sincos_q32(angle_q32, &sin_q31, &cos_q31);
    
    /* 转换回 float (Q31 → [-1, 1]) */
    *sin_out = (float)sin_q31 / MATHACL_Q31_SCALE_F;
    *cos_out = (float)cos_q31 / MATHACL_Q31_SCALE_F;
}

/**
 * @brief 硬件正弦 (float 封装)
 */
static inline float hw_sinf(float angle_rad)
{
    float s, c;
    hw_sincosf(angle_rad, &s, &c);
    return s;
}

/**
 * @brief 硬件余弦 (float 封装)
 */
static inline float hw_cosf(float angle_rad)
{
    float s, c;
    hw_sincosf(angle_rad, &s, &c);
    return c;
}

/**
 * @brief 硬件四象限反正切 (Q31 输入)
 * 
 * 根据 TI 官方实现，ATAN2 操作数顺序为：
 *   OP1 = X (触发器，最后写入)
 *   OP2 = Y
 * 
 * @param y_q31  Q31 格式的 Y 值
 * @param x_q31  Q31 格式的 X 值
 * @return angle/π 的 IQ31 有符号值 (位模式等价于 [0, 2π) 无符号 Q32):
 *         0° → 0x00000000
 *         90° → 0x40000000
 *         180° → 0x7FFFFFFF (TI 解释为 +π)
 *         270° → 0xC0000000 (TI 解释为 -π/2, 等价于 +3π/2)
 */
static inline uint32_t mathacl_atan2_q31(int32_t y_q31, int32_t x_q31)
{
    g_mathacl_op_count++;
    MATHACL_LOCK();
    MATHACL->CTL = MATHACL_ATAN2_CFG;
    MATHACL->OP2 = (uint32_t)y_q31;  // Y 放在 OP2
    MATHACL->OP1 = (uint32_t)x_q31;  // X 放在 OP1 (触发器)
    mathacl_wait_done();
    uint32_t result = (uint32_t)MATHACL->RES1;
    MATHACL_UNLOCK();
    return result;
}

/**
 * @brief 硬件四象限反正切 (float 封装)
 * 
 * MATHACL ATAN2 返回 [0, 2π) 格式角度 (无符号 Q32):
 *   0° → 0x00000000
 *   90° → 0x40000000
 *   180° → 0x80000000
 *   270° → 0xC0000000
 * 
 * 标准 atan2f 返回 [-π, π]，需要转换。
 * 
 * @param y  Y 值 (任意浮点数)
 * @param x  X 值 (任意浮点数)
 * @return 弧度值 [-π, π]
 */
static inline float hw_atan2f(float y, float x)
{
    /* 处理特殊情况 */
    if (x == 0.0f && y == 0.0f) return 0.0f;
    
    /* 归一化输入到 [-1, 1] 范围 (Q31 要求)
     * atan2 只依赖 y/x 比值和符号，归一化不影响角度 */
    float abs_y = y < 0.0f ? -y : y;
    float abs_x = x < 0.0f ? -x : x;
    float norm = abs_y > abs_x ? abs_y : abs_x;
    if (norm > 1.0f) {
        y /= norm;
        x /= norm;
    }
    
    /* 转换为 Q31 (现在 y, x 都在 [-1, 1] 范围内)
     * 使用钳位转换避免 ±1.0 溢出 INT32 */
    int32_t y_q31 = float_to_q31(y);
    int32_t x_q31 = float_to_q31(x);
    
    /* 硬件 ATAN2 (返回 [0, 2π) 无符号角度) */
    uint32_t angle_u32 = (uint32_t)mathacl_atan2_q31(y_q31, x_q31);
    
    /* 转换为弧度 [0, 2π)
     * 硬件输出范围 [0, 2^32) 映射到 [0, 2π)
     * 所以 angle_rad = angle_u32 / 2^32 * 2π */
    float angle_rad = (float)angle_u32 / MATHACL_Q32_SCALE_F * 6.283185307f;
    
    /* 转换为 [-π, π] 范围 (标准 atan2f 返回值) */
    if (angle_rad > 3.141592654f) {
        angle_rad -= 6.283185307f;
    }
    
    return angle_rad;
}

/**
 * @brief 硬件反正弦 (float 封装)
 * 
 * 使用 atan2 实现: asin(x) = atan2(x, sqrt(1-x²))
 * 
 * @param x  输入值 [-1, 1]
 * @return 弧度值 [-π/2, π/2]
 */
static inline float hw_asinf(float x)
{
    if (x >= 1.0f) return 1.570796327f;
    if (x <= -1.0f) return -1.570796327f;
    
    float sqrt_val = hw_sqrtf(1.0f - x * x);
    return hw_atan2f(x, sqrt_val);
}

/**
 * @brief 硬件 MAC 初始化
 * 
 * 清零累加器，准备进行乘累加运算。
 */
static inline void mathacl_mac_init(void)
{
    MATHACL->CTL = MATHACL_MAC_CFG;
    g_mathacl_op_count++;
    MATHACL->RES1 = 0;
    MATHACL->RES2 = 0;
}

/**
 * @brief 硬件 MAC 单步累加
 *
 * MATHACL 触发机制：写入 OP1 启动乘累加，因此必须先写 OP2，再写 OP1。
 * 必须等待完成后再进行下一次累加。
 *
 * @param a_q24  Q24 格式的操作数 A
 * @param b_q24  Q24 格式的操作数 B
 */
static inline void mathacl_mac_acc(int32_t a_q24, int32_t b_q24)
{
    MATHACL->OP2 = (uint32_t)b_q24;
    MATHACL->OP1 = (uint32_t)a_q24;
    mathacl_wait_done();  /* 等待本次 MAC 完成，防止下一次写入覆盖 */
}

/**
 * @brief 硬件 MAC 读取结果
 * 
 * @return Q24 格式的累加结果
 */
static inline int32_t mathacl_mac_result(void)
{
    mathacl_wait_done();
    return (int32_t)MATHACL->RES1;
}

/**
 * @brief 硬件除法 (Q24)
 *
 * MATHACL 触发机制：写入 OP1 会立即启动运算，因此必须先写 OP2（除数），
 * 再写 OP1（被除数）。顺序错误会导致 OP2 使用旧值，结果严重错误。
 * 同时必须置位 SIGNED 标志（bit 5），否则按无符号解释。
 *
 * @param num_q24  Q24 格式的被除数
 * @param den_q24  Q24 格式的除数
 * @return Q24 格式的商
 */
static inline int32_t mathacl_div_q24(int32_t num_q24, int32_t den_q24)
{
    if (den_q24 == 0) return (num_q24 >= 0) ? INT32_MAX : INT32_MIN;
    g_mathacl_op_count++;

    MATHACL_LOCK();
    MATHACL->CTL = MATHACL_CTL_FUNC_DIV | MATHACL_CTL_QVAL_Q24 |
                   MATHACL_CTL_OPTYPE_SIGNED;
    MATHACL->OP2 = (uint32_t)den_q24;
    MATHACL->OP1 = (uint32_t)num_q24;
    int32_t result;
    if (mathacl_wait_and_check() != 0) {
        /* 硬件错误 (OVF/ERR): 返回饱和值 */
        result = (num_q24 >= 0) ? INT32_MAX : INT32_MIN;
    } else {
        result = (int32_t)MATHACL->RES1;
    }
    MATHACL_UNLOCK();
    return result;
}

/**
 * @brief 矩阵乘法辅助函数 (Q24 定点)
 * 
 * 计算 C[i][j] = sum_k(A[i][k] * B[k][j])，使用 MATHACL MAC 加速。
 * 
 * @param A_row  A 矩阵的第 i 行 (Q24 格式)
 * @param B_col  B 矩阵的第 j 列 (Q24 格式)
 * @param len    向量长度
 * @return Q24 格式的点积结果
 */
static inline int32_t mathacl_dot_q24(const int32_t *A_row, const int32_t *B_col, int len)
{
    MATHACL_LOCK();
    mathacl_mac_init();
    for (int k = 0; k < len; k++) {
        mathacl_mac_acc(A_row[k], B_col[k]);
    }
    int32_t result = mathacl_mac_result();
    MATHACL_UNLOCK();
    return result;
}

/**
 * @brief 批量硬件点积 (Q24 定点)
 *
 * 与 mathacl_dot_q24 等价，供 filter 使用。
 *
 * @param A  A 向量 (Q24 格式)
 * @param B  B 向量 (Q24 格式)
 * @param len 向量长度
 * @return Q24 格式的点积结果
 */
static inline int32_t mathacl_dot_q24_batch(const int32_t *A, const int32_t *B, int len)
{
    return mathacl_dot_q24(A, B, len);
}

/**
 * @brief 浮点矩阵乘法 (纯软浮点实现, 未使用 MATHACL 硬件)
 *
 * @param C      结果矩阵 (float)
 * @param A      输入矩阵 A (float)
 * @param B      输入矩阵 B (float)
 * @param rows   A 的行数
 * @param cols   B 的列数
 * @param inner  A 的列数 / B 的行数
 */
static inline void matrix_mul_f32(float *C, const float *A, const float *B,
                                    int rows, int cols, int inner)
{
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            float sum = 0.0f;
            for (int k = 0; k < inner; k++) {
                sum += A[i * inner + k] * B[k * cols + j];
            }
            C[i * cols + j] = sum;
        }
    }
}

#endif /* BSP_MATHACL_ENABLE */

/* ============================================================================
 * 软浮点回退函数 (始终编译，用于选择性加速)
 * ============================================================================ */

#include <math.h>

/** @brief 软件平方根 */
static inline float soft_sqrtf(float x) { return sqrtf(x); }
/** @brief 软件正弦 */
static inline float soft_sinf(float x) { return sinf(x); }
/** @brief 软件余弦 */
static inline float soft_cosf(float x) { return cosf(x); }
/** @brief 软件正弦/余弦 */
static inline void soft_sincosf(float angle, float *s, float *c) {
    *s = sinf(angle);
    *c = cosf(angle);
}
/** @brief 软件四象限反正切 */
static inline float soft_atan2f(float y, float x) { return atan2f(y, x); }
/** @brief 软件反正弦 */
static inline float soft_asinf(float x) { return asinf(x); }

/* ============================================================================
 * 选择性硬件加速层
 * 
 * 根据性能测试结果选择最优实现:
 *   SQRT:  软件更快 (0.86x) → 默认用软件
 *   ATAN2: 硬件更快 (1.26x) → 默认用硬件
 *   SINCOS: 硬件更快 (2.51x) → 默认用硬件
 *   ASIN:  软件更快 (0.84x) → 默认用软件
 * ============================================================================ */

#ifdef BSP_MATHACL_ENABLE

/* SQRT: 根据宏选择 */
#ifdef BSP_MATHACL_SQRT_HW
  #define mathacl_sqrtf(x)    hw_sqrtf(x)
#else
  #define mathacl_sqrtf(x)    soft_sqrtf(x)
#endif

/* SINCOS: 根据宏选择 */
#ifdef BSP_MATHACL_SINCOS_HW
  #define mathacl_sincosf(a,s,c)  hw_sincosf(a,s,c)
  #define mathacl_sinf(x)         hw_sinf(x)
  #define mathacl_cosf(x)         hw_cosf(x)
#else
  #define mathacl_sincosf(a,s,c)  soft_sincosf(a,s,c)
  #define mathacl_sinf(x)         soft_sinf(x)
  #define mathacl_cosf(x)         soft_cosf(x)
#endif

/* ATAN2: 根据宏选择 */
#ifdef BSP_MATHACL_ATAN2_HW
  #define mathacl_atan2f(y,x) hw_atan2f(y,x)
#else
  #define mathacl_atan2f(y,x) soft_atan2f(y,x)
#endif

/* ASIN: 使用已选择的 sqrt 和 atan2 */
#define mathacl_asinf(x)    soft_asinf(x)

#else /* !BSP_MATHACL_ENABLE */

/* 全部使用软件 */
#define mathacl_sqrtf(x)        soft_sqrtf(x)
#define mathacl_sincosf(a,s,c)  soft_sincosf(a,s,c)
#define mathacl_sinf(x)         soft_sinf(x)
#define mathacl_cosf(x)         soft_cosf(x)
#define mathacl_atan2f(y,x)     soft_atan2f(y,x)
#define mathacl_asinf(x)        soft_asinf(x)

static inline void mathacl_init(void) { /* 无需初始化 */ }
static inline void mathacl_mac_init(void) { /* 无需初始化 */ }
static inline void mathacl_mac_acc(int32_t a, int32_t b) { (void)a; (void)b; }
static inline int32_t mathacl_mac_result(void) { return 0; }
static inline int32_t mathacl_div_q24(int32_t n, int32_t d) { (void)n; (void)d; return 0; }

#endif /* BSP_MATHACL_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* BSP_MATHACL_H */
