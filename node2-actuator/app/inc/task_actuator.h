/**
 * @file task_actuator.h
 * @brief Relay control and dispatch of CAN readings and debounced button events.
 */

#ifndef TASK_ACTUATOR_H
#define TASK_ACTUATOR_H

#include <stdint.h>

#include "FreeRTOS.h"

/**
 * @brief Register the CAN receive callback and create the CONTROL task.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_actuator_create(void);

/**
 * @brief Enqueue a batch of button presses and notify CONTROL from task context.
 * @param[in] events Press mask returned by hal_buttons_poll().
 * @note Waits for space if the FIFO is full.
 */
void task_actuator_post_buttons(uint8_t events);

#endif
