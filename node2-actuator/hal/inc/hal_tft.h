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

void hal_tft_init(SPI_HandleTypeDef *hspi, const TftPins_t *pins);

#endif
