/**
 * @file task_alarm.h
 * @brief CAN reception and alarm evaluation task.
 */

#ifndef TASK_ALARM_H
#define TASK_ALARM_H

#include "FreeRTOS.h"

/**
 * @brief Register the CAN receive callback and create the ALARM task.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_alarm_create(void);

#endif
