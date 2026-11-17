/**
 * @file hal_tft.c
 * @brief ST7735 initialization and SPI pixel transfer support.
 */

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

/**
 * @brief Drive a display control GPIO to the requested level.
 * @param[in] port GPIO port to drive.
 * @param[in] pin GPIO pin mask to drive.
 * @param[in] high true for a high GPIO level; false for a low level.
 */
static void pin_write(GPIO_TypeDef *port, uint16_t pin, bool high) {
    HAL_GPIO_WritePin(port, pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief Set the TFT chip-select GPIO level.
 * @param[in] high true for a high GPIO level; false for a low level.
 */
static void cs_set(bool high) {
    pin_write(tft_pins.cs_port, tft_pins.cs_pin, high);
}

/**
 * @brief Set the TFT data/command GPIO level.
 * @param[in] high true for a high GPIO level; false for a low level.
 */
static void dc_set(bool high) {
    pin_write(tft_pins.dc_port, tft_pins.dc_pin, high);
}

/**
 * @brief Set the TFT reset GPIO level.
 * @param[in] high true for a high GPIO level; false for a low level.
 */
static void rst_set(bool high) {
    pin_write(tft_pins.rst_port, tft_pins.rst_pin, high);
}

/* The first SPI failure stops all transfers; the relay keeps working without a display. */
/**
 * @brief Transmit TFT SPI bytes and stop subsequent transfers after the first failure.
 * @param[in] data Input buffer containing at least size bytes.
 * @param[in] size Number of bytes to transmit.
 */
static void spi_transmit(uint8_t *data, uint16_t size) {
    if (spi_failed) {
        return;
    }
    if ((tft_spi == NULL) ||
        (HAL_SPI_Transmit(tft_spi, data, size, TFT_SPI_TIMEOUT_MS) != HAL_OK)) {
        spi_failed = true;
    }
}

/**
 * @brief Transmit one TFT SPI byte unless a previous SPI error has stopped the driver.
 * @param[in] byte Byte to transmit.
 */
static void spi_send(uint8_t byte) {
    spi_transmit(&byte, 1U);
}

/**
 * @brief Send one ST7735 command with the data/command line low.
 * @param[in] cmd ST7735 command byte.
 */
static void write_cmd(uint8_t cmd) {
    dc_set(false);
    cs_set(false);
    spi_send(cmd);
    cs_set(true);
}

/**
 * @brief Send one ST7735 parameter byte with the data/command line high.
 * @param[in] data ST7735 parameter byte to transmit.
 */
static void write_data(uint8_t data) {
    dc_set(true);
    cs_set(false);
    spi_send(data);
    cs_set(true);
}

/**
 * @brief Execute a packed ST7735 command list until its sentinel or an SPI error.
 * @param[in] list Packed command/count/parameter sequence terminated by ST7735_CMD_LIST_END.
 */
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

/**
 * @brief Set the inclusive ST7735 column and row address window.
 * @param[in] x1 First column of the inclusive window.
 * @param[in] y1 First row of the inclusive window.
 * @param[in] x2 Last column of the inclusive window.
 * @param[in] y2 Last row of the inclusive window.
 */
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

/**
 * @brief Bind TFT control pins and SPI, reset the panel and configure ST7735 RGB565 mode.
 * @param[in] hspi Initialized SPI handle connected to the display.
 * @param[in] pins Input mapping of the TFT CS, DC and reset GPIOs.
 */
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

/**
 * @brief Check that a TFT SPI handle is bound and no SPI error has been recorded.
 * @return true if the TFT driver is ready and has no recorded SPI error.
 */
bool hal_tft_ok(void) {
    return (tft_spi != NULL) && !spi_failed;
}

/**
 * @brief Check that a nonempty drawing rectangle fits the panel and the driver has no SPI error.
 * @param[in] x Left coordinate in pixels.
 * @param[in] y Top coordinate in pixels.
 * @param[in] width Rectangle width in pixels.
 * @param[in] height Rectangle height in pixels.
 * @return true if the rectangle fits and SPI is healthy; otherwise false.
 */
static bool valid_rect(uint8_t x, uint8_t y, uint16_t width, uint16_t height) {
    return !spi_failed && (width > 0U) && (height > 0U) &&
           ((uint16_t)x + width <= TFT_WIDTH) && ((uint16_t)y + height <= TFT_HEIGHT);
}

/**
 * @brief Render a rectangle row by row through a painter callback and stream RGB565 pixels over SPI.
 * @param[in] x Left coordinate in pixels.
 * @param[in] y Top coordinate in pixels.
 * @param[in] width Rectangle width in pixels.
 * @param[in] height Rectangle height in pixels.
 * @param[in] painter Synchronous callback that fills width RGB565 pixels for each zero-based region row.
 */
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
