/**
 * @file bsp_board.h
 * @brief Board pin/peripheral bindings, driver initialization and failure diagnostics.
 */

#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include <stdint.h>

#include "stm32f1xx_hal.h"

typedef enum {
    BOARD_ERR_NONE = 0,
    BOARD_ERR_OSCILLATOR = 1,
    BOARD_ERR_CLOCK = 2,
    BOARD_ERR_UART1_INIT = 3,
    BOARD_ERR_UART2_INIT = 4,
    BOARD_ERR_CAN_INIT = 5,
    BOARD_ERR_QUEUE = 6,
    BOARD_ERR_TASK = 7,
    BOARD_ERR_CAN_FILTER = 8,
    BOARD_ERR_CAN_START = 9,
    BOARD_ERR_CAN_NOTIFICATION = 10,
    BOARD_ERR_SCHEDULER = 11,
    BOARD_ERR_STACK_OVERFLOW = 12
} BoardError_t;

extern volatile uint32_t g_errorCode;

/**
 * @brief Bind CAN and initialize the A7680C driver on USART2.
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
