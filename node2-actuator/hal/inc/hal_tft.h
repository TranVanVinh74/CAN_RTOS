#ifndef HAL_TFT_H
#define HAL_TFT_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

#include "hal_tft_font.h"

#define TFT_WIDTH 128U
#define TFT_HEIGHT 160U

#define TFT_BLACK 0x0000U
#define TFT_WHITE 0xFFFFU
#define TFT_RED 0xF800U
#define TFT_GREEN 0x07E0U
#define TFT_YELLOW 0xFFE0U
#define TFT_CYAN 0x07FFU

typedef struct {
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *dc_port;
    uint16_t dc_pin;
    GPIO_TypeDef *rst_port;
    uint16_t rst_pin;
} TftPins_t;

extern volatile uint32_t g_tftSPIErrors;
extern volatile uint32_t g_tftSPIStatus;
extern volatile uint32_t g_tftSPIHalError;

void hal_tft_init(SPI_HandleTypeDef *hspi, const TftPins_t *pins);
bool hal_tft_ok(void);
void hal_tft_fill(uint16_t color);
void hal_tft_draw_string(uint8_t x, uint8_t y, const char *text, const TftFont_t *font,
                         uint16_t color, uint16_t bg);
void hal_tft_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

#endif
