#ifndef APP_IPC_H
#define APP_IPC_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    uint8_t temperature;
    uint8_t humidity;
    uint16_t gas_raw;
    uint8_t sensor_faults;
} SensorData_t;

extern QueueHandle_t xSensorQueue;

bool app_ipc_init(void);

#endif
