#include "hal_tft.h"

#include <stddef.h>

#include "hal_tft_graphics.h"

#define ST7735_SWRESET 0x01U
#define ST7735_SLPOUT 0x11U
#define ST7735_INVOFF 0x20U
#define ST7735_DISPON 0x29U
#define ST7735_CASET 0x2AU
#define ST7735_RASET 0x2BU
#define ST7735_RAMWR 0x2CU
#define ST7735_MADCTL 0x36U
#define ST7735_COLMOD 0x3AU
#define ST7735_FRMCTR1 0xB1U
#define ST7735_FRMCTR2 0xB2U
#define ST7735_FRMCTR3 0xB3U
#define ST7735_INVCTR 0xB4U
#define ST7735_PWCTR1 0xC0U
#define ST7735_PWCTR2 0xC1U
#define ST7735_PWCTR3 0xC2U
#define ST7735_PWCTR4 0xC3U
#define ST7735_PWCTR5 0xC4U
#define ST7735_VMCTR1 0xC5U
#define ST7735_GMCTRP1 0xE0U
#define ST7735_GMCTRN1 0xE1U

#define ST7735_MADCTL_RGB 0x00U
#define ST7735_COLMOD_RGB565 0x05U
#define ST7735_CMD_LIST_END 0xFFU

#define TFT_SPI_TIMEOUT_MS 100U
#define TFT_RESET_HIGH_MS 10U
#define TFT_RESET_LOW_MS 20U
#define TFT_RESET_WAIT_MS 150U
#define TFT_SWRESET_WAIT_MS 150U
#define TFT_SLPOUT_WAIT_MS 255U
#define TFT_DISPON_WAIT_MS 100U

static SPI_HandleTypeDef *tft_spi = NULL;
static TftPins_t tft_pins;
static uint8_t pixel_line[TFT_WIDTH * 2U];
static bool spi_failed = false;

static const uint8_t st7735_init_cmds[] = {
    ST7735_FRMCTR1, 3, 0x01, 0x2C, 0x2D,
    ST7735_FRMCTR2, 3, 0x01, 0x2C, 0x2D,
    ST7735_FRMCTR3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,
    ST7735_INVCTR, 1, 0x07,
    ST7735_PWCTR1, 3, 0xA2, 0x02, 0x84,
    ST7735_PWCTR2, 1, 0xC5,
    ST7735_PWCTR3, 2, 0x0A, 0x00,
    ST7735_PWCTR4, 2, 0x8A, 0x2A,
    ST7735_PWCTR5, 2, 0x8A, 0xEE,
    ST7735_VMCTR1, 1, 0x0E,
    ST7735_GMCTRP1, 16, 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
                    0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16, 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
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

static void spi_transmit(uint8_t *data, uint16_t size) {
    if (spi_failed) {
        return;
    }
    if ((tft_spi == NULL) ||
        (HAL_SPI_Transmit(tft_spi, data, size, TFT_SPI_TIMEOUT_MS) != HAL_OK)) {
        spi_failed = true;
    }
}

static void spi_send(uint8_t byte) {
    spi_transmit(&byte, 1U);
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
    while (!spi_failed) {
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

void hal_tft_init(SPI_HandleTypeDef *hspi, const TftPins_t *pins) {
    tft_spi = hspi;
    spi_failed = false;

    if ((hspi == NULL) || (pins == NULL)) {
        spi_failed = true;
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
    write_data(ST7735_MADCTL_RGB);

    write_cmd(ST7735_COLMOD);
    write_data(ST7735_COLMOD_RGB565);

    write_cmd(ST7735_INVOFF);

    set_window(0U, 0U, TFT_WIDTH - 1U, TFT_HEIGHT - 1U);

    write_cmd(ST7735_DISPON);
    HAL_Delay(TFT_DISPON_WAIT_MS);
}

bool hal_tft_ok(void) {
    return (tft_spi != NULL) && !spi_failed;
}

static bool valid_rect(uint8_t x, uint8_t y, uint16_t width, uint16_t height) {
    return !spi_failed && (width > 0U) && (height > 0U) &&
           ((uint16_t)x + width <= TFT_WIDTH) && ((uint16_t)y + height <= TFT_HEIGHT);
}

void hal_tft_draw_region(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                         TftRowPainter_t painter) {
    static uint16_t row_pixels[TFT_WIDTH];
    if ((painter == NULL) || !valid_rect(x, y, width, height)) {
        return;
    }
    set_window(x, y, (uint8_t)(x + width - 1U), (uint8_t)(y + height - 1U));
    write_cmd(ST7735_RAMWR);
    dc_set(true);
    cs_set(false);
    uint16_t used = 0U;
    for (uint8_t row = 0U; row < height && !spi_failed; row++) {
        painter(row, row_pixels);
        for (uint8_t col = 0U; col < width; col++) {
            pixel_line[used++] = (uint8_t)(row_pixels[col] >> 8);
            pixel_line[used++] = (uint8_t)row_pixels[col];
            if (used == sizeof(pixel_line)) {
                spi_transmit(pixel_line, used);
                used = 0U;
            }
        }
    }
    if (used != 0U) {
        spi_transmit(pixel_line, used);
    }
    cs_set(true);
}
