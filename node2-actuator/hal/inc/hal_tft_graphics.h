/**
 * @file hal_tft_graphics.h
 * @brief TFT dimensions and row-painter interface for RGB565 rectangles.
 */

#ifndef HAL_TFT_GRAPHICS_H
#define HAL_TFT_GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>

#define TFT_WIDTH 128U
#define TFT_HEIGHT 160U

/**
 * @brief Fill one row of a rectangle requested by the TFT driver.
 * @param[in] row Zero-based row index relative to the requested rectangle.
 * @param[out] pixels Buffer to fill with the rectangle width in RGB565 pixels.
 * @note Called synchronously during drawing; the buffer must not be retained.
 */
typedef void (*TftRowPainter_t)(uint8_t row, uint16_t *pixels);

/**
 * @brief Render a rectangle row by row through a painter callback and stream RGB565 pixels over SPI.
 * @param[in] x Left coordinate in pixels.
 * @param[in] y Top coordinate in pixels.
 * @param[in] width Rectangle width in pixels.
 * @param[in] height Rectangle height in pixels.
 * @param[in] painter Synchronous callback that fills width RGB565 pixels for each zero-based region row.
 */
void hal_tft_draw_region(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                         TftRowPainter_t painter);
/**
 * @brief Check that a TFT SPI handle is bound and no SPI error has been recorded.
 * @return true if the TFT driver is ready and has no recorded SPI error.
 */
bool hal_tft_ok(void);

#endif
