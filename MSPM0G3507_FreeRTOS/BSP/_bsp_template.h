/**
 * @file    _bsp_template.h
 * @brief   BSP驱动头文件模板
 * @note    复制此文件并重命名为 bsp_xxx.h，替换所有"xxx"
 *          声明5个标准接口: init/deinit/read/write/control
 *          定义控制命令枚举和设备配置结构体
 */
#ifndef BSP_XXX_H
#define BSP_XXX_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 包含 ======================== */
#include "bsp_common.h"

/* ======================== 控制命令枚举 ======================== */

/**
 * @brief xxx设备控制命令枚举
 * @note  通过bsp_xxx_control()使用，扩展时在此添加
 */
typedef enum {
    BSP_XXX_CMD_RESET = 0,    /**< 复位设备 */
    BSP_XXX_CMD_SET_MODE,     /**< 设置工作模式 */
    /* 在此添加设备特有命令 */
} bsp_xxx_cmd_t;

/* ======================== 函数接口 ======================== */

/**
 * @brief  初始化xxx设备
 * @note   仅首次调用执行初始化，重复调用返回成功
 * @retval BSP_OK           初始化成功
 * @retval BSP_ERR_HW_FAULT 硬件初始化失败
 */
bsp_status_t bsp_xxx_init(void);

/**
 * @brief  反初始化xxx设备
 * @retval BSP_OK 反初始化成功
 */
bsp_status_t bsp_xxx_deinit(void);

/**
 * @brief  从xxx设备读取数据
 * @param  buf      读取缓冲区
 * @param  len      期望读取长度
 * @param  out_len  实际读取长度(可为NULL)
 * @retval BSP_OK           读取成功
 * @retval BSP_ERR_NOT_INIT 设备未初始化
 * @retval BSP_ERR_NULL_PTR 空指针
 */
bsp_status_t bsp_xxx_read(void *buf, uint32_t len, uint32_t *out_len);

/**
 * @brief  向xxx设备写入数据
 * @param  data 写入数据指针
 * @param  len  写入数据长度
 * @retval BSP_OK           写入成功
 * @retval BSP_ERR_NOT_INIT 设备未初始化
 * @retval BSP_ERR_NULL_PTR 空指针
 */
bsp_status_t bsp_xxx_write(const void *data, uint32_t len);

/**
 * @brief  xxx设备控制命令
 * @param  cmd   控制命令(bsp_xxx_cmd_t)
 * @param  param 命令参数(可为NULL)
 * @retval BSP_OK             执行成功
 * @retval BSP_ERR_INVALID_PARAM 不支持的命令
 */
bsp_status_t bsp_xxx_control(uint32_t cmd, void *param);

#ifdef __cplusplus
}
#endif

#endif /* BSP_XXX_H */
