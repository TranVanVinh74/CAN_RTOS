/**
 * @file hal_relay.h
 * @brief Relay GPIO polarity, commanded state and transition diagnostics.
 */

#ifndef HAL_RELAY_H
#define HAL_RELAY_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

/**
 * @brief Bind the relay GPIO and initialize it to the off state.
 * @param[in] port Relay GPIO port.
 * @param[in] pin Relay GPIO pin mask.
 * @param[in] on_level GPIO level that energizes the relay.
 */
void hal_relay_init(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState on_level);
/**
 * @brief Drive the relay output and retain the commanded state.
 * @param[in] on true to energize the relay; false to turn it off.
 */
void hal_relay_set(bool on);
/**
 * @brief Read the relay state last commanded by the driver.
 * @return true if the relay is commanded on; otherwise false. This is not physical relay feedback.
 */
bool hal_relay_is_on(void);

#endif
