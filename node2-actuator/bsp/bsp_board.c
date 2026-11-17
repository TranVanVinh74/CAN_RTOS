/**
 * @file bsp_board.c
 * @brief Board pin/peripheral bindings, driver initialization and failure diagnostics.
 */

#include "bsp_board.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

#include "hal_buttons.h"
#include "hal_can.h"
#include "hal_relay.h"
#include "hal_tft.h"

extern CAN_HandleTypeDef hcan;
extern SPI_HandleTypeDef hspi1;

volatile uint32_t g_errorCode = BOARD_ERR_NONE;
volatile char g_stackOverflowTask[configMAX_TASK_NAME_LEN];

static BoardError_t init_step = BOARD_ERR_NONE;

static const ButtonPin_t button_pins[BUTTON_COUNT] = {
    [BUTTON_SELECT] = {BSP_BUTTON_SELECT_PORT, BSP_BUTTON_SELECT_PIN},
    [BUTTON_PLUS] = {BSP_BUTTON_PLUS_PORT, BSP_BUTTON_PLUS_PIN},
    [BUTTON_MINUS] = {BSP_BUTTON_MINUS_PORT, BSP_BUTTON_MINUS_PIN},
    [BUTTON_SAVE] = {BSP_BUTTON_SAVE_PORT, BSP_BUTTON_SAVE_PIN},
};

static const TftPins_t tft_pins = {
    .cs_port = TFT_CS_GPIO_Port,
    .cs_pin = TFT_CS_Pin,
    .dc_port = TFT_DC_GPIO_Port,
    .dc_pin = TFT_DC_Pin,
    .rst_port = TFT_RST_GPIO_Port,
    .rst_pin = TFT_RST_Pin,
};

/**
 * @brief Initialize the relay, button mappings, TFT SPI driver and CAN handle binding.
 */
void bsp_board_init(void) {
    init_step = BOARD_ERR_NONE;
    hal_relay_init(BSP_RELAY_PORT, BSP_RELAY_PIN, BSP_RELAY_ON_LEVEL);
    hal_buttons_init(button_pins);
    hal_tft_init(&hspi1, &tft_pins);
    hal_can_init(&hcan);
}

/**
 * @brief Record a board error and enter the non-returning error handler.
 * @param[in] error Board error code identifying the failed initialization or runtime check.
 */
void bsp_board_fail(BoardError_t error) {
    g_errorCode = error;
    Error_Handler();
}

/**
 * @brief Record the initialization step so a later HAL failure can be identified.
 * @param[in] step Board initialization stage currently being executed.
 */
void bsp_board_set_init_step(BoardError_t step) {
    init_step = step;
}

/**
 * @brief Preserve the first board error or infer it from the current initialization stage.
 */
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

/**
 * @brief Capture the overflowing task name and stop execution through the error handler.
 * @param[in] xTask Handle of the task whose stack overflowed; unused by this implementation.
 * @param[in] pcTaskName FreeRTOS task name copied into the diagnostic buffer.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    for (uint32_t i = 0; i < (sizeof(g_stackOverflowTask) - 1U) && (pcTaskName[i] != '\0'); i++) {
        g_stackOverflowTask[i] = pcTaskName[i];
    }
    bsp_board_fail(BOARD_ERR_STACK_OVERFLOW);
}
