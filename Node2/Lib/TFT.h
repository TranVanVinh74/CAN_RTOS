#ifndef TFT_H
#define TFT_H

#include "stm32f1xx_hal.h"
#include "FONT.h"
#include <stdio.h>
#include <stdlib.h>

#define LCD_CMD_END   0xFF

// Kích thu?c 1.8 inch
#define TFT_WIDTH     128
#define TFT_HEIGHT    160

// Ð?nh nghia màu RGB565 thông d?ng
#define TFT_BLACK     0x0000
#define TFT_WHITE     0xFFFF
#define TFT_RED       0xF800
#define TFT_GREEN     0x07E0
#define TFT_BLUE      0x001F
#define TFT_YELLOW    0xFFE0
#define TFT_CYAN      0x07FF
#define TFT_MAGENTA   0xF81F
#define TFT_GRAY      0x8410

// Macro gán chân PB1 (CS), PB10 (DC/A0), PB11 (RST)
#define TFT_CS_LOW()    (GPIOB->BSRR = GPIO_BSRR_BR1)
#define TFT_CS_HIGH()   (GPIOB->BSRR = GPIO_BSRR_BS1)

#define TFT_DC_LOW()    (GPIOB->BSRR = GPIO_BSRR_BR10)
#define TFT_DC_HIGH()   (GPIOB->BSRR = GPIO_BSRR_BS10)

#define TFT_RST_LOW()   (GPIOB->BSRR = GPIO_BSRR_BR11)
#define TFT_RST_HIGH()  (GPIOB->BSRR = GPIO_BSRR_BS11)

// Khai báo hàm
void initTFT(SPI_HandleTypeDef *hspi);
void writeCMDTFT(uint8_t cmd);
void writeDataTFT(uint8_t data);
void setPos(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void fullDisplay(uint16_t color);
void drawPixel(uint8_t x, uint8_t y, uint16_t color);
void drawChar(uint8_t x, uint8_t y, char ch, FontDef font, uint16_t color, uint16_t bg);
void drawString(uint8_t x, uint8_t y, char *str, FontDef font, uint16_t color, uint16_t bg);
void drawNumber(uint8_t x, uint8_t y, int number, FontDef font, uint16_t color, uint16_t bg);
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

#endif /* TFT_H */