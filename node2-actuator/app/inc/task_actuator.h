/**
 * @file task_actuator.h
 * @brief CAN sensor consumption, relay control, buttons and UI servicing.
 */

#ifndef TASK_ACTUATOR_H
#define TASK_ACTUATOR_H

#include "FreeRTOS.h"

/**
 * @brief Register the CAN receive callback and create the ACTUATOR task.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_actuator_create(void);

#endif
