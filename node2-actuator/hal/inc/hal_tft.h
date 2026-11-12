#ifndef HAL_TFT_H
#define HAL_TFT_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

#include "hal_tft_graphics.h"

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

#endif
