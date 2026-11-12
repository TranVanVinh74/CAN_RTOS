#include "app_ipc.h"

#include "app_cfg.h"

QueueHandle_t xSensorQueue = NULL;

bool app_ipc_init(void) {
    xSensorQueue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorData_t));
    return xSensorQueue != NULL;
}
