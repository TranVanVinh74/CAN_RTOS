/**
 * @file app_cfg.h
 * @brief FreeRTOS task priorities, stack sizes and application timing constants.
 */

#ifndef APP_CFG_H
#define APP_CFG_H

#include "FreeRTOS.h"

#define TASK_CONTROL_PRIO (tskIDLE_PRIORITY + 3)
#define TASK_CONTROL_STACK_WORDS 512U
#define TASK_BUTTONS_PRIO (tskIDLE_PRIORITY + 2)
#define TASK_BUTTONS_STACK_WORDS 256U
#define TASK_UI_PRIO (tskIDLE_PRIORITY + 1)
#define TASK_UI_STACK_WORDS 512U

#define CONTROL_POLL_MS 20U
#define BUTTON_SCAN_MS 20U
#define UI_FRAME_MS 40U

#define SENSOR_QUEUE_LENGTH 5U
#define BUTTON_QUEUE_LENGTH 8U
#define UI_QUEUE_LENGTH 1U
#define SENSOR_FRESH_TIMEOUT_MS 3000U

#define THRESHOLD_SYNC_INTERVAL_MS 1000U
#define THRESHOLD_SYNC_RETRY_MS 100U

#endif
