#ifndef HAL_MQ5_H
#define HAL_MQ5_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

bool hal_mq5_init(ADC_HandleTypeDef *hadc);
bool hal_mq5_read_average(uint8_t samples, uint16_t *gas_raw);

#endif
