#include "task_actuator.h"

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"
#include "app_fan.h"
#include "app_ipc.h"
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

#define SCREEN_X 5U
#define SCREEN_TITLE_Y 5U
#define SCREEN_RULE_Y 22U
#define SCREEN_TEMP_Y 35U
#define SCREEN_HUM_Y 55U
#define SCREEN_GAS_Y 75U
#define SCREEN_FAN_Y 95U
#define SCREEN_STATUS_Y 125U
#define SCREEN_TEXT_LEN 24U

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

static void draw_text(uint8_t y, const char *text, uint16_t color) {
    hal_tft_draw_string(SCREEN_X, y, text, &tft_font_7x10, color, TFT_BLACK);
}

static void draw_waiting_screen(void) {
    if (!hal_tft_ok()) {
        g_tftStage = TFT_STAGE_ERROR;
        return;
    }

    hal_tft_fill(TFT_BLACK);
    draw_text(SCREEN_TITLE_Y, "NODE 2 - CAN", TFT_YELLOW);
    hal_tft_draw_line(0, SCREEN_RULE_Y, TFT_WIDTH - 1, SCREEN_RULE_Y, TFT_WHITE);
    draw_text(SCREEN_TEMP_Y, "TEMP: --- C", TFT_CYAN);
    draw_text(SCREEN_HUM_Y, "HUM : --- %", TFT_CYAN);
    draw_text(SCREEN_GAS_Y, "GAS : -----", TFT_WHITE);
    draw_text(SCREEN_FAN_Y, "FAN : OFF", TFT_GREEN);
    draw_text(SCREEN_STATUS_Y, "WAIT CAN... ", TFT_YELLOW);

    g_tftStage = hal_tft_ok() ? TFT_STAGE_WAITING : TFT_STAGE_ERROR;
}

static void draw_values(const ActuatorCommand_t *cmd) {
    char text[SCREEN_TEXT_LEN];

    if (!hal_tft_ok()) {
        g_tftStage = TFT_STAGE_ERROR;
        return;
    }

    snprintf(text, sizeof(text), "TEMP: %3u C", (unsigned int)cmd->temperature);
    draw_text(SCREEN_TEMP_Y, text, TFT_CYAN);

    snprintf(text, sizeof(text), "HUM : %3u %%", (unsigned int)cmd->humidity);
    draw_text(SCREEN_HUM_Y, text, TFT_CYAN);

    snprintf(text, sizeof(text), "GAS : %5u", (unsigned int)cmd->gas_level);
    draw_text(SCREEN_GAS_Y, text, TFT_WHITE);

    if (cmd->fan_state != 0U) {
        draw_text(SCREEN_FAN_Y, "FAN : ON ", TFT_RED);
    } else {
        draw_text(SCREEN_FAN_Y, "FAN : OFF", TFT_GREEN);
    }

    draw_text(SCREEN_STATUS_Y, "CAN RX OK   ", TFT_GREEN);

    g_tftStage = hal_tft_ok() ? TFT_STAGE_DATA : TFT_STAGE_ERROR;
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

static void prvActuatorTask(void *pvParameters) {
    (void)pvParameters;

    draw_waiting_screen();
    start_can();

    for (;;) {
        ActuatorCommand_t cmd;

        if (xQueueReceive(xActuatorQueue, &cmd, pdMS_TO_TICKS(TASK_ACTUATOR_QUEUE_WAIT_MS)) ==
            pdPASS) {
            hal_relay_set(app_fan_should_run(hal_relay_is_on(), cmd.gas_level));
            cmd.fan_state = hal_relay_is_on() ? 1U : 0U;

            g_node2Debug = cmd;
            g_canProcessedCount++;

            draw_values(&cmd);
        }

        g_taskLoops++;
        g_canError = hal_can_get_error();
    }
}

BaseType_t task_actuator_create(void) {
    hal_can_set_rx_handler(on_can_frame);
    return xTaskCreate(prvActuatorTask, "ACTUATOR", TASK_ACTUATOR_STACK_WORDS, NULL,
                       TASK_ACTUATOR_PRIO, &xActuatorTaskHandle);
}
