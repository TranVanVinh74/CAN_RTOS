#ifndef HAL_TFT_FONT_H
#define HAL_TFT_FONT_H

#include <stdint.h>

typedef struct {
    uint8_t width;
    uint8_t height;
    const uint16_t *glyphs;
} TftFont_t;

extern const TftFont_t tft_font_7x10;

#endif
