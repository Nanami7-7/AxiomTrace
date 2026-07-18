/**
 * @file    project_version.h
 * @brief   Firmware and host-protocol version contract.
 * @note    Keep this header dependency-free so it can be consumed by every
 *          firmware layer and by release tooling without pulling BSP headers.
 */
#ifndef PROJECT_VERSION_H
#define PROJECT_VERSION_H

#define PROJECT_VERSION_MAJOR        (0U)
#define PROJECT_VERSION_MINOR        (1U)
#define PROJECT_VERSION_PATCH        (0U)
#define PROJECT_VERSION_STRING       "0.1.0"

/** Text command/telemetry compatibility level used by the desktop GUI. */
#define PROJECT_PROTOCOL_VERSION     (1U)
#define PROJECT_BOARD_NAME           "MSPM0G3507"
#define PROJECT_MOTOR_DRIVER_NAME    "DRV8870"

#endif /* PROJECT_VERSION_H */
