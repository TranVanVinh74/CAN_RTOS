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

#include "alert_thresholds.h"

typedef struct {
    uint8_t temperature_c;
    uint8_t humidity_pct;
    uint16_t gas_raw;
    uint8_t sensor_faults;
    uint32_t received_ms;
} SensorData_t;

typedef struct {
    uint32_t received_ms;
    bool wants_call;
} AlarmState_t;

extern QueueHandle_t xSensorQueue;
extern QueueHandle_t xThresholdQueue;
extern QueueHandle_t xAlarmQueue;

/**
 * @brief Create the FreeRTOS queues used to exchange application data.
 * @return true if all required queues were created; false on allocation failure.
 */
bool app_ipc_init(void);

#endif
