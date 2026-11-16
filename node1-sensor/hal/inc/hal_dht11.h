#ifndef HAL_DHT11_H
#define HAL_DHT11_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

#define DHT11_FRAME_BYTES 5U

typedef struct {
    uint8_t temperature_c;
    uint8_t humidity_pct;
} Dht11Reading_t;

bool hal_dht11_init(TIM_HandleTypeDef *htim, GPIO_TypeDef *port, uint16_t pin);
bool hal_dht11_read(Dht11Reading_t *reading);

#endif
