/**
 * @file    bsp_oled.h
 * @brief   OLED display interface (SSD1306, 128x64, I2C)
 */

#ifndef BSP_OLED_H
#define BSP_OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_common.h"

/* ---------- Font size ---------- */
typedef enum {
    BSP_OLED_FONT_SMALL  = 0,   /* 6x8  pixels */
    BSP_OLED_FONT_MEDIUM = 1,   /* 8x16 pixels */
} bsp_oled_font_t;

/* ========== API ========== */

/**
 * @brief  Initialize OLED display (SSD1306, I2C, 128x64)
 */
hal_status_t bsp_oled_init(void);

/**
 * @brief  Deinitialize OLED display (turn off)
 */
hal_status_t bsp_oled_deinit(void);

/**
 * @brief  Clear frame buffer (all pixels off)
 */
hal_status_t bsp_oled_clear(void);

/**
 * @brief  Set a single pixel in frame buffer
 * @param  x     X coordinate (0~127)
 * @param  y     Y coordinate (0~63)
 * @param  color 1 = white (on), 0 = black (off)
 */
hal_status_t bsp_oled_set_pixel(uint32_t x, uint32_t y, uint8_t color);

/**
 * @brief  Draw a horizontal line in frame buffer
 * @param  x     Start X
 * @param  y     Y position
 * @param  len   Line length in pixels
 * @param  color 1 = on, 0 = off
 */
hal_status_t bsp_oled_draw_hline(uint32_t x, uint32_t y, uint32_t len, uint8_t color);

/**
 * @brief  Draw a vertical line in frame buffer
 */
hal_status_t bsp_oled_draw_vline(uint32_t x, uint32_t y, uint32_t len, uint8_t color);

/**
 * @brief  Draw a line (any angle) using Bresenham's algorithm
 * @param  x0    Start X
 * @param  y0    Start Y
 * @param  x1    End X
 * @param  y1    End Y
 * @param  color 1 = on, 0 = off
 */
hal_status_t bsp_oled_draw_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint8_t color);

/**
 * @brief  Fill a rectangular area
 * @param  x      Start X
 * @param  y      Start Y
 * @param  width  Rectangle width
 * @param  height Rectangle height
 * @param  color  1 = fill, 0 = clear
 */
hal_status_t bsp_oled_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t color);

/**
 * @brief  Display a character at position
 * @param  x     Column (in pixels)
 * @param  y     Row (in page units for small font, pixel units for medium)
 * @param  ch    ASCII character (0x20~0x7E)
 * @param  font  Font size selection
 * @param  color 1 = white, 0 = black
 */
hal_status_t bsp_oled_write_char(uint32_t x, uint32_t y, char ch, bsp_oled_font_t font, uint8_t color);

/**
 * @brief  Display a string at position
 * @param  x     Column (in pixels)
 * @param  y     Row
 * @param  str   Null-terminated ASCII string
 * @param  font  Font size selection
 * @param  color 1 = white, 0 = black
 */
hal_status_t bsp_oled_write_string(uint32_t x, uint32_t y, const char *str, bsp_oled_font_t font, uint8_t color);

/**
 * @brief  Flush frame buffer to OLED display
 * @note   Must be called after drawing to update the screen
 */
hal_status_t bsp_oled_update_display(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_OLED_H */
