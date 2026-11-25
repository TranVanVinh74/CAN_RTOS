/**
 * @file task_can_tx.c
 * @brief Periodic coordination of sensor reads and CAN transmission.
 */

#include "task_can_tx.h"

#include "task.h"

#include "app_cfg.h"
#include "app_ipc.h"
#include "bsp_board.h"
#include "can_protocol.h"
#include "hal_can.h"
#include "sensor_status.h"
#include "task_dht11.h"
#include "task_mq5.h"

/**
 * @brief Start CAN transmission or stop through the board failure handler if startup fails.
 */
static void start_can(void) {
    CanError_t error = hal_can_start();
    if (error != CAN_ERR_NONE) {
        bsp_board_fail(bsp_board_can_error(error));
    }
}

/* Samples for an earlier request_id arrived late and are discarded until this one or timeout. */
static bool receive_mq5(uint32_t request_id, Mq5Sample_t *sample) {
    TimeOut_t timeout;
    TickType_t remaining = pdMS_TO_TICKS(SENSOR_RESULT_TIMEOUT_MS);

    vTaskSetTimeOutState(&timeout);
    while (xQueueReceive(xMq5Queue, sample, remaining) == pdPASS) {
        if (sample->request_id == request_id) {
            return true;
        }
        if (xTaskCheckForTimeOut(&timeout, &remaining) != pdFALSE) {
            break;
        }
    }
    return false;
}

static bool receive_dht11(uint32_t request_id, Dht11Sample_t *sample) {
    TimeOut_t timeout;
    TickType_t remaining = pdMS_TO_TICKS(SENSOR_RESULT_TIMEOUT_MS);

    vTaskSetTimeOutState(&timeout);
    while (xQueueReceive(xDht11Queue, sample, remaining) == pdPASS) {
        if (sample->request_id == request_id) {
            return true;
        }
        if (xTaskCheckForTimeOut(&timeout, &remaining) != pdFALSE) {
            break;
        }
    }
    return false;
}

/**
 * @brief Request both samples once per cycle, then pack and transmit the paired results.
 * @param[in] pvParameters Unused task argument; task creation passes NULL.
 */
static void prvCanTxTask(void *pvParameters) {
    (void)pvParameters;

    start_can();
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t request_id = 0U;

    for (;;) {
        Mq5Sample_t mq5;
        Dht11Sample_t dht11;
        uint8_t payload[CAN_DLC_SENSOR];

        request_id++;
        task_mq5_request(request_id);
        task_dht11_request(request_id);
        /* A stuck sensor task is reported as a channel fault instead of stopping 0x100. */
        if (!receive_mq5(request_id, &mq5)) {
            mq5 = (Mq5Sample_t){0};
        }
        if (!receive_dht11(request_id, &dht11)) {
            dht11 = (Dht11Sample_t){0};
        }

        uint8_t faults = (mq5.valid ? 0U : SENSOR_FAULT_GAS) |
                         (dht11.valid ? 0U : SENSOR_FAULT_DHT);
        can_pack_sensor(dht11.temperature_c, dht11.humidity_pct, mq5.gas_raw, faults, payload);
        /* Busy TX mailboxes drop the frame; the next sample replaces it one period later. */
        (void)hal_can_send(CAN_ID_SENSOR, payload, CAN_DLC_SENSOR);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

/**
 * @brief Create the CAN_TX task that requests paired sensor samples and transmits them.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_can_tx_create(void) {
    return xTaskCreate(prvCanTxTask, "CAN_TX", TASK_CAN_TX_STACK_WORDS, NULL, TASK_CAN_TX_PRIO,
                       NULL);
}
