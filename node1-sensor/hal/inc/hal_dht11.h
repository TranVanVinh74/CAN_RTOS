/**
 * @file hal_dht11.h
 * @brief GPIO pulse capture and checksum validation for DHT11 readings.
 */

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

/**
 * @brief Bind the DHT11 GPIO and start its microsecond timer.
 * @param[in] htim Initialized timer configured with a 1 MHz counter for pulse timing.
 * @param[in] port GPIO port connected to the DHT11 data line.
 * @param[in] pin GPIO pin mask connected to the DHT11 data line.
 * @return true if the timer starts and the GPIO is configured; otherwise false.
 */
bool hal_dht11_init(TIM_HandleTypeDef *htim, GPIO_TypeDef *port, uint16_t pin);
/**
 * @brief Read and checksum a DHT11 frame, suspending task scheduling during pulse capture.
 * @param[out] reading Output temperature in degrees Celsius and humidity in percent; updated only on success.
 * @return true if a valid frame was decoded; false on invalid arguments, timer failure, timeout or checksum error.
 */
bool hal_dht11_read(Dht11Reading_t *reading);

#endif
