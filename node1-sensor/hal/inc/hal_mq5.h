/**
 * @file hal_mq5.h
 * @brief ADC calibration, MQ5 sampling and averaging diagnostics.
 */

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

/**
 * @brief Calibrate the ADC and bind it to the MQ5 driver.
 * @param[in] hadc Initialized ADC handle for the MQ5 analog input.
 */
void hal_mq5_init(ADC_HandleTypeDef *hadc);
/**
 * @brief Average successful MQ5 ADC conversions and retain the previous value when all conversions fail.
 * @param[in] samples Number of conversions to attempt; zero is treated as one sample.
 * @return Average raw ADC value, or the last valid value if no sample succeeds; inspect g_mq5Error for faults.
 */
uint16_t hal_mq5_read_average(uint8_t samples);

#endif
