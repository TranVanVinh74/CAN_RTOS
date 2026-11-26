/**
 * @file task_ui.h
 * @brief FreeRTOS task for rendering the Node2 display.
 */

#ifndef TASK_UI_H
#define TASK_UI_H

#include "FreeRTOS.h"

/**
 * @brief Create the UI task.
 * @return pdPASS on success; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_ui_create(void);

#endif
