#include "TFT.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static SPI_HandleTypeDef *tft_hspi = NULL;

volatile uint32_t g_tftSPIErrors = 0;
volatile uint32_t g_tftSPIStatus = HAL_OK;
volatile uint32_t g_tftSPIHalError = 0;

/* Bang khoi tao giong ban test da hien man. */
static const uint8_t u8InitCmdList[] = {
    0xB1, 3, 0x01, 0x2C, 0x2D,
    0xB2, 3, 0x01, 0x2C, 0x2D,
    0xB3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,
    0xB4, 1, 0x07,

    0xC0, 3, 0xA2, 0x02, 0x84,
    0xC1, 1, 0xC5,
    0xC2, 2, 0x0A, 0x00,
    0xC3, 2, 0x8A, 0x2A,
    0xC4, 2, 0x8A, 0xEE,
    0xC5, 1, 0x0E,

    0xE0, 16,
    0x02, 0x1C, 0x07, 0x12,
    0x37, 0x32, 0x29, 0x2D,
    0x29, 0x25, 0x2B, 0x39,
    0x00, 0x01, 0x03, 0x10,

    0xE1, 16,
    0x03, 0x1D, 0x07, 0x06,
    0x2E, 0x2C, 0x29, 0x2D,
    0x2E, 0x2E, 0x37, 0x3F,
    0x00, 0x00, 0x02, 0x10,

    LCD_CMD_END, 0
};

/* Gui SPI bang HAL, giu lai loi dau tien de debug. */
static void SPI1_Send(uint8_t data)
{
    HAL_StatusTypeDef status;

    if (g_tftSPIErrors != 0U)
    {
        return;
    }

    if (tft_hspi == NULL)
    {
        g_tftSPIStatus = (uint32_t)HAL_ERROR;
        g_tftSPIErrors++;
        return;
    }

    status = HAL_SPI_Transmit(tft_hspi, &data, 1, 100);

    g_tftSPIStatus = (uint32_t)status;
    g_tftSPIHalError = HAL_SPI_GetError(tft_hspi);

    if (status != HAL_OK)
    {
        g_tftSPIErrors++;
    }
}

void writeCMDTFT(uint8_t cmd)
{
    TFT_DC_LOW();
    TFT_CS_LOW();

    SPI1_Send(cmd);

    TFT_CS_HIGH();
}

void writeDataTFT(uint8_t data)
{
    TFT_DC_HIGH();
    TFT_CS_LOW();

    SPI1_Send(data);

    TFT_CS_HIGH();
}

void sendCMDList(const uint8_t *cmdList)
{
    uint8_t cmd;
    uint8_t count;
    uint8_t i;

    if (cmdList == NULL)
    {
        return;
    }

    while (g_tftSPIErrors == 0U)
    {
        cmd = *cmdList++;
        count = *cmdList++;

        if (cmd == LCD_CMD_END)
        {
            break;
        }

        writeCMDTFT(cmd);

        for (i = 0; i < count; i++)
        {
            writeDataTFT(*cmdList++);
        }
    }
}

void setPos(uint8_t x1, uint8_t y1,
            uint8_t x2, uint8_t y2)
{
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

void initTFT(SPI_HandleTypeDef *hspi)
{
    tft_hspi = hspi;

    g_tftSPIErrors = 0;
    g_tftSPIStatus = HAL_OK;
    g_tftSPIHalError = 0;

    if (tft_hspi == NULL)
    {
        g_tftSPIStatus = (uint32_t)HAL_ERROR;
        g_tftSPIErrors++;
        return;
    }

    TFT_CS_HIGH();
    TFT_DC_HIGH();

    /* Hardware reset. Dung nguyen chan trong TFT.h. */
    TFT_RST_HIGH();
    HAL_Delay(10);

    TFT_RST_LOW();
    HAL_Delay(20);

    TFT_RST_HIGH();
    HAL_Delay(150);

    /* Software reset. */
    writeCMDTFT(0x01);
    HAL_Delay(150);

    /* Sleep out. */
    writeCMDTFT(0x11);
    HAL_Delay(255);

    sendCMDList(u8InitCmdList);

    /* Memory access control: giong ban test. */
    writeCMDTFT(0x36);
    writeDataTFT(0x08);

    /* RGB565: 16 bit/pixel. */
    writeCMDTFT(0x3A);
    writeDataTFT(0x05);

    /* Display inversion off. */
    writeCMDTFT(0x20);

    setPos(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

    /* Display on. */
    writeCMDTFT(0x29);
    HAL_Delay(100);
}

void fullDisplay(uint16_t color)
{
    uint32_t i;
    uint8_t highByte;
    uint8_t lowByte;

    if (g_tftSPIErrors != 0U)
    {
        return;
    }

    highByte = (uint8_t)(color >> 8);
    lowByte = (uint8_t)(color & 0xFFU);

    setPos(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
    writeCMDTFT(0x2C);

    TFT_DC_HIGH();
    TFT_CS_LOW();

    for (i = 0; i < (uint32_t)TFT_WIDTH * TFT_HEIGHT; i++)
    {
        /* RGB565: byte cao truoc, byte thap sau. */
        SPI1_Send(highByte);
        SPI1_Send(lowByte);

        if (g_tftSPIErrors != 0U)
        {
            break;
        }
    }

    TFT_CS_HIGH();
}

void drawPixel(uint8_t x, uint8_t y, uint16_t color)
{
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT ||
        g_tftSPIErrors != 0U)
    {
        return;
    }

    setPos(x, y, x, y);
    writeCMDTFT(0x2C);

    writeDataTFT((uint8_t)(color >> 8));
    writeDataTFT((uint8_t)(color & 0xFFU));
}

void drawChar(uint8_t x, uint8_t y, char ch,
              FontDef font, uint16_t color, uint16_t bg)
{
    uint32_t row;
    uint32_t col;
    uint16_t pixelData;
    uint16_t pixelColor;
    uint8_t character;

    if (g_tftSPIErrors != 0U ||
        font.data == NULL ||
        font.width == 0U ||
        font.height == 0U ||
        font.width > 16U)
    {
        return;
    }

    if ((uint16_t)x + font.width > TFT_WIDTH ||
        (uint16_t)y + font.height > TFT_HEIGHT)
    {
        return;
    }

    character = (uint8_t)ch;

    if (character < 32U || character > 126U)
    {
        character = (uint8_t)' ';
    }

    setPos(x, y,
           (uint8_t)(x + font.width - 1U),
           (uint8_t)(y + font.height - 1U));

    writeCMDTFT(0x2C);

    TFT_DC_HIGH();
    TFT_CS_LOW();

    for (row = 0; row < font.height; row++)
    {
        pixelData =
            font.data[(character - 32U) * font.height + row];

        for (col = 0; col < font.width; col++)
        {
            if ((pixelData & (0x8000U >> col)) != 0U)
            {
                pixelColor = color;
            }
            else
            {
                pixelColor = bg;
            }

            SPI1_Send((uint8_t)(pixelColor >> 8));
            SPI1_Send((uint8_t)(pixelColor & 0xFFU));

            if (g_tftSPIErrors != 0U)
            {
                TFT_CS_HIGH();
                return;
            }
        }
    }

    TFT_CS_HIGH();
}

void drawString(uint8_t x, uint8_t y, char *str,
                FontDef font, uint16_t color, uint16_t bg)
{
    uint16_t cursorX = x;
    uint16_t cursorY = y;

    if (str == NULL ||
        font.width == 0U ||
        font.height == 0U)
    {
        return;
    }

    while (*str != '\0' && g_tftSPIErrors == 0U)
    {
        if (*str == '\r')
        {
            str++;
            continue;
        }

        if (*str == '\n')
        {
            cursorX = 0;
            cursorY += font.height;
            str++;
            continue;
        }

        if (cursorX + font.width > TFT_WIDTH)
        {
            cursorX = 0;
            cursorY += font.height;
        }

        if (cursorY + font.height > TFT_HEIGHT)
        {
            break;
        }

        drawChar((uint8_t)cursorX,
                 (uint8_t)cursorY,
                 *str,
                 font,
                 color,
                 bg);

        cursorX += font.width;
        str++;
    }
}

void drawNumber(uint8_t x, uint8_t y, int number,
                FontDef font, uint16_t color, uint16_t bg)
{
    char buf[16];

    snprintf(buf, sizeof(buf), "%d", number);
    drawString(x, y, buf, font, color, bg);
}

/* Kiem tra toa do am truoc khi chuyen sang uint8_t. */
static void TFT_DrawPixelSigned(int32_t x, int32_t y,
                                uint16_t color)
{
    if (x < 0 || y < 0 ||
        x >= TFT_WIDTH || y >= TFT_HEIGHT)
    {
        return;
    }

    drawPixel((uint8_t)x, (uint8_t)y, color);
}

void drawLine(int16_t x0, int16_t y0,
              int16_t x1, int16_t y1, uint16_t color)
{
    int32_t x = x0;
    int32_t y = y0;
    int32_t dx = abs((int)x1 - (int)x0);
    int32_t dy = -abs((int)y1 - (int)y0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;
    int32_t e2;

    while (g_tftSPIErrors == 0U)
    {
        TFT_DrawPixelSigned(x, y, color);

        if (x == x1 && y == y1)
        {
            break;
        }

        e2 = 2 * err;

        if (e2 >= dy)
        {
            err += dy;
            x += sx;
        }

        if (e2 <= dx)
        {
            err += dx;
            y += sy;
        }
    }
}

void drawCircle(int16_t x0, int16_t y0,
                int16_t r, uint16_t color)
{
    int32_t f;
    int32_t ddF_x;
    int32_t ddF_y;
    int32_t x;
    int32_t y;

    if (r < 0 || g_tftSPIErrors != 0U)
    {
        return;
    }

    f = 1 - (int32_t)r;
    ddF_x = 1;
    ddF_y = -2 * (int32_t)r;
    x = 0;
    y = r;

    TFT_DrawPixelSigned(x0, (int32_t)y0 + r, color);
    TFT_DrawPixelSigned(x0, (int32_t)y0 - r, color);
    TFT_DrawPixelSigned((int32_t)x0 + r, y0, color);
    TFT_DrawPixelSigned((int32_t)x0 - r, y0, color);

    while (x < y && g_tftSPIErrors == 0U)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }

        x++;
        ddF_x += 2;
        f += ddF_x;

        TFT_DrawPixelSigned(x0 + x, y0 + y, color);
        TFT_DrawPixelSigned(x0 - x, y0 + y, color);
        TFT_DrawPixelSigned(x0 + x, y0 - y, color);
        TFT_DrawPixelSigned(x0 - x, y0 - y, color);

        TFT_DrawPixelSigned(x0 + y, y0 + x, color);
        TFT_DrawPixelSigned(x0 - y, y0 + x, color);
        TFT_DrawPixelSigned(x0 + y, y0 - x, color);
        TFT_DrawPixelSigned(x0 - y, y0 - x, color);
    }
}