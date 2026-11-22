/**
 * @file task_buttons.h
 * @brief FreeRTOS task for polling the four Node2 buttons.
 */

#ifndef TASK_BUTTONS_H
#define TASK_BUTTONS_H

#include "FreeRTOS.h"

/**
 * @brief Create the BUTTONS task.
 * @return pdPASS on success; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_buttons_create(void);

#endif
