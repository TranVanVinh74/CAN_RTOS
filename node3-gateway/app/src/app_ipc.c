#include "app_ipc.h"

#include "app_cfg.h"

QueueHandle_t xSensorQueue = NULL;
QueueHandle_t xThresholdQueue = NULL;

bool app_ipc_init(void) {
    xSensorQueue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorData_t));
    xThresholdQueue = xQueueCreate(THRESHOLD_QUEUE_LENGTH, sizeof(AlertThresholds_t));
    return (xSensorQueue != NULL) && (xThresholdQueue != NULL);
}
