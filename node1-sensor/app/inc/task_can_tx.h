/**
 * @file task_can_tx.h
 * @brief Periodic coordination of sensor reads and CAN transmission.
 */

#ifndef TASK_CAN_TX_H
#define TASK_CAN_TX_H

#include "FreeRTOS.h"

/**
 * @brief Create the CAN_TX task that requests paired sensor samples and transmits them.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_can_tx_create(void);

#endif
