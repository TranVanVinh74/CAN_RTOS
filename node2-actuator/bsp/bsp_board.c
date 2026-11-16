#include "bsp_board.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

#include "hal_buttons.h"
#include "hal_relay.h"
#include "hal_tft.h"

#define BSP_RELAY_ON_LEVEL GPIO_PIN_SET

extern CAN_HandleTypeDef hcan;
extern SPI_HandleTypeDef hspi1;

volatile BoardError_t g_errorCode = BOARD_ERR_NONE;
volatile char g_stackOverflowTask[configMAX_TASK_NAME_LEN];

static BoardError_t init_step = BOARD_ERR_NONE;

static const ButtonPin_t button_pins[BUTTON_COUNT] = {
    [BUTTON_SELECT] = {BUTTON_SELECT_GPIO_Port, BUTTON_SELECT_Pin},
    [BUTTON_PLUS] = {BUTTON_PLUS_GPIO_Port, BUTTON_PLUS_Pin},
    [BUTTON_MINUS] = {BUTTON_MINUS_GPIO_Port, BUTTON_MINUS_Pin},
    [BUTTON_SAVE] = {BUTTON_SAVE_GPIO_Port, BUTTON_SAVE_Pin},
};

static const TftPins_t tft_pins = {
    .cs_port = TFT_CS_GPIO_Port,
    .cs_pin = TFT_CS_Pin,
    .dc_port = TFT_DC_GPIO_Port,
    .dc_pin = TFT_DC_Pin,
    .rst_port = TFT_RST_GPIO_Port,
    .rst_pin = TFT_RST_Pin,
};

void bsp_board_init(void) {
    init_step = BOARD_ERR_NONE;
    hal_relay_init(RELAY_FAN_GPIO_Port, RELAY_FAN_Pin, BSP_RELAY_ON_LEVEL);
    hal_buttons_init(button_pins);
    hal_tft_init(&hspi1, &tft_pins);
    hal_can_init(&hcan);
}

void bsp_board_set_init_step(BoardError_t step) {
    init_step = step;
}

void bsp_board_record_error(void) {
    if (g_errorCode != BOARD_ERR_NONE) {
        return;
    }

    if ((init_step == BOARD_ERR_OSCILLATOR) && ((RCC->CR & RCC_CR_PLLRDY) != 0U)) {
        g_errorCode = BOARD_ERR_CLOCK;
    } else {
        g_errorCode = init_step;
    }
}

void bsp_board_set_error(BoardError_t error) {
    g_errorCode = error;
}

void bsp_board_fail(BoardError_t error) {
    bsp_board_set_error(error);
    Error_Handler();
}

BoardError_t bsp_board_can_error(CanError_t error) {
    switch (error) {
    case CAN_ERR_NONE:
        return BOARD_ERR_NONE;
    case CAN_ERR_FILTER:
        return BOARD_ERR_CAN_FILTER;
    case CAN_ERR_NOTIFICATION:
        return BOARD_ERR_CAN_NOTIFICATION;
    case CAN_ERR_NO_HANDLE:
    case CAN_ERR_START:
    default:
        return BOARD_ERR_CAN_START;
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    for (uint32_t i = 0; (i < sizeof(g_stackOverflowTask) - 1U) && (pcTaskName[i] != '\0'); i++) {
        g_stackOverflowTask[i] = pcTaskName[i];
    }
    bsp_board_fail(BOARD_ERR_STACK_OVERFLOW);
}
