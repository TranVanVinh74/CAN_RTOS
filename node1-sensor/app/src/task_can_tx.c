/**
 * @file task_can_tx.c
 * @brief Queue-driven packing and transmission of sensor CAN frames.
 */

#include "task_can_tx.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"
#include "app_ipc.h"
#include "bsp_board.h"
#include "can_protocol.h"
#include "hal_can.h"

static TaskHandle_t xCanTxTaskHandle = NULL;

/**
 * @brief Start CAN transmission or stop through the board failure handler if startup fails.
 */
static void start_can(void) {
    CanError_t error = hal_can_start();
    if (error != CAN_ERR_NONE) {
        bsp_board_fail(bsp_board_can_error(error));
    }
}

/**
 * @brief Wait for queued sensor readings, pack five payload bytes and request CAN transmission.
 * @param[in] pvParameters Unused task argument; task creation passes NULL.
 */
static void prvCanTxTask(void *pvParameters) {
    (void)pvParameters;

    start_can();

    for (;;) {
        SensorData_t data;
        uint8_t payload[CAN_DLC_SENSOR];

        if (xQueueReceive(xSensorQueue, &data, pdMS_TO_TICKS(TASK_CAN_TX_QUEUE_WAIT_MS)) !=
            pdPASS) {
            continue;
        }

        can_pack_sensor(data.temperature_c, data.humidity_pct, data.gas_raw, data.sensor_faults,
                        payload);
        /* Busy TX mailboxes drop the frame; the next sample replaces it one period later. */
        (void)hal_can_send(CAN_ID_SENSOR, payload, CAN_DLC_SENSOR);
    }
}

/**
 * @brief Create the CAN_TX task that consumes sensor samples and queues CAN frames.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_can_tx_create(void) {
    return xTaskCreate(prvCanTxTask, "CAN_TX", TASK_CAN_TX_STACK_WORDS, NULL, TASK_CAN_TX_PRIO,
                       &xCanTxTaskHandle);
}
