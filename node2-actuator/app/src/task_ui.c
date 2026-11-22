/**
 * @file task_ui.c
 * @brief Periodic TFT rendering from the latest control snapshot.
 */

#include "task_ui.h"

#include "task.h"

#include "app_cfg.h"
#include "app_ipc.h"
#include "app_time.h"
#include "app_ui.h"

/**
 * @brief Consume the most recent view and render the display at the frame interval.
 * @param[in] pvParameters Unused task argument.
 */
static void prvUiTask(void *pvParameters) {
    (void)pvParameters;
    UiView_t view;
    app_ui_reset();
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        if (xQueueReceive(xUiQueue, &view, 0U) == pdPASS) {
            app_ui_update(&view);
        }
        app_ui_draw(app_time_ms());
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(UI_FRAME_MS));
    }
}

BaseType_t task_ui_create(void) {
    return xTaskCreate(prvUiTask, "UI", TASK_UI_STACK_WORDS, NULL, TASK_UI_PRIO, NULL);
}
