/**
 * @file task_buttons.c
 * @brief Periodic button polling and delivery of debounced presses to CONTROL.
 */

#include "task_buttons.h"

#include "task.h"

#include "app_cfg.h"
#include "app_time.h"
#include "hal_buttons.h"
#include "task_actuator.h"

/**
 * @brief Scan the buttons periodically and publish one event mask per scan with new presses.
 * @param[in] pvParameters Unused task argument.
 */
static void prvButtonsTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(BUTTON_SCAN_MS));
        uint8_t events = hal_buttons_poll(app_time_ms());
        if (events != 0U) {
            task_actuator_post_buttons(events);
        }
    }
}

BaseType_t task_buttons_create(void) {
    return xTaskCreate(prvButtonsTask, "BUTTONS", TASK_BUTTONS_STACK_WORDS, NULL,
                       TASK_BUTTONS_PRIO, NULL);
}
