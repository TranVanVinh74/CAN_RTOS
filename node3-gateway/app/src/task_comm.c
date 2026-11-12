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
#include "hal_a7680c.h"
#include "hal_can.h"

#define CAN_STAGE_STARTING 1U
#define CAN_STAGE_RUNNING 2U

volatile SensorData_t g_node3Debug = {0};
volatile uint32_t g_canStage = 0;
volatile uint32_t g_canRxCount = 0;
volatile uint32_t g_canProcessedCount = 0;
volatile uint32_t g_canInvalidCount = 0;
volatile uint32_t g_canQueueErrorCount = 0;
volatile uint32_t g_sensorSeen = 0;
volatile uint32_t g_sensorFresh = 0;
volatile uint32_t g_sensorAgeMs = 0;
volatile uint32_t g_gasAlarm = 0;
volatile uint32_t g_taskLoops = 0;
volatile uint32_t g_atFailCount = 0;

static TaskHandle_t xCommTaskHandle = NULL;

static SensorData_t latest_sensor = {0};
static bool sensor_seen = false;
static bool gas_alarm = false;

static uint32_t now_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static bool is_sensor_frame(const CanRxFrame_t *frame) {
    return !frame->is_extended && !frame->is_remote && (frame->std_id == CAN_ID_SENSOR) &&
           (frame->dlc == CAN_DLC_SENSOR);
}

static void on_can_frame(const CanRxFrame_t *frame, BaseType_t *task_woken) {
    SensorData_t sample = {0};

    if (!is_sensor_frame(frame)) {
        g_canInvalidCount++;
        return;
    }

    can_unpack_sensor(frame->data, &sample.temperature, &sample.humidity, &sample.gas_raw);
    sample.received_ms = (uint32_t)(xTaskGetTickCountFromISR() * portTICK_PERIOD_MS);
    g_canRxCount++;

    if (xQueueOverwriteFromISR(xSensorQueue, &sample, task_woken) != pdPASS) {
        g_canQueueErrorCount++;
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

    g_canStage = CAN_STAGE_STARTING;

    CanError_t error = hal_can_start();
    if (error != CAN_ERR_NONE) {
        bsp_board_fail(board_error[error]);
    }

    g_canStage = CAN_STAGE_RUNNING;
}

static void update_alarm(void) {
    if (latest_sensor.gas_raw > GAS_ALERT_ON_THRESHOLD) {
        gas_alarm = true;
    } else if (latest_sensor.gas_raw < GAS_ALERT_OFF_THRESHOLD) {
        gas_alarm = false;
    }
}

static void sensor_update(void) {
    SensorData_t sample;

    if (xQueueReceive(xSensorQueue, &sample, 0) == pdPASS) {
        latest_sensor = sample;
        sensor_seen = true;
        g_node3Debug = sample;
        g_canProcessedCount++;
    }

    g_sensorSeen = sensor_seen ? 1U : 0U;

    if (!sensor_seen) {
        g_sensorFresh = 0;
        g_sensorAgeMs = 0;
        g_gasAlarm = 0;
        return;
    }

    g_sensorAgeMs = now_ms() - latest_sensor.received_ms;
    g_sensorFresh = (g_sensorAgeMs <= SENSOR_FRESH_TIMEOUT_MS) ? 1U : 0U;

    if (g_sensorFresh != 0U) {
        update_alarm();
    }

    g_gasAlarm = gas_alarm ? 1U : 0U;
}

static bool check_modem(uint32_t now, uint32_t *last_check_ms, bool *checked) {
    if (*checked && ((now - *last_check_ms) < MODEM_AT_RETRY_MS)) {
        return false;
    }

    *checked = true;
    *last_check_ms = now;
    g_callStage = CALL_STAGE_AT_CHECK;

    hal_a7680c_drain();

    if (hal_a7680c_is_ready()) {
        g_atOK = 1;
        g_callStage = CALL_STAGE_READY;
    } else {
        g_atFailCount++;
    }
    return true;
}

static bool alarm_wants_call(void) {
    return (g_sensorFresh != 0U) && gas_alarm && (latest_sensor.gas_raw > GAS_ALERT_ON_THRESHOLD);
}

static void prvCommTask(void *pvParameters) {
    (void)pvParameters;

    uint32_t last_at_check_ms = 0;
    uint32_t last_idle_check_ms = 0;
    bool at_checked = false;

    start_can();

    g_callStage = CALL_STAGE_MODEM_BOOT;
    uint32_t modem_wait_start = now_ms();

    for (;;) {
        sensor_update();
        g_taskLoops++;
        g_canError = hal_can_get_error();

        uint32_t now = now_ms();
        app_call_update_cooldown(now);

        if (app_call_is_active()) {
            app_call_poll_modem();
            app_call_service();
            if (g_callStage == CALL_STAGE_MODEM_RESET) {
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

        if (g_atOK == 0U) {
            if (check_modem(now, &last_at_check_ms, &at_checked)) {
                last_idle_check_ms = now_ms();
                sensor_update();
            }
            vTaskDelay(pdMS_TO_TICKS(TASK_COMM_WAIT_DELAY_MS));
            continue;
        }

        app_call_poll_modem();
        if (g_atOK == 0U) {
            modem_wait_start = now_ms();
            at_checked = false;
            vTaskDelay(pdMS_TO_TICKS(TASK_COMM_WAIT_DELAY_MS));
            continue;
        }

        sensor_update();

        if (!alarm_wants_call() && (g_callRequest == 0U) &&
            ((now_ms() - last_idle_check_ms) >= MODEM_IDLE_CHECK_MS)) {
            g_atOK = 0;
            at_checked = false;
            check_modem(now_ms(), &last_at_check_ms, &at_checked);
            last_idle_check_ms = now_ms();
            sensor_update();
        }

        bool manual_request = (g_callRequest == 1U);
        if ((g_atOK != 0U) && (alarm_wants_call() || manual_request) &&
            app_call_cooldown_over(now_ms())) {
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
