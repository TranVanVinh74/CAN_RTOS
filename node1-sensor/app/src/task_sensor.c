#include "task_sensor.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"
#include "app_ipc.h"
#include "hal_dht11.h"
#include "hal_mq5.h"

volatile SensorData_t g_sensorDebug = {0};
volatile uint32_t g_sensorQueueDropCount = 0;

static TaskHandle_t xSensorTaskHandle = NULL;

static void prvSensorTask(void *pvParameters) {
    (void)pvParameters;

    SensorData_t data = {0};
    Dht11Reading_t dht = {0};
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        data.gas_raw = hal_mq5_read_average(TASK_SENSOR_MQ5_SAMPLES);

        if (hal_dht11_read(&dht)) {
            data.temperature = dht.temperature_c;
            data.humidity = dht.humidity_pct;
        }

        g_sensorDebug = data;

        if (xQueueSend(xSensorQueue, &data, pdMS_TO_TICKS(TASK_SENSOR_QUEUE_SEND_TIMEOUT_MS)) !=
            pdPASS) {
            g_sensorQueueDropCount++;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_SENSOR_PERIOD_MS));
    }
}

BaseType_t task_sensor_create(void) {
    return xTaskCreate(prvSensorTask, "SENSOR", TASK_SENSOR_STACK_WORDS, NULL, TASK_SENSOR_PRIO,
                       &xSensorTaskHandle);
}
