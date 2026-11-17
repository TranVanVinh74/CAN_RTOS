/**
 * @file task_can_tx.h
 * @brief Queue-driven packing and transmission of sensor CAN frames.
 */

#ifndef TASK_CAN_TX_H
#define TASK_CAN_TX_H

#include "FreeRTOS.h"

/**
 * @brief Create the CAN_TX task that consumes sensor samples and queues CAN frames.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_can_tx_create(void);

#endif
