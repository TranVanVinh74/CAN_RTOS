#include "task_comm.h"

#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "app_alert_phone.h"
#include "app_call.h"
#include "app_cfg.h"
#include "app_ipc.h"
#include "bsp_board.h"
#include "can_protocol.h"
#include "hal_can.h"
#include "sensor_status.h"

static TaskHandle_t xCommTaskHandle = NULL;

static SensorData_t latest_sensor = {0};
static bool sensor_seen = false;
static bool sensor_fresh = false;
static bool alarm_active = false;
static AlertThresholds_t active_thresholds = {
    .gas_limit_raw = ALERT_GAS_DEFAULT_RAW,
    .temperature_limit_c = ALERT_TEMPERATURE_DEFAULT_C,
};

static uint32_t now_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static bool is_data_frame(const CanRxFrame_t *frame, uint32_t std_id, uint8_t dlc) {
    return !frame->is_extended && !frame->is_remote && (frame->std_id == std_id) &&
           (frame->dlc == dlc);
}

static void on_threshold_frame(const CanRxFrame_t *frame, BaseType_t *task_woken) {
    AlertThresholds_t thresholds;

    can_unpack_thresholds(frame->data, &thresholds.gas_limit_raw,
                          &thresholds.temperature_limit_c);
    if (alert_thresholds_valid(&thresholds)) {
        (void)xQueueOverwriteFromISR(xThresholdQueue, &thresholds, task_woken);
    }
}

static void on_sensor_frame(const CanRxFrame_t *frame, BaseType_t *task_woken) {
    SensorData_t sample = {0};

    can_unpack_sensor(frame->data, &sample.temperature, &sample.humidity, &sample.gas_raw,
                      &sample.sensor_faults);
    if (!sensor_faults_valid(sample.sensor_faults)) {
        return;
    }
    sample.received_ms = (uint32_t)(xTaskGetTickCountFromISR() * portTICK_PERIOD_MS);
    (void)xQueueOverwriteFromISR(xSensorQueue, &sample, task_woken);
}

static void on_can_frame(const CanRxFrame_t *frame, BaseType_t *task_woken) {
    if (is_data_frame(frame, CAN_ID_THRESHOLDS, CAN_DLC_THRESHOLDS)) {
        on_threshold_frame(frame, task_woken);
    } else if (is_data_frame(frame, CAN_ID_SENSOR, CAN_DLC_SENSOR)) {
        on_sensor_frame(frame, task_woken);
    }
}

static void start_can(void) {
    static const BoardError_t board_error[] = {
        [CAN_ERR_NONE] = BOARD_ERR_NONE,
        [CAN_ERR_NO_HANDLE] = BOARD_ERR_CAN_START,
        [CAN_ERR_FILTER] = BOARD_ERR_CAN_FILTER,
        [CAN_ERR_START] = BOARD_ERR_CAN_START,
        [CAN_ERR_NOTIFICATION] = BOARD_ERR_CAN_NOTIFICATION,
    };

    CanError_t error = hal_can_start();
    if (error != CAN_ERR_NONE) {
        bsp_board_fail(board_error[error]);
    }
}

static void update_sensor_state(void) {
    SensorData_t sample;
    AlertThresholds_t thresholds;

    if (xQueueReceive(xThresholdQueue, &thresholds, 0U) == pdPASS) {
        active_thresholds = thresholds;
    }

    if (xQueueReceive(xSensorQueue, &sample, 0U) == pdPASS) {
        latest_sensor = sample;
        sensor_seen = true;
    }

    sensor_fresh = sensor_seen &&
                   ((now_ms() - latest_sensor.received_ms) <= SENSOR_FRESH_TIMEOUT_MS);
    if (sensor_fresh) {
        alarm_active = sensor_alarm_should_run(alarm_active, latest_sensor.gas_raw,
                                               latest_sensor.temperature,
                                               latest_sensor.sensor_faults, &active_thresholds);
    }
}

static bool alarm_wants_call(void) {
    return sensor_fresh && alarm_active;
}

static void prvCommTask(void *pvParameters) {
    (void)pvParameters;

    uint32_t last_at_check_ms = 0;
    uint32_t last_idle_check_ms = 0;
    bool at_checked = false;

    start_can();
    app_call_init();
    uint32_t modem_wait_start = now_ms();

    for (;;) {
        update_sensor_state();
        uint32_t now = now_ms();

        if (app_call_is_active()) {
            if (!alarm_wants_call()) {
                app_call_request_hangup();
            }
            app_call_poll_modem();
            app_call_service();
            if (app_call_stage() == CALL_STAGE_MODEM_RESET) {
                modem_wait_start = now_ms();
                at_checked = false;
            }
            vTaskDelay(pdMS_TO_TICKS(TASK_COMM_IDLE_DELAY_MS));
            continue;
        }

        if ((now - modem_wait_start) < MODEM_BOOT_WAIT_MS) {
            vTaskDelay(pdMS_TO_TICKS(TASK_COMM_WAIT_DELAY_MS));
            continue;
        }

        if (!app_call_modem_ready()) {
            if (!at_checked || ((now - last_at_check_ms) >= MODEM_AT_RETRY_MS)) {
                at_checked = true;
                last_at_check_ms = now;
                (void)app_call_check_modem();
                last_idle_check_ms = now_ms();
                update_sensor_state();
            }
            vTaskDelay(pdMS_TO_TICKS(TASK_COMM_WAIT_DELAY_MS));
            continue;
        }

        app_call_poll_modem();
        if (!app_call_modem_ready()) {
            modem_wait_start = now_ms();
            at_checked = false;
            vTaskDelay(pdMS_TO_TICKS(TASK_COMM_WAIT_DELAY_MS));
            continue;
        }

        update_sensor_state();

        if (!alarm_wants_call() && ((now_ms() - last_idle_check_ms) >= MODEM_IDLE_CHECK_MS)) {
            at_checked = true;
            last_at_check_ms = now_ms();
            (void)app_call_check_modem();
            last_idle_check_ms = now_ms();
            update_sensor_state();
        }

        if (app_call_modem_ready() && alarm_wants_call() && app_call_cooldown_over(now_ms())) {
            app_call_start(APP_ALERT_PHONE);
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_COMM_IDLE_DELAY_MS));
    }
}

BaseType_t task_comm_create(void) {
    hal_can_set_rx_handler(on_can_frame);
    return xTaskCreate(prvCommTask, "COMM", TASK_COMM_STACK_WORDS, NULL, TASK_COMM_PRIO,
                       &xCommTaskHandle);
}
