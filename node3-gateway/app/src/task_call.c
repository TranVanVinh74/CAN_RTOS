/**
 * @file task_call.c
 * @brief Call state machine and call policy.
 */

#include "task_call.h"

#include <stdio.h>

#include "app_alert_phone.h"
#include "app_cfg.h"
#include "app_ipc.h"
#include "app_time.h"

typedef enum {
    CALL_BOOT_WAIT = 0,
    CALL_WAIT_AT,
    CALL_READY,
    CALL_IN_PROGRESS
} CallState_t;

static bool alarm_wants_call(void) {
    AlarmState_t state;
    return xQueuePeek(xAlarmQueue, &state, 0U) == pdPASS && state.wants_call &&
           (app_time_ms() - state.received_ms <= SENSOR_FRESH_TIMEOUT_MS);
}

static bool enqueue_command(AtCommandType_t type, const char *text) {
    AtCommand_t command = {.type = type, .timeout_ms = MODEM_COMMAND_TIMEOUT_MS};
    (void)snprintf(command.text, sizeof(command.text), "%s", text);
    return xQueueSend(xAtCmdQueue, &command, 0U) == pdPASS;
}

static void prvCallTask(void *argument) {
    (void)argument;
    CallState_t state = CALL_BOOT_WAIT;
    TickType_t boot_started = xTaskGetTickCount();
    TickType_t last_at_check = 0U;
    TickType_t last_idle_check = 0U;
    TickType_t call_started = 0U;
    TickType_t last_call = 0U;
    bool call_attempted = false;
    bool command_pending = false;

    for (;;) {
        ModemEvent_t event;
        while (xQueueReceive(xModemEventQueue, &event, 0U) == pdPASS) {
            command_pending = false;
            if (event.type == MODEM_EVENT_RESET || event.type == MODEM_EVENT_TX_FAILED ||
                event.type == MODEM_EVENT_TIMEOUT) {
                state = CALL_WAIT_AT;
                last_at_check = xTaskGetTickCount() - pdMS_TO_TICKS(MODEM_AT_RETRY_MS);
                continue;
            }
            if (event.type == MODEM_EVENT_OK) {
                if (event.command == AT_CMD_CHECK) {
                    state = CALL_READY;
                    last_idle_check = xTaskGetTickCount();
                } else if (event.command == AT_CMD_DIAL) {
                    state = CALL_IN_PROGRESS;
                    call_started = xTaskGetTickCount();
                } else if (event.command == AT_CMD_HANGUP) {
                    state = CALL_READY;
                }
            } else if (event.type == MODEM_EVENT_CALL_ENDED || event.type == MODEM_EVENT_ERROR) {
                state = (event.command == AT_CMD_CHECK) ? CALL_WAIT_AT : CALL_READY;
                if (event.command == AT_CMD_CHECK) {
                    last_at_check = xTaskGetTickCount() - pdMS_TO_TICKS(MODEM_AT_RETRY_MS);
                }
            }
        }

        TickType_t now = xTaskGetTickCount();
        if (state == CALL_BOOT_WAIT) {
            if ((now - boot_started) >= pdMS_TO_TICKS(MODEM_BOOT_WAIT_MS)) {
                state = CALL_WAIT_AT;
                last_at_check = now - pdMS_TO_TICKS(MODEM_AT_RETRY_MS);
            }
        } else if (state == CALL_WAIT_AT) {
            if (!command_pending && (now - last_at_check) >= pdMS_TO_TICKS(MODEM_AT_RETRY_MS)) {
                if (enqueue_command(AT_CMD_CHECK, "AT\r\n")) {
                    command_pending = true;
                    last_at_check = now;
                }
            }
        } else if (state == CALL_READY) {
            if (!command_pending && !alarm_wants_call() &&
                (now - last_idle_check) >= pdMS_TO_TICKS(MODEM_IDLE_CHECK_MS)) {
                if (enqueue_command(AT_CMD_CHECK, "AT\r\n")) {
                    command_pending = true;
                    last_idle_check = now;
                    state = CALL_WAIT_AT;
                }
            }
            if (!command_pending && alarm_wants_call() &&
                (!call_attempted || (now - last_call) >= pdMS_TO_TICKS(CALL_INTERVAL_MS))) {
                char command[AT_COMMAND_TEXT_LEN];
                (void)snprintf(command, sizeof(command), "ATD%s;\r\n", APP_ALERT_PHONE);
                if (enqueue_command(AT_CMD_DIAL, command)) {
                    call_attempted = true;
                    last_call = now;
                    command_pending = true;
                }
            }
        } else if (state == CALL_IN_PROGRESS) {
            if ((!alarm_wants_call() || (now - call_started) >= pdMS_TO_TICKS(CALL_DURATION_MS)) &&
                !command_pending) {
                if (enqueue_command(AT_CMD_HANGUP, "ATH\r\n")) {
                    command_pending = true;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_CALL_POLL_MS));
    }
}

BaseType_t task_call_create(void) {
    return xTaskCreate(prvCallTask, "CALL", TASK_CALL_STACK_WORDS, NULL, TASK_CALL_PRIO, NULL);
}
