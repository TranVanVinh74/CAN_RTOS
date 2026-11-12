#ifndef HAL_TFT_GRAPHICS_H
#define HAL_TFT_GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>

#define TFT_WIDTH 128U
#define TFT_HEIGHT 160U

typedef void (*TftRowPainter_t)(uint8_t row, uint16_t *pixels);

void hal_tft_draw_region(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                         TftRowPainter_t painter);
bool hal_tft_ok(void);

#endif
