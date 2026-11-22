/**
 * @file task_dht11.c
 * @brief DHT11 sampling and fault reporting for each requested cycle.
 */

#include "task_dht11.h"

#include "task.h"

#include "app_cfg.h"
#include "app_ipc.h"
#include "hal_dht11.h"

static TaskHandle_t xDht11TaskHandle = NULL;

/**
 * @brief Read DHT11 on request and report the retained reading and sample validity.
 * @param[in] pvParameters Unused task argument; task creation passes NULL.
 */
static void prvDht11Task(void *pvParameters) {
    (void)pvParameters;

    Dht11Reading_t reading = {0};

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        bool valid = hal_dht11_read(&reading);
        Dht11Sample_t sample = {
            .temperature_c = reading.temperature_c,
            .humidity_pct = reading.humidity_pct,
            .valid = valid,
        };
        (void)xQueueSend(xDht11Queue, &sample, portMAX_DELAY);
    }
}

BaseType_t task_dht11_create(void) {
    return xTaskCreate(prvDht11Task, "DHT11", TASK_DHT11_STACK_WORDS, NULL, TASK_DHT11_PRIO,
                       &xDht11TaskHandle);
}

void task_dht11_request(void) {
    (void)xTaskNotifyGive(xDht11TaskHandle);
}
