/**
 * @file task_mq5.c
 * @brief MQ5 ADC averaging and fault reporting for each requested cycle.
 */

#include "task_mq5.h"

#include "task.h"

#include "app_cfg.h"
#include "app_ipc.h"
#include "hal_mq5.h"

static TaskHandle_t xMq5TaskHandle = NULL;

/**
 * @brief Sample MQ5 on request and report the gas reading and sample validity.
 * @param[in] pvParameters Unused task argument.
 */
static void prvMq5Task(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        uint32_t request_id;
        if (xTaskNotifyWait(0U, 0U, &request_id, pdMS_TO_TICKS(SENSOR_REQUEST_WAIT_MS)) != pdTRUE) {
            continue;
        }
        Mq5Sample_t sample = {.request_id = request_id};
        sample.valid = hal_mq5_read_average(TASK_MQ5_SAMPLES, &sample.gas_raw);
        /* A full queue means CAN_TX gave up on this cycle; the sample is dropped. */
        (void)xQueueSend(xMq5Queue, &sample, pdMS_TO_TICKS(SENSOR_RESULT_TIMEOUT_MS));
    }
}

BaseType_t task_mq5_create(void) {
    return xTaskCreate(prvMq5Task, "MQ5", TASK_MQ5_STACK_WORDS, NULL, TASK_MQ5_PRIO,
                       &xMq5TaskHandle);
}

void task_mq5_request(uint32_t request_id) {
    (void)xTaskNotify(xMq5TaskHandle, request_id, eSetValueWithOverwrite);
}
