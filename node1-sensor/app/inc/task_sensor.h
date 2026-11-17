/**
 * @file task_sensor.h
 * @brief Periodic MQ5 and DHT11 sampling with sensor fault reporting.
 */

#ifndef TASK_SENSOR_H
#define TASK_SENSOR_H

#include "FreeRTOS.h"

/**
 * @brief Create the SENSOR task that samples MQ5 and DHT11.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_sensor_create(void);

#endif
