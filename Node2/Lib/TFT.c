#include "TFT.h"

static SPI_HandleTypeDef *tft_hspi;

// B?ng l?nh kh?i t?o ST7735R tiêu chu?n
static const uint8_t u8InitCmdList[] = {
    // C?u hình Frame Rate
    0xB1, 3, 0x01, 0x2C, 0x2D,
    0xB2, 3, 0x01, 0x2C, 0x2D,
    0xB3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,
    // C?u hình Power Sequence
    0xB4, 1, 0x07,
    0xC0, 3, 0xA2, 0x02, 0x84,
    0xC1, 1, 0xC5,
    0xC2, 2, 0x0A, 0x00,
    0xC3, 2, 0x8A, 0x2A,
    0xC4, 2, 0x8A, 0xEE,
    0xC5, 1, 0x0E,
    // Gamma Sequence
    0xE0, 16, 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
    0xE1, 16, 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
    LCD_CMD_END, 0
};

// G?i 1 byte SPI tr?c ti?p b?ng thanh ghi DR (t?i da t?c d?, không qua overhead HAL)
static inline void SPI1_Send(uint8_t data) {
    while (!(tft_hspi->Instance->SR & SPI_SR_TXE));
    *((__IO uint8_t *)&tft_hspi->Instance->DR) = data;
    while (!(tft_hspi->Instance->SR & SPI_SR_TXE));
    while (tft_hspi->Instance->SR & SPI_SR_BSY);
}

void writeCMDTFT(uint8_t cmd) {
    TFT_CS_LOW();
    TFT_DC_LOW(); // Command
    SPI1_Send(cmd);
    TFT_CS_HIGH();
}

void writeDataTFT(uint8_t data) {
    TFT_CS_LOW();
    TFT_DC_HIGH(); // Data
    SPI1_Send(data);
    TFT_CS_HIGH();
}

void sendCMDList(const uint8_t *cmdList) {
    uint8_t cmd = 0;
    uint8_t num = 0;
    while (1) {
        cmd = *cmdList++;
        num = *cmdList++;
        if (cmd == LCD_CMD_END) break;
        writeCMDTFT(cmd);
        for (uint8_t index = 0; index < num; index++) {
            writeDataTFT(*cmdList++);
        }
    }
}

void setPos(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    writeCMDTFT(0x2A);
    writeDataTFT(0x00);
    writeDataTFT(x1);
    writeDataTFT(0x00);
    writeDataTFT(x2);

    writeCMDTFT(0x2B);
    writeDataTFT(0x00);
    writeDataTFT(y1);
    writeDataTFT(0x00);
    writeDataTFT(y2);
}

void fullDisplay(uint16_t color) {
    setPos(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
    writeCMDTFT(0x2C);

    uint8_t high_byte = color >> 8;
    uint8_t low_byte = color & 0xFF;

    TFT_CS_LOW();
    TFT_DC_HIGH();

    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
        SPI1_Send(high_byte);
        SPI1_Send(low_byte);
    }

    TFT_CS_HIGH();
}

void drawPixel(uint8_t x, uint8_t y, uint16_t color) {
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    setPos(x, y, x, y);
    writeCMDTFT(0x2C);
    writeDataTFT(color >> 8);
    writeDataTFT(color & 0xFF);
}

void initTFT(SPI_HandleTypeDef *hspi) {
    tft_hspi = hspi;

    // Reset ph?n c?ng màn hình
    TFT_RST_LOW();
    HAL_Delay(20);
    TFT_RST_HIGH();
    HAL_Delay(150);

    // B?t d?u chu?i l?nh kh?i t?o
    writeCMDTFT(0x01); // Software Reset
    HAL_Delay(150);

    writeCMDTFT(0x11); // Sleep Out
    HAL_Delay(255);

    sendCMDList(u8InitCmdList);

    writeCMDTFT(0x36); // MADCTL: Chi?u quét và h? màu RGB
    writeDataTFT(0xC8);

    writeCMDTFT(0x3A); // 16-bit color format
    writeDataTFT(0x05);

    writeCMDTFT(0x20); // Display inversion off
    
    setPos(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

    writeCMDTFT(0x29); // Display ON
    HAL_Delay(100);
}

void drawChar(uint8_t x, uint8_t y, char ch, FontDef font, uint16_t color, uint16_t bg) {
    if (ch < 32 || ch > 126) ch = ' ';

    setPos(x, y, x + font.width - 1, y + font.height - 1);
    writeCMDTFT(0x2C);

    TFT_CS_LOW();
    TFT_DC_HIGH();

    for (uint32_t i = 0; i < font.height; i++) {
        uint16_t pixelData = font.data[(ch - 32) * font.height + i];
        for (uint32_t j = 0; j < font.width; j++) {
            // L?y m?u t?ng bit t? trái qua ph?i theo Font7x10
            uint16_t c = ((pixelData << j) & 0x8000) ? color : bg;
            SPI1_Send(c >> 8);
            SPI1_Send(c & 0xFF);
        }
    }

    TFT_CS_HIGH();
}

void drawString(uint8_t x, uint8_t y, char *str, FontDef font, uint16_t color, uint16_t bg) {
    while (*str) {
        if (x + font.width > TFT_WIDTH) {
            x = 0;
            y += font.height;
            if (y + font.height > TFT_HEIGHT) break;
        }
        drawChar(x, y, *str, font, color, bg);
        x += font.width;
        str++;
    }
}

void drawNumber(uint8_t x, uint8_t y, int number, FontDef font, uint16_t color, uint16_t bg) {
    char buf[16];
    sprintf(buf, "%d", number);
    drawString(x, y, buf, font, color, bg);
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy, e2;

    while (1) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    drawPixel(x0, y0 + r, color);
    drawPixel(x0, y0 - r, color);
    drawPixel(x0 + r, y0, color);
    drawPixel(x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;

        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 + x, y0 - y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 + y, y0 + x, color);
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 + y, y0 - x, color);
        drawPixel(x0 - y, y0 - x, color);
    }
}