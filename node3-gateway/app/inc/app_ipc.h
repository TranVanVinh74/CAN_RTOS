#ifndef APP_IPC_H
#define APP_IPC_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "alert_thresholds.h"

typedef struct {
    uint8_t temperature;
    uint8_t humidity;
    uint16_t gas_raw;
    uint32_t received_ms;
} SensorData_t;

extern QueueHandle_t xSensorQueue;
extern QueueHandle_t xThresholdQueue;

bool app_ipc_init(void);

#endif
