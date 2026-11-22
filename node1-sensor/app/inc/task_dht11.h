/**
 * @file task_dht11.h
 * @brief DHT11 sampling task.
 */

#ifndef TASK_DHT11_H
#define TASK_DHT11_H

#include "FreeRTOS.h"

/**
 * @brief Create the DHT11 task that services sampling requests.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_dht11_create(void);

/**
 * @brief Request one DHT11 sample from CAN_TX after the task has been created.
 */
void task_dht11_request(void);

#endif
