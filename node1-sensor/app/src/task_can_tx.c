#include "task_can_tx.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"
#include "app_ipc.h"
#include "bsp_board.h"
#include "can_protocol.h"
#include "hal_can.h"

static TaskHandle_t xCanTxTaskHandle = NULL;

static void start_can(void) {
    CanError_t error = hal_can_start();
    if (error != CAN_ERR_NONE) {
        bsp_board_fail(bsp_board_can_error(error));
    }
}

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

BaseType_t task_can_tx_create(void) {
    return xTaskCreate(prvCanTxTask, "CAN_TX", TASK_CAN_TX_STACK_WORDS, NULL, TASK_CAN_TX_PRIO,
                       &xCanTxTaskHandle);
}
