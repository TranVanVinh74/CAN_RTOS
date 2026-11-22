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
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        Mq5Sample_t sample;
        sample.valid = hal_mq5_read_average(TASK_MQ5_SAMPLES, &sample.gas_raw);
        (void)xQueueSend(xMq5Queue, &sample, portMAX_DELAY);
    }
}

BaseType_t task_mq5_create(void) {
    return xTaskCreate(prvMq5Task, "MQ5", TASK_MQ5_STACK_WORDS, NULL, TASK_MQ5_PRIO,
                       &xMq5TaskHandle);
}

void task_mq5_request(void) {
    (void)xTaskNotifyGive(xMq5TaskHandle);
}
