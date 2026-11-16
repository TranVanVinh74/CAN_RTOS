#include "task_sensor.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"
#include "app_ipc.h"
#include "hal_dht11.h"
#include "hal_mq5.h"
#include "sensor_status.h"

static TaskHandle_t xSensorTaskHandle = NULL;

static void prvSensorTask(void *pvParameters) {
    (void)pvParameters;

    SensorData_t data = {0};
    Dht11Reading_t dht = {0};
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        data.sensor_faults = 0U;
        if (!hal_mq5_read_average(TASK_SENSOR_MQ5_SAMPLES, &data.gas_raw)) {
            data.sensor_faults |= SENSOR_FAULT_GAS;
        }
        if (hal_dht11_read(&dht)) {
            data.temperature = dht.temperature_c;
            data.humidity = dht.humidity_pct;
        } else {
            data.sensor_faults |= SENSOR_FAULT_DHT;
        }

        (void)xQueueSend(xSensorQueue, &data, pdMS_TO_TICKS(TASK_SENSOR_QUEUE_SEND_TIMEOUT_MS));

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_SENSOR_PERIOD_MS));
    }
}

BaseType_t task_sensor_create(void) {
    return xTaskCreate(prvSensorTask, "SENSOR", TASK_SENSOR_STACK_WORDS, NULL, TASK_SENSOR_PRIO,
                       &xSensorTaskHandle);
}
