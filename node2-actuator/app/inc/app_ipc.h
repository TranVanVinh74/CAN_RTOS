/**
 * @file app_ipc.h
 * @brief Application queue handles, message types and queue allocation.
 */

#ifndef APP_IPC_H
#define APP_IPC_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    uint8_t fan_state;
    uint8_t temperature;
    uint8_t humidity;
    uint16_t gas_raw;
    uint8_t sensor_faults;
} ActuatorCommand_t;

extern QueueHandle_t xActuatorQueue;

/**
 * @brief Create the FreeRTOS queues used to exchange application data.
 * @return true if all required queues were created; false on allocation failure.
 */
bool app_ipc_init(void);

#endif
