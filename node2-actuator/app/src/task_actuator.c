#include "task_actuator.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"
#include "app_fan.h"
#include "app_ipc.h"
#include "app_ui.h"
#include "bsp_board.h"
#include "can_protocol.h"
#include "hal_buttons.h"
#include "hal_can.h"
#include "hal_relay.h"
#include "hal_tft_graphics.h"

#define TFT_STAGE_WAITING 2U
#define TFT_STAGE_DATA 3U
#define TFT_STAGE_ERROR 99U

#define CAN_STAGE_STARTING 1U
#define CAN_STAGE_RUNNING 2U
#define CAN_STAGE_ERROR 99U

volatile ActuatorCommand_t g_node2Debug = {0};
volatile uint32_t g_tftStage = 0;
volatile uint32_t g_canStage = 0;
volatile uint32_t g_taskLoops = 0;
volatile uint32_t g_canRxCount = 0;
volatile uint32_t g_canProcessedCount = 0;
volatile uint32_t g_canQueueFullCount = 0;
volatile uint32_t g_canInvalidCount = 0;
volatile uint32_t g_buttonPressCount[BUTTON_COUNT] = {0};
volatile UiView_t g_uiDebug = {0};

static TaskHandle_t xActuatorTaskHandle = NULL;

static void process_buttons(UiView_t *view, uint32_t now_ms) {
    static const UiButton_t ui_buttons[BUTTON_COUNT] = {
        [BUTTON_SELECT] = UI_BUTTON_SELECT,
        [BUTTON_PLUS] = UI_BUTTON_PLUS,
        [BUTTON_MINUS] = UI_BUTTON_MINUS,
        [BUTTON_SAVE] = UI_BUTTON_SAVE,
    };
    uint8_t events = hal_buttons_poll(now_ms);
    for (uint8_t i = 0U; i < BUTTON_COUNT; i++) {
        if ((events & (1U << i)) != 0U) {
            g_buttonPressCount[i]++;
            app_ui_button(view, ui_buttons[i]);
        }
    }
}

static bool is_sensor_frame(const CanRxFrame_t *frame) {
    return !frame->is_extended && !frame->is_remote && (frame->std_id == CAN_ID_SENSOR) &&
           (frame->dlc == CAN_DLC_SENSOR);
}

static void on_can_frame(const CanRxFrame_t *frame, BaseType_t *task_woken) {
    ActuatorCommand_t cmd = {0};

    if (!is_sensor_frame(frame)) {
        g_canInvalidCount++;
        return;
    }

    can_unpack_sensor(frame->data, &cmd.temperature, &cmd.humidity, &cmd.gas_raw);
    g_canRxCount++;

    if (xQueueSendFromISR(xActuatorQueue, &cmd, task_woken) != pdPASS) {
        g_canQueueFullCount++;
    }
}

static void draw_view(const UiView_t *view) {
    app_ui_render(view);
    g_tftStage = hal_tft_ok() ?
                 (view->has_data ? TFT_STAGE_DATA : TFT_STAGE_WAITING) : TFT_STAGE_ERROR;
}

static bool start_can(void) {
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
        g_errorCode = board_error[error];
        g_canStage = CAN_STAGE_ERROR;
        return false;
    }

    g_canStage = CAN_STAGE_RUNNING;
    return true;
}

static void prvActuatorTask(void *pvParameters) {
    (void)pvParameters;

    UiView_t view = {
        .gas_threshold = APP_GAS_LIMIT_DEFAULT_RAW,
        .temperature_threshold = APP_TEMPERATURE_LIMIT_DEFAULT_C,
        .temperature_threshold_configured = true,
        .edit_mode = false,
    };
    TickType_t last_sensor_tick = 0;

    app_ui_reset();
    draw_view(&view);
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
            hal_relay_set(app_fan_should_run(hal_relay_is_on(), cmd.gas_raw));
            cmd.fan_state = hal_relay_is_on() ? 1U : 0U;

            g_node2Debug = cmd;
            g_canProcessedCount++;

            last_sensor_tick = xTaskGetTickCount();
            view.sample_id++;
            view.has_data = true;
            view.temperature = cmd.temperature;
            view.humidity = cmd.humidity;
            view.gas_raw = cmd.gas_raw;
            view.fan_on = cmd.fan_state != 0U;
        }

        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_button_tick) >= pdMS_TO_TICKS(BUTTON_SCAN_MS)) {
            last_button_tick = now;
            process_buttons(&view, now * portTICK_PERIOD_MS);
        }

        view.stale = view.has_data &&
                     ((TickType_t)(now - last_sensor_tick) >=
                      pdMS_TO_TICKS(SCREEN_STALE_TIMEOUT_MS));
        g_canError = hal_can_get_error();
        view.bus_error = !can_started || hal_can_bus_fault();
        if ((TickType_t)(now - last_ui_tick) >= pdMS_TO_TICKS(UI_FRAME_MS)) {
            last_ui_tick = now;
            draw_view(&view);
            app_ui_tick(now * portTICK_PERIOD_MS);
            g_tftStage = hal_tft_ok() ? g_tftStage : TFT_STAGE_ERROR;
        }
        g_uiDebug = view;

        g_taskLoops++;
    }
}

BaseType_t task_actuator_create(void) {
    hal_can_set_rx_handler(on_can_frame);
    return xTaskCreate(prvActuatorTask, "ACTUATOR", TASK_ACTUATOR_STACK_WORDS, NULL,
                       TASK_ACTUATOR_PRIO, &xActuatorTaskHandle);
}
