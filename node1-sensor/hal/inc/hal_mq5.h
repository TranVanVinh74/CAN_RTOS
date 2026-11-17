/**
 * @file hal_mq5.h
 * @brief ADC calibration, MQ5 sampling and averaging diagnostics.
 */

#ifndef HAL_MQ5_H
#define HAL_MQ5_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

/**
 * @brief Calibrate the ADC, bind the MQ5 input and reset the retained reading.
 * @param[in] hadc Initialized ADC handle for the MQ5 analog channel.
 * @return true if a non-NULL ADC handle was calibrated successfully; otherwise false.
 */
bool hal_mq5_init(ADC_HandleTypeDef *hadc);
/**
 * @brief Average successful conversions while reporting whether every requested sample succeeded.
 * @param[in] samples Number of ADC conversions to attempt; zero is treated as one.
 * @param[out] gas_raw Non-NULL pointer receiving the average of successful samples, or the retained value if all conversions fail.
 * @return true only if all requested conversions succeeded; false if any conversion failed.
 */
bool hal_mq5_read_average(uint8_t samples, uint16_t *gas_raw);

#endif
