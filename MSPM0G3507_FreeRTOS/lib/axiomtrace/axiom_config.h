/**
 * @file    axiom_config.h
 * @brief   AxiomTrace 配置 (项目专用)
 * @note    仅使用 AX_LOG 文本日志功能, 不引入二进制协议
 */
#ifndef AXIOM_CONFIG_H
#define AXIOM_CONFIG_H

/* 编译配置 */
#define AXIOM_PROFILE       AXIOM_PROFILE_DEV   /* 开发模式: 启用日志输出 */
#define AXIOM_LOG_MAX_LINE  128                 /* 单行日志最大长度 */

/* Level 定义 (仅用于条件编译开关, 不参与二进制协议) */
#define AXIOM_LEVEL_DEBUG   0
#define AXIOM_LEVEL_INFO    1
#define AXIOM_LEVEL_WARN    2
#define AXIOM_LEVEL_ERROR   3
#define AXIOM_LEVEL_FAULT   4

#endif /* AXIOM_CONFIG_H */