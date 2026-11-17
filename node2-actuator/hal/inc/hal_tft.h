/**
 * @file hal_tft.h
 * @brief ST7735 initialization and SPI pixel transfer support.
 */

#ifndef HAL_TFT_H
#define HAL_TFT_H

#include <stdint.h>

#include "stm32f1xx_hal.h"

typedef struct {
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *dc_port;
    uint16_t dc_pin;
    GPIO_TypeDef *rst_port;
    uint16_t rst_pin;
} TftPins_t;

/**
 * @brief Bind TFT control pins and SPI, reset the panel and configure ST7735 RGB565 mode.
 * @param[in] hspi Initialized SPI handle connected to the display.
 * @param[in] pins Input mapping of the TFT CS, DC and reset GPIOs.
 */
void hal_tft_init(SPI_HandleTypeDef *hspi, const TftPins_t *pins);

#endif
