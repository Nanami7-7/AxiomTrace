/**
 * @file    hal_common.h
 * @brief   HAL硬件抽象层公共定义
 * @note    提供HAL层统一的类型定义、错误码和宏，上层模块通过
 *          此头文件获取HAL层的基本类型，无需了解底层SDK细节
 */
#ifndef HAL_COMMON_H
#define HAL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ======================== HAL错误码 ======================== */
/**
 * @brief HAL操作返回状态码
 * @note  所有HAL函数返回此枚举值，上层通过判断HAL_OK确认成功
 */
typedef enum {
    HAL_OK               = 0,   /**< 操作成功 */
    HAL_ERR_INVALID_PARAM = -1, /**< 无效参数(空指针/越界/非法值) */
    HAL_ERR_BUSY         = -2,  /**< 外设忙(如UART正在发送) */
    HAL_ERR_TIMEOUT      = -3,  /**< 操作超时 */
    HAL_ERR_HW_FAULT     = -4,  /**< 硬件故障 */
    HAL_ERR_NOT_INIT     = -5,  /**< 外设未初始化 */
    HAL_ERR_UNSUPPORTED  = -6,  /**< 不支持的操作 */
} hal_status_t;

/* ======================== GPIO抽象类型 ======================== */
/**
 * @brief GPIO端口抽象枚举
 * @note  隐藏GPIO_Regs*指针，上层通过枚举值指定端口
 */
typedef enum {
    HAL_GPIO_PORT_A = 0,   /**< GPIOA端口 */
    HAL_GPIO_PORT_B,       /**< GPIOB端口 */
    HAL_GPIO_PORT_COUNT    /**< 端口总数(用于数组大小定义) */
} hal_gpio_port_t;

/**
 * @brief GPIO引脚方向枚举
 */
typedef enum {
    HAL_GPIO_DIR_INPUT  = 0, /**< 输入方向 */
    HAL_GPIO_DIR_OUTPUT = 1, /**< 输出方向 */
} hal_gpio_dir_t;

/**
 * @brief GPIO上拉/下拉配置枚举
 */
typedef enum {
    HAL_GPIO_PULL_NONE = 0,  /**< 无上下拉 */
    HAL_GPIO_PULL_UP,        /**< 上拉 */
    HAL_GPIO_PULL_DOWN,      /**< 下拉 */
} hal_gpio_pull_t;

/**
 * @brief GPIO引脚配置结构体
 * @note  用于软件I2C等SysConfig未配置的引脚初始化
 */
typedef struct {
    hal_gpio_port_t port;   /**< GPIO端口 */
    uint32_t        pin;    /**< 引脚编号(DL_GPIO_PIN_x) */
    uint32_t        iomux;  /**< 引脚复用配置(IOMUX_PINCMx) */
} hal_gpio_pin_config_t;

/* ======================== Timer抽象类型 ======================== */
/**
 * @brief 定时器实例抽象枚举
 * @note  隐藏GPTIMER_Regs*指针，上层通过枚举值指定定时器
 */
typedef enum {
    HAL_TIMER_PWM_MOTOR = 0, /**< TIMA0 - 电机PWM */
    HAL_TIMER_CAPTURE_LF,    /**< TIMG8 - 左前编码器捕获 */
    HAL_TIMER_CAPTURE_LB,    /**< TIMG7 - 左后编码器捕获 */
    HAL_TIMER_CAPTURE_RF,    /**< TIMG6 - 右前编码器捕获 */
    HAL_TIMER_CAPTURE_RB,    /**< TIMG0 - 右后编码器捕获 */
    HAL_TIMER_SYS_TICK,      /**< TIMA1 - 系统节拍定时器 */
    HAL_TIMER_COUNT          /**< 实例总数(用于数组大小定义) */
} hal_timer_id_t;

/**
 * @brief 定时器中断标志枚举
 * @note  用于hal_timer_get_pending_irq()返回值判断
 */
typedef enum {
    HAL_TIMER_IRQ_NONE    = 0,   /**< 无中断 */
    HAL_TIMER_IRQ_CC0     = 1,   /**< 捕获/比较通道0中断 */
    HAL_TIMER_IRQ_CC1     = 2,   /**< 捕获/比较通道1中断 */
    HAL_TIMER_IRQ_LOAD    = 4,   /**< 加载(溢出)中断 */
    HAL_TIMER_IRQ_CC0_CC1 = 3,   /**< CC0和CC1同时中断 */
} hal_timer_irq_flag_t;

/* ======================== UART抽象类型 ======================== */
/**
 * @brief UART实例抽象枚举
 * @note  隐藏UART_Regs*指针，上层通过枚举值指定串口
 */
typedef enum {
    HAL_UART_DEBUG = 0,   /**< UART0 - 调试串口 */
    HAL_UART_COUNT        /**< 实例总数(用于数组大小定义) */
} hal_uart_id_t;

/**
 * @brief UART中断标志枚举
 */
typedef enum {
    HAL_UART_IRQ_NONE = 0, /**< 无中断 */
    HAL_UART_IRQ_RX   = 1, /**< 接收中断 */
    HAL_UART_IRQ_TX   = 2, /**< 发送中断 */
} hal_uart_irq_flag_t;

/* ======================== ADC抽象类型 ======================== */
/**
 * @brief ADC实例抽象枚举
 * @note  隐藏ADC_Regs*指针，上层通过枚举值指定ADC
 */
typedef enum {
    HAL_ADC_VOLTAGE = 0,  /**< ADC0 - 电压采样 */
    HAL_ADC_COUNT         /**< 实例总数(用于数组大小定义) */
} hal_adc_id_t;

/* ======================== 通用宏 ======================== */
/**
 * @brief 数组元素个数计算宏
 */
#define HAL_ARRAY_SIZE(arr)   (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief 最小值/最大值宏
 */
#define HAL_MIN(a, b)         ((a) < (b) ? (a) : (b))
#define HAL_MAX(a, b)         ((a) > (b) ? (a) : (b))

/**
 * @brief HAL断言宏
 * @note  仅在DEBUG模式下启用，用于捕获编程错误
 */
#ifdef DEBUG
#define HAL_ASSERT(cond)       \
    do {                       \
        if (!(cond)) {         \
            for (;;) {         \
            }                  \
        }                      \
    } while (0)
#else
#define HAL_ASSERT(cond) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* HAL_COMMON_H */
