/**
 * @file app_ipc.c
 * @brief Application queue handles, message types and queue allocation.
 */

#include "app_ipc.h"

#include "app_cfg.h"

QueueHandle_t xActuatorQueue = NULL;

/**
 * @brief Create the FreeRTOS queues used to exchange application data.
 * @return true if all required queues were created; false on allocation failure.
 */
bool app_ipc_init(void) {
    xActuatorQueue = xQueueCreate(ACTUATOR_QUEUE_LENGTH, sizeof(ActuatorCommand_t));
    return xActuatorQueue != NULL;
}
