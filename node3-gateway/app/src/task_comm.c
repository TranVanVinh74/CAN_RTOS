/**
 * @file task_comm.c
 * @brief CAN sensor and threshold consumption with cellular alarm coordination.
 */

#include "task_comm.h"

#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "app_alert_phone.h"
#include "app_call.h"
#include "app_cfg.h"
#include "app_ipc.h"
#include "app_time.h"
#include "bsp_board.h"
#include "can_protocol.h"
#include "hal_can.h"
#include "sensor_status.h"

static TaskHandle_t xCommTaskHandle = NULL;

static SensorData_t latest_sensor = {0};
static bool has_sample = false;
static bool sensor_fresh = false;
static bool alarm_active = false;
static AlertThresholds_t active_thresholds = {
    .gas_limit_raw = ALERT_GAS_DEFAULT_RAW,
    .temperature_limit_c = ALERT_TEMPERATURE_DEFAULT_C,
};
static uint32_t modem_wait_start_ms = 0;
static uint32_t last_at_check_ms = 0;
static uint32_t last_idle_check_ms = 0;
static bool at_checked = false;

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
    if (alert_thresholds_valid(&thresholds)) {
        (void)xQueueOverwriteFromISR(xThresholdQueue, &thresholds, task_woken);
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
    (void)xQueueOverwriteFromISR(xSensorQueue, &sample, task_woken);
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
 * @brief Consume the latest CAN data, expire stale samples and recompute the alarm using valid channels.
 */
static void update_sensor_state(void) {
    SensorData_t sample;
    AlertThresholds_t thresholds;

    if (xQueueReceive(xThresholdQueue, &thresholds, 0U) == pdPASS) {
        active_thresholds = thresholds;
    }

    if (xQueueReceive(xSensorQueue, &sample, 0U) == pdPASS) {
        latest_sensor = sample;
        has_sample = true;
    }

    sensor_fresh = has_sample &&
                   ((app_time_ms() - latest_sensor.received_ms) <= SENSOR_FRESH_TIMEOUT_MS);
    /*
     * Drop a stale sample: after about 49.7 days without data the millisecond counter
     * wraps and the old sample would look fresh again.
     */
    if (!sensor_fresh) {
        has_sample = false;
        return;
    }

    alarm_active = sensor_alarm_should_run(alarm_active, latest_sensor.gas_raw,
                                           latest_sensor.temperature_c,
                                           latest_sensor.sensor_faults, &active_thresholds);
}

/**
 * @brief Require both a fresh sensor sample and an active alarm before automatic dialing.
 * @return true if current alarm data permits an automatic call.
 */
static bool alarm_wants_call(void) {
    return sensor_fresh && alarm_active;
}

/**
 * @brief Restart the modem boot-wait timer and clear AT-probe scheduling state.
 */
static void restart_modem_wait(void) {
    modem_wait_start_ms = app_time_ms();
    at_checked = false;
}

/**
 * @brief Probe the modem, update probe timestamps and refresh sensor state after the blocking check.
 */
static void check_modem(void) {
    at_checked = true;
    last_at_check_ms = app_time_ms();
    (void)app_call_check_modem();
    last_idle_check_ms = app_time_ms();
    update_sensor_state();
}

/**
 * @brief Consume CAN data and service modem readiness, automatic alarm calls and call cooldowns.
 * @param[in] pvParameters Unused task argument; task creation passes NULL.
 */
static void prvCommTask(void *pvParameters) {
    (void)pvParameters;

    start_can();
    app_call_init();
    restart_modem_wait();

    for (;;) {
        update_sensor_state();
        uint32_t now = app_time_ms();

        if (app_call_is_active()) {
            if (!alarm_wants_call()) {
                app_call_request_hangup();
            }
            app_call_poll_modem();
            app_call_service();
            if (app_call_stage() == CALL_STAGE_MODEM_RESET) {
                restart_modem_wait();
            }
            vTaskDelay(pdMS_TO_TICKS(TASK_COMM_IDLE_DELAY_MS));
            continue;
        }

        if ((now - modem_wait_start_ms) < MODEM_BOOT_WAIT_MS) {
            vTaskDelay(pdMS_TO_TICKS(TASK_COMM_WAIT_DELAY_MS));
            continue;
        }

        if (!app_call_modem_ready()) {
            if (!at_checked || ((now - last_at_check_ms) >= MODEM_AT_RETRY_MS)) {
                check_modem();
            }
            vTaskDelay(pdMS_TO_TICKS(TASK_COMM_WAIT_DELAY_MS));
            continue;
        }

        app_call_poll_modem();
        if (!app_call_modem_ready()) {
            restart_modem_wait();
            vTaskDelay(pdMS_TO_TICKS(TASK_COMM_WAIT_DELAY_MS));
            continue;
        }

        update_sensor_state();

        /* An idle modem can reboot without notice; re-check it before an alarm needs it. */
        if (!alarm_wants_call() &&
            ((app_time_ms() - last_idle_check_ms) >= MODEM_IDLE_CHECK_MS)) {
            check_modem();
        }

        if (app_call_modem_ready() && alarm_wants_call() &&
            app_call_cooldown_over(app_time_ms())) {
            app_call_start(APP_ALERT_PHONE);
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_COMM_IDLE_DELAY_MS));
    }
}

/**
 * @brief Register the CAN receive callback and create the COMM task.
 * @return pdPASS on successful task creation; otherwise the FreeRTOS allocation error.
 */
BaseType_t task_comm_create(void) {
    hal_can_set_rx_handler(on_can_frame);
    return xTaskCreate(prvCommTask, "COMM", TASK_COMM_STACK_WORDS, NULL, TASK_COMM_PRIO,
                       &xCommTaskHandle);
}
