/**
 * @file    project_config.h
 * @brief   MSPM0G3507 M0 Base 工程的通用项目配置。
 *
 * 本工程只保留通用外设和应用框架，不包含电机、编码器、IMU 或陀螺仪配置。
 * 产品相关的引脚和参数应放在派生工程中，避免修改通用驱动。
 */
#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include "hal_common.h"

/* ======================== MCU 信息 ======================== */
#define PRJ_MCU_NAME "MSPM0G3507"

/* ======================== BLE 串口配置 ========================
 * BLE 模块最终连接 UART2；UART1 专用于 Board B 与 Board A 通信。
 * UART2 的波特率和引脚以 SysConfig 的实际配置为准。
 */
#ifndef PRJ_BLE_MODULE
#define PRJ_BLE_MODULE               (1U)
#endif

#if (PRJ_BLE_MODULE == 1U)
#define PRJ_BLE_MODULE_DX_BT311      (1U)
#define PRJ_BLE_MODULE_JDY23         (0U)
#else
#define PRJ_BLE_MODULE_DX_BT311      (0U)
#define PRJ_BLE_MODULE_JDY23         (1U)
#endif

#define PRJ_UART_BLE_ID              HAL_UART_2
#define UART_BLE_ID                  HAL_UART_2
#define DX_BT311_UART_BAUD           (9600U)
#define JDY23_UART_BAUD              (9600U)
#define PRJ_JDY23_UART_BAUD          JDY23_UART_BAUD

/* DX-BT311 角色：0=从机，1=主机。 */
#define PRJ_BLE_ROLE_SLAVE           (0U)
#define PRJ_BLE_ROLE_MASTER          (1U)
#ifndef PRJ_BLE_ROLE
#define PRJ_BLE_ROLE                 PRJ_BLE_ROLE_SLAVE
#endif
#if ((PRJ_BLE_ROLE != PRJ_BLE_ROLE_SLAVE) && \
     (PRJ_BLE_ROLE != PRJ_BLE_ROLE_MASTER))
#error "PRJ_BLE_ROLE 必须为 PRJ_BLE_ROLE_SLAVE 或 PRJ_BLE_ROLE_MASTER"
#endif

/* BLE 服务任务的启动策略。 */
#ifndef PRJ_BLE_AUTO_INIT
#define PRJ_BLE_AUTO_INIT            (1U)
#endif
#ifndef PRJ_BLE_AUTO_PROBE
#define PRJ_BLE_AUTO_PROBE           (1U)
#endif
/* 启动时仅设置一次 BLE 名称，随后 UART2 进入透明协议模式。 */
#ifndef PRJ_BLE_AUTO_SET_NAME
#define PRJ_BLE_AUTO_SET_NAME        (1U)
#endif
#define PRJ_BLE_NAME_CONFIG_TIMEOUT_MS (800U)

#ifndef PRJ_BLE_AUTO_CONNECT
#define PRJ_BLE_AUTO_CONNECT         (0U)
#endif

/* 是否自动写入 BLE 模块的 Flash 配置，默认关闭。 */
#ifndef PRJ_BLE_APPLY_AT_CONFIG
#define PRJ_BLE_APPLY_AT_CONFIG      (0U)
#endif

#define PRJ_BLE_INIT_TIMEOUT_MS      (1000U)
#define PRJ_BLE_INQUIRE_TIMEOUT_MS   (8000U)
#define PRJ_BLE_CONNECT_TIMEOUT_MS   (10000U)
#define PRJ_BLE_TASK_STACK_WORDS     (384U)
#define PRJ_BLE_TASK_PRIORITY        (2U)
#define PRJ_BLE_MASTER_MAX_DEVICES   (8U)

/* ======================== BLE 主机目标参数 ======================== */
#ifndef PRJ_BLE_MASTER_TARGET_MAC
#define PRJ_BLE_MASTER_TARGET_MAC    ""
#endif
#ifndef PRJ_BLE_MASTER_TARGET_NAME
#define PRJ_BLE_MASTER_TARGET_NAME   ""
#endif
#ifndef PRJ_BLE_MASTER_MUUID
#define PRJ_BLE_MASTER_MUUID         ""
#endif

/* ======================== BLE 从机参数 ======================== */
#ifndef PRJ_BLE_SLAVE_NAME
#define PRJ_BLE_SLAVE_NAME           "ONB"
#endif
#ifndef PRJ_BLE_SLAVE_UUID
#define PRJ_BLE_SLAVE_UUID           ""
#endif
#ifndef PRJ_BLE_SLAVE_CHAR
#define PRJ_BLE_SLAVE_CHAR           ""
#endif
#ifndef PRJ_BLE_SLAVE_WRITE
#define PRJ_BLE_SLAVE_WRITE          ""
#endif
#ifndef PRJ_BLE_SLAVE_NOTI
#define PRJ_BLE_SLAVE_NOTI           ""
#endif
#ifndef PRJ_BLE_SLAVE_ADVI
#define PRJ_BLE_SLAVE_ADVI           ""
#endif
#ifndef PRJ_BLE_SLAVE_CLOSEADV
#define PRJ_BLE_SLAVE_CLOSEADV       ""
#endif

#endif /* PROJECT_CONFIG_H */
