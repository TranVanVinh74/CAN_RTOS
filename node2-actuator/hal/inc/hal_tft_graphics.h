#ifndef HAL_TFT_GRAPHICS_H
#define HAL_TFT_GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>

#include "hal_tft_font.h"

#define TFT_WIDTH 128U
#define TFT_HEIGHT 160U

#define TFT_BLACK 0x0000U
#define TFT_WHITE 0xFFFFU
#define TFT_RED 0xF800U
#define TFT_GREEN 0x07E0U
#define TFT_YELLOW 0xFFE0U
#define TFT_CYAN 0x07FFU
#define TFT_GREY 0x8410U

#define TFT_NAVY 0x0863U
#define TFT_CARD 0x10E5U
#define TFT_MUTED 0xAD55U
#define TFT_INACTIVE 0x8410U
#define TFT_ACCENT 0x4E7BU
#define TFT_SAFE 0x5EAFU
#define TFT_WARN 0xFE49U
#define TFT_ALARM 0xFAAAU
#define TFT_ALARM_BG 0xA126U
#define TFT_SELECTED 0x1949U
#define TFT_TRACK 0x2949U

typedef void (*TftRowPainter_t)(uint8_t row, uint16_t *pixels);

void hal_tft_draw_region(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                         TftRowPainter_t painter);
void hal_tft_fill_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t color);
void hal_tft_draw_bitmap(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                         const uint8_t *bits, uint16_t color, uint16_t bg, uint8_t scale);
void hal_tft_draw_alpha(uint8_t x, uint8_t y, uint8_t size, const uint8_t *mask,
                        uint16_t color, uint16_t bg);
bool hal_tft_ok(void);
void hal_tft_fill(uint16_t color);
void hal_tft_draw_string(uint8_t x, uint8_t y, const char *text, const TftFont_t *font,
                         uint16_t color, uint16_t bg);
void hal_tft_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

#endif
