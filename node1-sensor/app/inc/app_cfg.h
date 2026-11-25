/**
 * @file app_cfg.h
 * @brief FreeRTOS task priorities, stack sizes and application timing constants.
 */

#ifndef APP_CFG_H
#define APP_CFG_H

#include "FreeRTOS.h"

#define TASK_DHT11_PRIO (tskIDLE_PRIORITY + 2)
#define TASK_DHT11_STACK_WORDS 256U
#define TASK_MQ5_PRIO (tskIDLE_PRIORITY + 2)
#define TASK_MQ5_STACK_WORDS 256U
#define TASK_MQ5_SAMPLES 8U

#define TASK_CAN_TX_PRIO (tskIDLE_PRIORITY + 3)
#define TASK_CAN_TX_STACK_WORDS 256U
#define SENSOR_PERIOD_MS 500U
#define SENSOR_RESULT_QUEUE_LENGTH 1U
#define SENSOR_REQUEST_WAIT_MS 1000U
#define SENSOR_RESULT_TIMEOUT_MS 200U

#endif
