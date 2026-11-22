/**
 * @file task_alarm.c
 * @brief CAN sensor and threshold consumption and alarm state publication.
 */

#include "task_alarm.h"

#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"
#include "app_ipc.h"
#include "app_time.h"
#include "bsp_board.h"
#include "can_protocol.h"
#include "hal_can.h"
#include "sensor_status.h"

static TaskHandle_t xAlarmTaskHandle = NULL;

static SensorData_t latest_sensor = {0};
static bool has_sample = false;
static bool alarm_active = false;
static AlertThresholds_t active_thresholds = {
    .gas_limit_raw = ALERT_GAS_DEFAULT_RAW,
    .temperature_limit_c = ALERT_TEMPERATURE_DEFAULT_C,
};

/**
 * @brief Match standard CAN data frames by identifier and payload length.
 * @param[in] frame Non-NULL input CAN frame.
 * @param[in] std_id Expected standard CAN identifier.
 * @param[in] dlc Expected payload length in bytes.
 * @return true if IDE, RTR, identifier and DLC match; otherwise false.
 */
static bool is_data_frame(const CanRxFrame_t *frame, uint32_t std_id, uint8_t dlc) {
    return !frame->is_extended && !frame->is_remote && (frame->std_id == std_id) &&
           (frame->dlc == dlc);
}

/**
 * @brief Decode and range-check threshold data before overwriting the ISR threshold queue.
 * @param[in] frame Input frame whose metadata was validated by on_can_frame.
 * @param[in,out] task_woken Shared FreeRTOS ISR wake flag.
 */
static void on_threshold_frame(const CanRxFrame_t *frame, BaseType_t *task_woken) {
    AlertThresholds_t thresholds;

    can_unpack_thresholds(frame->data, &thresholds.gas_limit_raw,
                          &thresholds.temperature_limit_c);
    if (alert_thresholds_valid(&thresholds) &&
        xQueueOverwriteFromISR(xThresholdQueue, &thresholds, task_woken) == pdPASS) {
        vTaskNotifyGiveFromISR(xAlarmTaskHandle, task_woken);
    }
}

/**
 * @brief Validate sensor faults, timestamp arrival and overwrite the latest-sample queue from the ISR.
 * @param[in] frame Input frame whose metadata was validated by on_can_frame.
 * @param[in,out] task_woken Shared FreeRTOS ISR wake flag.
 */
static void on_sensor_frame(const CanRxFrame_t *frame, BaseType_t *task_woken) {
    SensorData_t sample = {0};

    can_unpack_sensor(frame->data, &sample.temperature_c, &sample.humidity_pct, &sample.gas_raw,
                      &sample.sensor_faults);
    if (!sensor_faults_valid(sample.sensor_faults)) {
        return;
    }
    /* Stamped on arrival so freshness does not depend on when the task reads the queue. */
    sample.received_ms = app_time_ms_from_isr();
    if (xQueueOverwriteFromISR(xSensorQueue, &sample, task_woken) == pdPASS) {
        vTaskNotifyGiveFromISR(xAlarmTaskHandle, task_woken);
    }
}

/**
 * @brief Validate and decode a received CAN frame and post its data to the application queues from the ISR.
 * @param[in] frame Input received frame; its data is copied before the callback returns.
 * @param[in,out] task_woken Input/output FreeRTOS wake flag shared by ISR queue operations.
 */
static void on_can_frame(const CanRxFrame_t *frame, BaseType_t *task_woken) {
    if (is_data_frame(frame, CAN_ID_THRESHOLDS, CAN_DLC_THRESHOLDS)) {
        on_threshold_frame(frame, task_woken);
    } else if (is_data_frame(frame, CAN_ID_SENSOR, CAN_DLC_SENSOR)) {
        on_sensor_frame(frame, task_woken);
    }
}

/**
 * @brief Start CAN reception or enter the non-returning board failure handler on error.
 */
static void start_can(void) {
    CanError_t error = hal_can_start();
    if (error != CAN_ERR_NONE) {
        bsp_board_fail(bsp_board_can_error(error));
    }
}

/**
 * @brief Consume the latest CAN data and publish the updated call decision with the reception time.
 */
static void update_alarm(void) {
    SensorData_t sample;
    AlertThresholds_t thresholds;

    if (xQueueReceive(xThresholdQueue, &thresholds, 0U) == pdPASS) {
        active_thresholds = thresholds;
    }

    if (xQueueReceive(xSensorQueue, &sample, 0U) == pdPASS) {
        latest_sensor = sample;
        has_sample = true;
    }

    AlarmState_t state = {.received_ms = latest_sensor.received_ms};
    if (has_sample && ((app_time_ms() - latest_sensor.received_ms) <= SENSOR_FRESH_TIMEOUT_MS)) {
        alarm_active = sensor_alarm_should_run(alarm_active, latest_sensor.gas_raw,
                                               latest_sensor.temperature_c,
                                               latest_sensor.sensor_faults, &active_thresholds);
        state.wants_call = alarm_active;
    } else {
        /* Expire the sample without resetting the hysteresis state. */
        has_sample = false;
    }
    (void)xQueueOverwrite(xAlarmQueue, &state);
}

/**
 * @brief Start CAN and evaluate the alarm on arrivals and periodic freshness checks.
 * @param[in] pvParameters Unused task argument; task creation passes NULL.
 */
static void prvAlarmTask(void *pvParameters) {
    (void)pvParameters;

    start_can();
    for (;;) {
        update_alarm();
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TASK_ALARM_POLL_MS));
    }
}

/**
 * @brief Register the CAN receive callback and create the ALARM task.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_alarm_create(void) {
    hal_can_set_rx_handler(on_can_frame);
    return xTaskCreate(prvAlarmTask, "ALARM", TASK_ALARM_STACK_WORDS, NULL, TASK_ALARM_PRIO,
                       &xAlarmTaskHandle);
}
