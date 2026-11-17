/**
 * @file bsp_board.h
 * @brief Board pin/peripheral bindings, driver initialization and failure diagnostics.
 */

#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include <stdint.h>

#include "stm32f1xx_hal.h"
#include "main.h"

#define BSP_RELAY_PORT GPIOB
#define BSP_RELAY_PIN GPIO_PIN_0
#define BSP_RELAY_ON_LEVEL GPIO_PIN_SET

#define BSP_BUTTON_SELECT_PORT BUTTON_SELECT_GPIO_Port
#define BSP_BUTTON_SELECT_PIN BUTTON_SELECT_Pin
#define BSP_BUTTON_PLUS_PORT BUTTON_PLUS_GPIO_Port
#define BSP_BUTTON_PLUS_PIN BUTTON_PLUS_Pin
#define BSP_BUTTON_MINUS_PORT BUTTON_MINUS_GPIO_Port
#define BSP_BUTTON_MINUS_PIN BUTTON_MINUS_Pin
#define BSP_BUTTON_SAVE_PORT BUTTON_SAVE_GPIO_Port
#define BSP_BUTTON_SAVE_PIN BUTTON_SAVE_Pin

typedef enum {
    BOARD_ERR_NONE = 0,
    BOARD_ERR_OSCILLATOR = 1,
    BOARD_ERR_CLOCK = 2,
    BOARD_ERR_SPI_INIT = 3,
    BOARD_ERR_CAN_INIT = 4,
    BOARD_ERR_QUEUE = 5,
    BOARD_ERR_TASK = 6,
    BOARD_ERR_SCHEDULER = 7,
    BOARD_ERR_CAN_FILTER = 8,
    BOARD_ERR_CAN_START = 9,
    BOARD_ERR_CAN_NOTIFICATION = 10,
    BOARD_ERR_STACK_OVERFLOW = 11
} BoardError_t;

extern volatile uint32_t g_errorCode;

/**
 * @brief Initialize the relay, button mappings, TFT SPI driver and CAN handle binding.
 */
void bsp_board_init(void);
/**
 * @brief Record a board error and enter the non-returning error handler.
 * @param[in] error Board error code identifying the failed initialization or runtime check.
 */
void bsp_board_fail(BoardError_t error);
/**
 * @brief Record the initialization step so a later HAL failure can be identified.
 * @param[in] step Board initialization stage currently being executed.
 */
void bsp_board_set_init_step(BoardError_t step);
/**
 * @brief Preserve the first board error or infer it from the current initialization stage.
 */
void bsp_board_record_error(void);

#endif
