/**
 * @file task_mq5.h
 * @brief MQ5 sampling task.
 */

#ifndef TASK_MQ5_H
#define TASK_MQ5_H

#include "FreeRTOS.h"

/**
 * @brief Create the MQ5 task that services sampling requests.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_mq5_create(void);

/**
 * @brief Request one MQ5 sample from CAN_TX after the task has been created.
 */
void task_mq5_request(void);

#endif
