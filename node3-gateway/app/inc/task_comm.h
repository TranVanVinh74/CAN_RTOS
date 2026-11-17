/**
 * @file task_comm.h
 * @brief CAN sensor and threshold consumption with cellular alarm coordination.
 */

#ifndef TASK_COMM_H
#define TASK_COMM_H

#include "FreeRTOS.h"

/**
 * @brief Register the CAN receive callback and create the COMM task.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_comm_create(void);

#endif
