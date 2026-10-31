#ifndef DHT11_H
#define DHT11_H

#include "stm32f1xx_hal.h"

typedef struct {
    float Temperature;
    float Humidity;
} DHT11_Data_t;

void DHT11_Init(TIM_HandleTypeDef *htim);
uint8_t DHT11_Read(DHT11_Data_t *DHT_Data);

#endif /* DHT11_H */