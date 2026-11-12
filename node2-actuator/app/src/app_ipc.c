#include "app_ipc.h"

#include "app_cfg.h"

QueueHandle_t xActuatorQueue = NULL;

bool app_ipc_init(void) {
    xActuatorQueue = xQueueCreate(ACTUATOR_QUEUE_LENGTH, sizeof(ActuatorCommand_t));
    return xActuatorQueue != NULL;
}
