#include "hal_tft.h"

#include <stddef.h>
#include <stdlib.h>

#define ST7735_SWRESET 0x01U
#define ST7735_SLPOUT 0x11U
#define ST7735_INVOFF 0x20U
#define ST7735_DISPON 0x29U
#define ST7735_CASET 0x2AU
#define ST7735_RASET 0x2BU
#define ST7735_RAMWR 0x2CU
#define ST7735_MADCTL 0x36U
#define ST7735_COLMOD 0x3AU

#define ST7735_MADCTL_BGR 0x08U
#define ST7735_COLMOD_RGB565 0x05U
#define ST7735_CMD_LIST_END 0xFFU

#define TFT_SPI_TIMEOUT_MS 100U
#define TFT_RESET_HIGH_MS 10U
#define TFT_RESET_LOW_MS 20U
#define TFT_RESET_WAIT_MS 150U
#define TFT_SWRESET_WAIT_MS 150U
#define TFT_SLPOUT_WAIT_MS 255U
#define TFT_DISPON_WAIT_MS 100U

#define TFT_FONT_FIRST_CHAR 32U
#define TFT_FONT_LAST_CHAR 126U
#define TFT_FONT_MAX_WIDTH 16U

volatile uint32_t g_tftSPIErrors = 0;
volatile uint32_t g_tftSPIStatus = HAL_OK;
volatile uint32_t g_tftSPIHalError = 0;

static SPI_HandleTypeDef *tft_spi = NULL;
static TftPins_t tft_pins;

static const uint8_t st7735_init_cmds[] = {
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
    0xE0, 16, 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
              0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
    0xE1, 16, 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
              0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
    ST7735_CMD_LIST_END, 0
};

static void pin_write(GPIO_TypeDef *port, uint16_t pin, bool high) {
    HAL_GPIO_WritePin(port, pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void cs_set(bool high) {
    pin_write(tft_pins.cs_port, tft_pins.cs_pin, high);
}

static void dc_set(bool high) {
    pin_write(tft_pins.dc_port, tft_pins.dc_pin, high);
}

static void rst_set(bool high) {
    pin_write(tft_pins.rst_port, tft_pins.rst_pin, high);
}

static void spi_send(uint8_t byte) {
    if (g_tftSPIErrors != 0U) {
        return;
    }

    if (tft_spi == NULL) {
        g_tftSPIStatus = (uint32_t)HAL_ERROR;
        g_tftSPIErrors++;
        return;
    }

    HAL_StatusTypeDef status = HAL_SPI_Transmit(tft_spi, &byte, 1U, TFT_SPI_TIMEOUT_MS);

    g_tftSPIStatus = (uint32_t)status;
    g_tftSPIHalError = HAL_SPI_GetError(tft_spi);
    if (status != HAL_OK) {
        g_tftSPIErrors++;
    }
}

static void write_cmd(uint8_t cmd) {
    dc_set(false);
    cs_set(false);
    spi_send(cmd);
    cs_set(true);
}

static void write_data(uint8_t data) {
    dc_set(true);
    cs_set(false);
    spi_send(data);
    cs_set(true);
}

static void send_cmd_list(const uint8_t *list) {
    while (g_tftSPIErrors == 0U) {
        uint8_t cmd = *list++;
        uint8_t count = *list++;

        if (cmd == ST7735_CMD_LIST_END) {
            break;
        }

        write_cmd(cmd);
        for (uint8_t i = 0; i < count; i++) {
            write_data(*list++);
        }
    }
}

static void set_window(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    write_cmd(ST7735_CASET);
    write_data(0x00U);
    write_data(x1);
    write_data(0x00U);
    write_data(x2);

    write_cmd(ST7735_RASET);
    write_data(0x00U);
    write_data(y1);
    write_data(0x00U);
    write_data(y2);
}

static void send_color(uint16_t color) {
    spi_send((uint8_t)(color >> 8));
    spi_send((uint8_t)(color & 0xFFU));
}

static void draw_pixel(int32_t x, int32_t y, uint16_t color) {
    if ((x < 0) || (y < 0) || (x >= (int32_t)TFT_WIDTH) || (y >= (int32_t)TFT_HEIGHT) ||
        (g_tftSPIErrors != 0U)) {
        return;
    }

    set_window((uint8_t)x, (uint8_t)y, (uint8_t)x, (uint8_t)y);
    write_cmd(ST7735_RAMWR);
    write_data((uint8_t)(color >> 8));
    write_data((uint8_t)(color & 0xFFU));
}

static void draw_char(uint8_t x, uint8_t y, char ch, const TftFont_t *font, uint16_t color,
                      uint16_t bg) {
    if ((g_tftSPIErrors != 0U) || (font->width > TFT_FONT_MAX_WIDTH)) {
        return;
    }

    if (((uint16_t)x + font->width > TFT_WIDTH) || ((uint16_t)y + font->height > TFT_HEIGHT)) {
        return;
    }

    uint8_t code = (uint8_t)ch;
    if ((code < TFT_FONT_FIRST_CHAR) || (code > TFT_FONT_LAST_CHAR)) {
        code = (uint8_t)' ';
    }

    set_window(x, y, (uint8_t)(x + font->width - 1U), (uint8_t)(y + font->height - 1U));
    write_cmd(ST7735_RAMWR);

    dc_set(true);
    cs_set(false);

    for (uint32_t row = 0; row < font->height; row++) {
        uint16_t bits = font->glyphs[(code - TFT_FONT_FIRST_CHAR) * font->height + row];

        for (uint32_t col = 0; col < font->width; col++) {
            send_color(((bits & (0x8000U >> col)) != 0U) ? color : bg);
            if (g_tftSPIErrors != 0U) {
                cs_set(true);
                return;
            }
        }
    }

    cs_set(true);
}

void hal_tft_init(SPI_HandleTypeDef *hspi, const TftPins_t *pins) {
    tft_spi = hspi;
    g_tftSPIErrors = 0;
    g_tftSPIStatus = HAL_OK;
    g_tftSPIHalError = 0;

    if ((hspi == NULL) || (pins == NULL)) {
        g_tftSPIStatus = (uint32_t)HAL_ERROR;
        g_tftSPIErrors++;
        return;
    }

    tft_pins = *pins;

    cs_set(true);
    dc_set(true);

    rst_set(true);
    HAL_Delay(TFT_RESET_HIGH_MS);
    rst_set(false);
    HAL_Delay(TFT_RESET_LOW_MS);
    rst_set(true);
    HAL_Delay(TFT_RESET_WAIT_MS);

    write_cmd(ST7735_SWRESET);
    HAL_Delay(TFT_SWRESET_WAIT_MS);

    write_cmd(ST7735_SLPOUT);
    HAL_Delay(TFT_SLPOUT_WAIT_MS);

    send_cmd_list(st7735_init_cmds);

    write_cmd(ST7735_MADCTL);
    write_data(ST7735_MADCTL_BGR);

    write_cmd(ST7735_COLMOD);
    write_data(ST7735_COLMOD_RGB565);

    write_cmd(ST7735_INVOFF);

    set_window(0U, 0U, TFT_WIDTH - 1U, TFT_HEIGHT - 1U);

    write_cmd(ST7735_DISPON);
    HAL_Delay(TFT_DISPON_WAIT_MS);
}

bool hal_tft_ok(void) {
    return (tft_spi != NULL) && (g_tftSPIErrors == 0U);
}

void hal_tft_fill(uint16_t color) {
    if (g_tftSPIErrors != 0U) {
        return;
    }

    set_window(0U, 0U, TFT_WIDTH - 1U, TFT_HEIGHT - 1U);
    write_cmd(ST7735_RAMWR);

    dc_set(true);
    cs_set(false);

    for (uint32_t i = 0; i < (uint32_t)TFT_WIDTH * TFT_HEIGHT; i++) {
        send_color(color);
        if (g_tftSPIErrors != 0U) {
            break;
        }
    }

    cs_set(true);
}

void hal_tft_draw_string(uint8_t x, uint8_t y, const char *text, const TftFont_t *font,
                         uint16_t color, uint16_t bg) {
    if ((text == NULL) || (font == NULL) || (font->glyphs == NULL) || (font->width == 0U) ||
        (font->height == 0U)) {
        return;
    }

    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    while ((*text != '\0') && (g_tftSPIErrors == 0U)) {
        if (*text == '\r') {
            text++;
            continue;
        }

        if (*text == '\n') {
            cursor_x = 0U;
            cursor_y += font->height;
            text++;
            continue;
        }

        if (cursor_x + font->width > TFT_WIDTH) {
            cursor_x = 0U;
            cursor_y += font->height;
        }

        if (cursor_y + font->height > TFT_HEIGHT) {
            break;
        }

        draw_char((uint8_t)cursor_x, (uint8_t)cursor_y, *text, font, color, bg);
        cursor_x += font->width;
        text++;
    }
}

void hal_tft_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int32_t x = x0;
    int32_t y = y0;
    int32_t dx = abs((int)x1 - (int)x0);
    int32_t dy = -abs((int)y1 - (int)y0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    while (g_tftSPIErrors == 0U) {
        draw_pixel(x, y, color);

        if ((x == x1) && (y == y1)) {
            break;
        }

        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}
