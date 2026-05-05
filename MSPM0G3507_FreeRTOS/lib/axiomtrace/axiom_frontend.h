/**
 * @file    axiom_frontend.h
 * @brief   AxiomTrace 轻量级前端 (仅 AX_LOG 宏)
 * @note    从 AxiomTrace baremetal/frontend/axiom_frontend.h 精简
 *          仅保留 AX_LOG 系列文本日志宏, 去掉 AX_EVT/AX_KV/AX_PROBE
 *          AX_LOG 输出纯文本, 通过 axiom_port_string_out() 投递
 *          AXIOM_PROFILE=DEV 时编译输出, PROD 时全部静默
 */
#ifndef AXIOM_FRONTEND_H
#define AXIOM_FRONTEND_H

#include "axiom_config.h"
#include "axiom_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Profile 定义
 * --------------------------------------------------------------------------- */
#define AXIOM_PROFILE_DEV    0
#define AXIOM_PROFILE_FIELD  1
#define AXIOM_PROFILE_PROD   2

/* ---------------------------------------------------------------------------
 * AX_LOG — 文本日志 (PROD profile = no-op)
 * 输出纯文本到 axiom_port_string_out(), 不进入二进制协议
 * --------------------------------------------------------------------------- */
#if (AXIOM_PROFILE == AXIOM_PROFILE_DEV) || (AXIOM_PROFILE == AXIOM_PROFILE_FIELD)

#define AX_LOG(msg) \
    do { axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)

#define AX_LOG_DEBUG(msg) \
    do { axiom_port_string_out("[DEBUG] "); axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)

#define AX_LOG_INFO(msg) \
    do { axiom_port_string_out("[INFO] "); axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)

#define AX_LOG_WARN(msg) \
    do { axiom_port_string_out("[WARN] "); axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)

#define AX_LOG_ERROR(msg) \
    do { axiom_port_string_out("[ERROR] "); axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)

#else /* PROD */

#define AX_LOG(msg)         ((void)0)
#define AX_LOG_DEBUG(msg)   ((void)0)
#define AX_LOG_INFO(msg)    ((void)0)
#define AX_LOG_WARN(msg)    ((void)0)
#define AX_LOG_ERROR(msg)   ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_FRONTEND_H */