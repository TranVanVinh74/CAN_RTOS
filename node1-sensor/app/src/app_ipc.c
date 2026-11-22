/**
 * @file app_ipc.c
 * @brief Application queue handles, message types and queue allocation.
 */

#include "app_ipc.h"

#include "app_cfg.h"

QueueHandle_t xDht11Queue = NULL;
QueueHandle_t xMq5Queue = NULL;

/**
 * @brief Create the FreeRTOS queues used to exchange application data.
 * @return true if all required queues were created; false on allocation failure.
 */
bool app_ipc_init(void) {
    xDht11Queue = xQueueCreate(SENSOR_RESULT_QUEUE_LENGTH, sizeof(Dht11Sample_t));
    xMq5Queue = xQueueCreate(SENSOR_RESULT_QUEUE_LENGTH, sizeof(Mq5Sample_t));
    return (xDht11Queue != NULL) && (xMq5Queue != NULL);
}
