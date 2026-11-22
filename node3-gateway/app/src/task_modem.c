/**
 * @file task_modem.c
 * @brief Modem readiness and automatic call control driven by the latest alarm state.
 */

#include "task_modem.h"

#include "task.h"

#include "app_alert_phone.h"
#include "app_call.h"
#include "app_cfg.h"
#include "app_ipc.h"
#include "app_time.h"

static uint32_t modem_wait_start_ms = 0;
static uint32_t last_at_check_ms = 0;
static uint32_t last_idle_check_ms = 0;
static bool at_checked = false;

/**
 * @brief Check the latest alarm and its original CAN reception age.
 * @return true if current alarm data permits an automatic call.
 */
static bool alarm_wants_call(void) {
    AlarmState_t state;
    return (xQueuePeek(xAlarmQueue, &state, 0U) == pdPASS) && state.wants_call &&
           ((app_time_ms() - state.received_ms) <= SENSOR_FRESH_TIMEOUT_MS);
}

/**
 * @brief Restart the modem boot-wait timer and clear AT-probe scheduling state.
 */
static void restart_modem_wait(void) {
    modem_wait_start_ms = app_time_ms();
    at_checked = false;
}

/**
 * @brief Probe modem readiness and record the start and end times of the check.
 */
static void check_modem(void) {
    at_checked = true;
    last_at_check_ms = app_time_ms();
    (void)app_call_check_modem();
    last_idle_check_ms = app_time_ms();
}

/**
 * @brief Service modem readiness, automatic calls and hang-up requests.
 * @param[in] pvParameters Unused task argument.
 */
static void prvModemTask(void *pvParameters) {
    (void)pvParameters;
    app_call_init();
    restart_modem_wait();

    for (;;) {
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
            vTaskDelay(pdMS_TO_TICKS(TASK_MODEM_IDLE_DELAY_MS));
            continue;
        }

        if ((now - modem_wait_start_ms) < MODEM_BOOT_WAIT_MS) {
            vTaskDelay(pdMS_TO_TICKS(TASK_MODEM_WAIT_DELAY_MS));
            continue;
        }

        if (!app_call_modem_ready()) {
            if (!at_checked || ((now - last_at_check_ms) >= MODEM_AT_RETRY_MS)) {
                check_modem();
            }
            vTaskDelay(pdMS_TO_TICKS(TASK_MODEM_WAIT_DELAY_MS));
            continue;
        }

        app_call_poll_modem();
        if (!app_call_modem_ready()) {
            restart_modem_wait();
            vTaskDelay(pdMS_TO_TICKS(TASK_MODEM_WAIT_DELAY_MS));
            continue;
        }

        /* An idle modem can reboot without notice; re-check it before an alarm needs it. */
        if (!alarm_wants_call() &&
            ((app_time_ms() - last_idle_check_ms) >= MODEM_IDLE_CHECK_MS)) {
            check_modem();
        }

        if (app_call_modem_ready() && alarm_wants_call() &&
            app_call_cooldown_over(app_time_ms())) {
            app_call_start(APP_ALERT_PHONE);
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_MODEM_IDLE_DELAY_MS));
    }
}

BaseType_t task_modem_create(void) {
    return xTaskCreate(prvModemTask, "MODEM", TASK_MODEM_STACK_WORDS, NULL, TASK_MODEM_PRIO, NULL);
}
