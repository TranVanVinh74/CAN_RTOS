/**
 * @file task_modem.h
 * @brief Cellular modem and call lifecycle task.
 */

#ifndef TASK_MODEM_H
#define TASK_MODEM_H

#include "FreeRTOS.h"

/**
 * @brief Create the MODEM task that owns call and modem state.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_modem_create(void);

#endif
