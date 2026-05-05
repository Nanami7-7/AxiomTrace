/**
 * @file    _bsp_template.c
 * @brief   BSP驱动开发模板
 * @note    复制此文件并重命名后修改，创建新的BSP设备驱动。
 *          替换所有"xxx"为实际设备名称(如"oled"/"imu"/"eeprom")。
 *          每个BSP驱动提供5个标准接口: init/deinit/read/write/control
 *
 * 使用步骤:
 *   1. 复制本文件，重命名为 bsp_xxx.c
 *   2. 创建 bsp_xxx.h，声明5个标准接口
 *   3. 在 project_config.h 中添加设备引脚/参数定义
 *   4. 实现各函数逻辑
 *   5. 在 app_main.c 中调用 bsp_xxx_init() 初始化
 */
#include "bsp_xxx.h"
#include "hal_gpio.h"       /* GPIO操作 */
#include "hal_uart.h"       /* UART操作(按需) */
#include "hal_timer.h"      /* Timer操作(按需) */
#include "hal_adc.h"        /* ADC操作(按需) */
#include "osal_api.h"       /* OS抽象接口 */
#include "bsp_common.h"     /* BSP公共定义 */
#include "project_config.h" /* 项目配置 */

/* ======================== 私有宏 ======================== */

/** 设备初始化标志魔法数(用于检测是否已初始化) */
#define XXX_INIT_MAGIC    (0xA5A5U)

/* ======================== 私有类型 ======================== */

/**
 * @brief 设备运行时状态结构体
 * @note  所有运行时变量集中在此，避免全局变量散落
 */
typedef struct {
    bool          inited;    /**< 初始化标志 */
    osal_mutex_t  mutex;     /**< 互斥锁(多任务访问保护) */
    /* 在此添加设备特有状态变量 */
} xxx_state_t;

/* ======================== 私有变量 ======================== */

/** 设备运行时状态(文件静态) */
static xxx_state_t s_xxx;

/* ======================== 私有函数声明 ======================== */

/* 在此声明仅文件内部使用的函数 */

/* ======================== 公共函数实现 ======================== */

/**
 * @brief  初始化xxx设备
 * @note   调用HAL层接口配置硬件，注册中断回调
 *         仅首次调用时执行初始化，重复调用直接返回成功
 * @retval BSP_OK           初始化成功
 * @retval BSP_ERR_NULL_PTR 内部状态异常
 * @retval BSP_ERR_HW_FAULT 硬件初始化失败
 */
bsp_status_t bsp_xxx_init(void)
{
    /* 防止重复初始化 */
    if (s_xxx.inited) {
        return BSP_OK;
    }

    /* 创建互斥锁 */
    s_xxx.mutex = osal_mutex_create();
    if (NULL == s_xxx.mutex) {
        return BSP_ERR_HW_FAULT;
    }

    /*
     * 在此调用HAL层接口完成硬件配置:
     * - hal_gpio_init_output() / hal_gpio_init_input()
     * - hal_uart_enable_irq()
     * - hal_timer_enable_irq()
     * - hal_adc_enable_irq()
     *
     * 示例:
     *   hal_status_t ret;
     *   ret = hal_gpio_init_output(&(hal_gpio_pin_config_t){
     *       .port  = PRJ_XXX_PORT,
     *       .pin   = PRJ_XXX_PIN,
     *       .iomux = PRJ_XXX_IOMUX,
     *   });
     *   if (HAL_OK != ret) {
     *       return BSP_ERR_HW_FAULT;
     *   }
     */

    s_xxx.inited = true;

    return BSP_OK;
}

/**
 * @brief  反初始化xxx设备
 * @note   释放硬件资源，关闭中断，删除互斥锁
 * @retval BSP_OK 反初始化成功
 */
bsp_status_t bsp_xxx_deinit(void)
{
    if (!s_xxx.inited) {
        return BSP_OK;
    }

    /*
     * 在此调用HAL层接口关闭硬件:
     * - hal_timer_stop()
     * - hal_uart_disable_irq()
     * - hal_gpio_write_pin() 设为安全状态
     */

    /* 删除互斥锁 */
    if (NULL != s_xxx.mutex) {
        osal_mutex_delete(s_xxx.mutex);
        s_xxx.mutex = NULL;
    }

    s_xxx.inited = false;

    return BSP_OK;
}

/**
 * @brief  从xxx设备读取数据
 * @param  buf  读取缓冲区
 * @param  len  期望读取长度
 * @param  out_len 实际读取长度(可为NULL)
 * @retval BSP_OK           读取成功
 * @retval BSP_ERR_NOT_INIT 设备未初始化
 * @retval BSP_ERR_NULL_PTR 空指针参数
 */
bsp_status_t bsp_xxx_read(void *buf, uint32_t len, uint32_t *out_len)
{
    if (!s_xxx.inited) {
        return BSP_ERR_NOT_INIT;
    }
    if ((NULL == buf) || (0U == len)) {
        return BSP_ERR_NULL_PTR;
    }

    osal_mutex_lock(s_xxx.mutex);

    /*
     * 在此实现读取逻辑:
     * - 从环形缓冲区取数据
     * - 调用hal_xxx_read()
     * - 协议解析
     */

    osal_mutex_unlock(s_xxx.mutex);

    (void)out_len;
    return BSP_OK;
}

/**
 * @brief  向xxx设备写入数据
 * @param  data 写入数据指针
 * @param  len  写入数据长度
 * @retval BSP_OK           写入成功
 * @retval BSP_ERR_NOT_INIT 设备未初始化
 * @retval BSP_ERR_NULL_PTR 空指针参数
 */
bsp_status_t bsp_xxx_write(const void *data, uint32_t len)
{
    if (!s_xxx.inited) {
        return BSP_ERR_NOT_INIT;
    }
    if ((NULL == data) || (0U == len)) {
        return BSP_ERR_NULL_PTR;
    }

    osal_mutex_lock(s_xxx.mutex);

    /*
     * 在此实现写入逻辑:
     * - 调用hal_xxx_write()
     * - 数据格式化/打包
     */

    osal_mutex_unlock(s_xxx.mutex);

    return BSP_OK;
}

/**
 * @brief  xxx设备控制命令
 * @param  cmd   控制命令枚举
 * @param  param 命令参数(可为NULL)
 * @retval BSP_OK           执行成功
 * @retval BSP_ERR_NOT_INIT 设备未初始化
 * @retval BSP_ERR_INVALID_PARAM 不支持的命令
 */
bsp_status_t bsp_xxx_control(uint32_t cmd, void *param)
{
    if (!s_xxx.inited) {
        return BSP_ERR_NOT_INIT;
    }

    osal_mutex_lock(s_xxx.mutex);

    /*
     * 在此实现控制逻辑:
     * switch (cmd) {
     * case BSP_XXX_CMD_RESET:
     *     ...
     *     break;
     * case BSP_XXX_CMD_SET_MODE:
     *     ...
     *     break;
     * default:
     *     osal_mutex_unlock(s_xxx.mutex);
     *     return BSP_ERR_INVALID_PARAM;
     * }
     */

    (void)cmd;
    (void)param;

    osal_mutex_unlock(s_xxx.mutex);

    return BSP_OK;
}

/* ======================== 中断回调 ======================== */

/**
 * @brief  xxx设备中断服务函数
 * @note   ISR中仅做最简操作(读寄存器/填充缓冲区/释放信号量)
 *         不做协议解析/长时间处理
 */
/*
void XXX_IRQHandler(void)
{
    // 1. 读取中断标志
    // 2. 读取硬件数据(如UART接收字节)
    // 3. 填充环形缓冲区
    // 4. 释放信号量通知任务(osal_sem_release_from_isr)
}
*/
