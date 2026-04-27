/**
 * @file    bsp_board.h
 * @brief   Board-level initialization interface
 */

#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/**
 * @brief  Initialize all board-level hardware
 *         Calls individual BSP module init functions in order:
 *         GPIO → UART → SPI → I2C → ADC
 */
hal_status_t bsp_board_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BOARD_H */
