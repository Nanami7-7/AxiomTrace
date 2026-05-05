/**
 * @file    _hal_template.c
 * @brief   HAL驱动模板 - 复制此文件创建新的HAL外设驱动
 *
 * @note    使用方法：
 *          1. 复制本文件，重命名为hal_xxx.c
 *          2. 替换所有TEMPLATE/xxx为实际外设名
 *          3. 在hal_common.h中添加hal_xxx_id_t枚举
 *          4. 在ti_msp_dl_config.h中添加对应的外设宏定义
 *          5. 实现各函数的具体SDK调用
 *
 *          本模板遵循HAL薄封装原则：
 *          - 函数1:1映射DL_xxx调用
 *          - 仅增加参数校验和错误码返回
 *          - 不添加业务逻辑
 *          - 不依赖OSAL或BSP头文件
 */

/* ======================== 替换指南 ======================== */
/* 将下面的TEMPLATE替换为实际外设名，如SPI、I2C等             */
/* #include "hal_template.h" -> #include "hal_spi.h"         */
/* hal_template_xxx          -> hal_spi_xxx                  */
/* HAL_TEMPLATE_xxx          -> HAL_SPI_xxx                  */
/* s_template_inst_map       -> s_spi_inst_map               */
/* ========================================================= */

#include "hal_template.h"      /* 替换为: hal_xxx.h */
#include "ti_msp_dl_config.h"  /* HAL层唯一允许的SDK头文件 */

/* ======================== 私有映射表 ======================== */

/**
 * @brief TEMPLATE实例寄存器指针映射表
 * @note  顺序必须与hal_template_id_t枚举一致
 *        每个元素对应一个外设实例的寄存器基址
 */
static TEMPLATE_Regs *const s_template_inst_map[HAL_TEMPLATE_COUNT] = {
    /* 替换为实际外设实例，如: */
    /* SPI0, */
    /* SPI1, */
};

/**
 * @brief TEMPLATE中断号映射表
 * @note  用于NVIC中断使能/禁止操作
 */
static const IRQn_Type s_template_irq_map[HAL_TEMPLATE_COUNT] = {
    /* 替换为实际中断号，如: */
    /* SPI0_INT_IRQn, */
};

/* ======================== 内联辅助函数 ======================== */

/**
 * @brief  检查TEMPLATE实例枚举是否合法
 * @param  id TEMPLATE实例枚举值
 * @retval true  合法
 * @retval false 越界
 */
static inline bool is_template_valid(hal_template_id_t id)
{
    return ((uint32_t)id < HAL_TEMPLATE_COUNT);
}

/**
 * @brief  将HAL TEMPLATE枚举转换为SDK寄存器指针
 * @param  id HAL TEMPLATE枚举值
 * @retval 寄存器指针，越界返回NULL
 */
static inline TEMPLATE_Regs *template_to_regs(hal_template_id_t id)
{
    if (!is_template_valid(id)) {
        return NULL;
    }
    return s_template_inst_map[id];
}

/* ======================== 公共函数实现 ======================== */

/**
 * @brief  初始化TEMPLATE外设
 * @note   如果SYSCFG_DL_init()已完成初始化，此函数可留空或仅做
 *         运行时补充配置(如使能中断)。如果SysConfig未配置，则需
 *         完整实现时钟使能、引脚复用、外设参数配置等
 * @param  id TEMPLATE实例编号
 * @retval HAL_OK           初始化成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_template_init(hal_template_id_t id)
{
    if (!is_template_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /* TODO: 实现外设初始化 */
    /* 如果SysConfig已配置，通常只需使能NVIC中断： */
    /* NVIC_ClearPendingIRQ(s_template_irq_map[id]); */
    /* NVIC_EnableIRQ(s_template_irq_map[id]);        */

    return HAL_OK;
}

/**
 * @brief  反初始化TEMPLATE外设(关闭时钟/中断等)
 * @param  id TEMPLATE实例编号
 * @retval HAL_OK           反初始化成功
 * @retval HAL_ERR_INVALID_PARAM 实例越界
 */
hal_status_t hal_template_deinit(hal_template_id_t id)
{
    if (!is_template_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /* TODO: 实现外设反初始化 */
    NVIC_DisableIRQ(s_template_irq_map[id]);

    return HAL_OK;
}

/**
 * @brief  读取数据
 * @param  id   TEMPLATE实例编号
 * @param  buf  读取缓冲区指针
 * @param  len  读取长度
 * @retval HAL_OK           读取成功
 * @retval HAL_ERR_INVALID_PARAM 参数无效
 */
hal_status_t hal_template_read(hal_template_id_t id, uint8_t *buf,
                                uint16_t len)
{
    /* 参数校验 */
    if (buf == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    if (!is_template_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /* TODO: 实现数据读取 */
    /* TEMPLATE_Regs *regs = template_to_regs(id); */

    return HAL_OK;
}

/**
 * @brief  写入数据
 * @param  id   TEMPLATE实例编号
 * @param  buf  写入数据缓冲区指针
 * @param  len  写入长度
 * @retval HAL_OK           写入成功
 * @retval HAL_ERR_INVALID_PARAM 参数无效
 */
hal_status_t hal_template_write(hal_template_id_t id, const uint8_t *buf,
                                 uint16_t len)
{
    /* 参数校验 */
    if (buf == NULL) {
        return HAL_ERR_INVALID_PARAM;
    }
    if (!is_template_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /* TODO: 实现数据写入 */

    return HAL_OK;
}

/**
 * @brief  扩展控制(设置参数/注册回调等)
 * @param  id  TEMPLATE实例编号
 * @param  cmd 控制命令
 * @param  arg 命令参数
 * @retval HAL_OK           控制成功
 * @retval HAL_ERR_INVALID_PARAM 参数无效
 * @retval HAL_ERR_UNSUPPORTED 不支持的命令
 */
hal_status_t hal_template_control(hal_template_id_t id, uint32_t cmd,
                                   void *arg)
{
    if (!is_template_valid(id)) {
        return HAL_ERR_INVALID_PARAM;
    }

    /* TODO: 根据cmd实现扩展控制 */
    (void)cmd;
    (void)arg;

    return HAL_ERR_UNSUPPORTED;
}
