#ifndef HAL_DHT11_H
#define HAL_DHT11_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

#define DHT11_FRAME_BYTES 5U

typedef enum {
    DHT11_ERR_NONE = 0,
    DHT11_ERR_TIMER = 1,
    DHT11_ERR_RESPONSE_LOW = 2,
    DHT11_ERR_RESPONSE_HIGH = 3,
    DHT11_ERR_RESPONSE_END = 4,
    DHT11_ERR_BIT_START = 5,
    DHT11_ERR_BIT_END = 6,
    DHT11_ERR_CHECKSUM = 7,
    DHT11_ERR_ARGUMENT = 8
} Dht11Error_t;

typedef struct {
    uint8_t temperature_c;
    uint8_t humidity_pct;
} Dht11Reading_t;

extern volatile uint8_t g_dhtError;
extern volatile uint8_t g_dhtBitIndex;
extern volatile uint8_t g_dhtBytes[DHT11_FRAME_BYTES];

bool hal_dht11_init(TIM_HandleTypeDef *htim, GPIO_TypeDef *port, uint16_t pin);
bool hal_dht11_read(Dht11Reading_t *reading);

#endif
