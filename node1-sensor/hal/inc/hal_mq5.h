#ifndef HAL_MQ5_H
#define HAL_MQ5_H

#include <stdint.h>

#include "stm32f1xx_hal.h"

typedef enum {
    MQ5_ERR_NONE = 0,
    MQ5_ERR_NO_ADC = 1,
    MQ5_ERR_CALIBRATION = 2,
    MQ5_ERR_START = 3,
    MQ5_ERR_POLL = 4,
    MQ5_ERR_STOP = 5
} Mq5Error_t;

extern volatile uint8_t g_mq5Error;
extern volatile uint32_t g_mq5ReadErrors;

void hal_mq5_init(ADC_HandleTypeDef *hadc);
uint16_t hal_mq5_read_average(uint8_t samples);

#endif
