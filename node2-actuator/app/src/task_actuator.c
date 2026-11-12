#include "task_actuator.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"
#include "app_fan.h"
#include "app_ipc.h"
#include "app_ui.h"
#include "bsp_board.h"
#include "can_protocol.h"
#include "hal_can.h"
#include "hal_relay.h"
#include "hal_tft.h"

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

static TaskHandle_t xActuatorTaskHandle = NULL;

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

    can_unpack_sensor(frame->data, &cmd.temperature, &cmd.humidity, &cmd.gas_level);
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
        .gas_threshold = FAN_GAS_ON_THRESHOLD,
        .temperature_threshold_configured = false,
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

    for (;;) {
        ActuatorCommand_t cmd;

        if (xQueueReceive(xActuatorQueue, &cmd, pdMS_TO_TICKS(UI_FRAME_MS)) ==
            pdPASS) {
            hal_relay_set(app_fan_should_run(hal_relay_is_on(), cmd.gas_level));
            cmd.fan_state = hal_relay_is_on() ? 1U : 0U;

            g_node2Debug = cmd;
            g_canProcessedCount++;

            last_sensor_tick = xTaskGetTickCount();
            view.sample_id++;
            view.has_data = true;
            view.temperature = cmd.temperature;
            view.humidity = cmd.humidity;
            view.gas_raw = cmd.gas_level;
            view.fan_on = cmd.fan_state != 0U;
        }

        view.stale = view.has_data &&
                     ((TickType_t)(xTaskGetTickCount() - last_sensor_tick) >=
                      pdMS_TO_TICKS(SCREEN_STALE_TIMEOUT_MS));
        g_canError = hal_can_get_error();
        view.bus_error = !can_started || hal_can_bus_fault();
        draw_view(&view);
        app_ui_tick(xTaskGetTickCount() * portTICK_PERIOD_MS);
        g_tftStage = hal_tft_ok() ? g_tftStage : TFT_STAGE_ERROR;

        g_taskLoops++;
    }
}

BaseType_t task_actuator_create(void) {
    hal_can_set_rx_handler(on_can_frame);
    return xTaskCreate(prvActuatorTask, "ACTUATOR", TASK_ACTUATOR_STACK_WORDS, NULL,
                       TASK_ACTUATOR_PRIO, &xActuatorTaskHandle);
}
