#include "task_actuator.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"
#include "app_ipc.h"
#include "app_settings.h"
#include "app_threshold_sync.h"
#include "app_ui.h"
#include "bsp_board.h"
#include "can_protocol.h"
#include "sensor_status.h"
#include "hal_buttons.h"
#include "hal_can.h"
#include "hal_relay.h"
#include "hal_tft_graphics.h"

static TaskHandle_t xActuatorTaskHandle = NULL;

static bool process_buttons(UiView_t *view, AppSettings_t *active, uint32_t now_ms) {
    static const UiButton_t ui_buttons[BUTTON_COUNT] = {
        [BUTTON_SELECT] = UI_BUTTON_SELECT,
        [BUTTON_PLUS] = UI_BUTTON_PLUS,
        [BUTTON_MINUS] = UI_BUTTON_MINUS,
        [BUTTON_SAVE] = UI_BUTTON_SAVE,
    };
    uint8_t events = hal_buttons_poll(now_ms);
    bool changed = false;

    for (uint8_t i = 0U; i < BUTTON_COUNT; i++) {
        if ((events & (1U << i)) != 0U) {
            if ((i == BUTTON_SAVE) && view->edit_mode) {
                AppSettings_t settings = {
                    .gas_limit_raw = view->gas_threshold,
                    .temperature_limit_c = view->temperature_threshold,
                };
                SettingsSaveResult_t result = app_settings_save(&settings);
                view->settings_save_error = (result == SETTINGS_SAVE_ERROR) ||
                                            (result == SETTINGS_INVALID);
                if (view->settings_save_error) {
                    continue;
                }
                changed = (settings.gas_limit_raw != active->gas_limit_raw) ||
                          (settings.temperature_limit_c != active->temperature_limit_c);
                *active = settings;
            }
            app_ui_button(view, ui_buttons[i]);
        }
    }
    return changed;
}

static bool is_sensor_frame(const CanRxFrame_t *frame) {
    return !frame->is_extended && !frame->is_remote && (frame->std_id == CAN_ID_SENSOR) &&
           (frame->dlc == CAN_DLC_SENSOR);
}

static void on_can_frame(const CanRxFrame_t *frame, BaseType_t *task_woken) {
    ActuatorCommand_t cmd = {0};

    if (!is_sensor_frame(frame)) {
        return;
    }

    can_unpack_sensor(frame->data, &cmd.temperature, &cmd.humidity, &cmd.gas_raw,
                      &cmd.sensor_faults);
    if (!sensor_faults_valid(cmd.sensor_faults)) {
        return;
    }

    (void)xQueueSendFromISR(xActuatorQueue, &cmd, task_woken);
}

static bool start_can(void) {
    static const BoardError_t board_error[] = {
        [CAN_ERR_NONE] = BOARD_ERR_NONE,
        [CAN_ERR_NO_HANDLE] = BOARD_ERR_CAN_START,
        [CAN_ERR_FILTER] = BOARD_ERR_CAN_FILTER,
        [CAN_ERR_START] = BOARD_ERR_CAN_START,
        [CAN_ERR_NOTIFICATION] = BOARD_ERR_CAN_NOTIFICATION,
    };

    CanError_t error = hal_can_start();
    if (error != CAN_ERR_NONE) {
        g_errorCode = board_error[error];
        return false;
    }
    return true;
}

static void prvActuatorTask(void *pvParameters) {
    (void)pvParameters;

    AppSettings_t settings;
    (void)app_settings_load(&settings);
    UiView_t view = {
        .gas_threshold = settings.gas_limit_raw,
        .temperature_threshold = settings.temperature_limit_c,
    };
    app_threshold_sync_init();
    TickType_t last_sensor_tick = 0;

    app_ui_reset();
    app_ui_render(&view);
    for (uint8_t frame = 0U; frame < UI_BOOT_LAYOUT_FRAMES; frame++) {
        app_ui_tick(xTaskGetTickCount() * portTICK_PERIOD_MS);
        vTaskDelay(pdMS_TO_TICKS(UI_FRAME_MS));
    }
    bool can_started = start_can();
    TickType_t last_button_tick = xTaskGetTickCount();
    TickType_t last_ui_tick = last_button_tick;

    for (;;) {
        ActuatorCommand_t cmd;
        TickType_t elapsed = xTaskGetTickCount() - last_button_tick;
        TickType_t wait_ticks = elapsed < pdMS_TO_TICKS(BUTTON_SCAN_MS) ?
                                pdMS_TO_TICKS(BUTTON_SCAN_MS) - elapsed : 0U;

        if (xQueueReceive(xActuatorQueue, &cmd, wait_ticks) == pdPASS) {
            hal_relay_set(sensor_fan_should_run(hal_relay_is_on(), cmd.gas_raw, cmd.temperature,
                                                cmd.sensor_faults, &settings));

            last_sensor_tick = xTaskGetTickCount();
            view.sample_id++;
            view.has_data = true;
            view.temperature = cmd.temperature;
            view.humidity = cmd.humidity;
            view.gas_raw = cmd.gas_raw;
            view.sensor_faults = cmd.sensor_faults;
            view.fan_on = hal_relay_is_on();
        }

        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_button_tick) >= pdMS_TO_TICKS(BUTTON_SCAN_MS)) {
            last_button_tick = now;
            bool changed = process_buttons(&view, &settings, now * portTICK_PERIOD_MS);
            if (changed && view.has_data &&
                ((TickType_t)(now - last_sensor_tick) <=
                 pdMS_TO_TICKS(SCREEN_STALE_TIMEOUT_MS))) {
                hal_relay_set(sensor_fan_should_run(hal_relay_is_on(), view.gas_raw,
                                                    view.temperature, view.sensor_faults,
                                                    &settings));
                view.fan_on = hal_relay_is_on();
            }
        }

        if (can_started) {
            app_threshold_sync_update(&settings, xTaskGetTickCount() * portTICK_PERIOD_MS);
        }

        view.stale = view.has_data &&
                     ((TickType_t)(now - last_sensor_tick) >=
                      pdMS_TO_TICKS(SCREEN_STALE_TIMEOUT_MS));
        view.bus_error = !can_started || hal_can_bus_fault();
        if ((TickType_t)(now - last_ui_tick) >= pdMS_TO_TICKS(UI_FRAME_MS)) {
            last_ui_tick = now;
            app_ui_render(&view);
            app_ui_tick(now * portTICK_PERIOD_MS);
        }
    }
}

BaseType_t task_actuator_create(void) {
    hal_can_set_rx_handler(on_can_frame);
    return xTaskCreate(prvActuatorTask, "ACTUATOR", TASK_ACTUATOR_STACK_WORDS, NULL,
                       TASK_ACTUATOR_PRIO, &xActuatorTaskHandle);
}
