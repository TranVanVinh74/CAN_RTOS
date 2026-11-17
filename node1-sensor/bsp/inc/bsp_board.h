/**
 * @file bsp_board.h
 * @brief Board pin/peripheral bindings, driver initialization and failure diagnostics.
 */

#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "hal_can.h"

typedef enum {
    BOARD_ERR_NONE,
    BOARD_ERR_OSCILLATOR,
    BOARD_ERR_CLOCK,
    BOARD_ERR_ADC_INIT,
    BOARD_ERR_CAN_INIT,
    BOARD_ERR_TIM_INIT,
    BOARD_ERR_QUEUE,
    BOARD_ERR_TASK,
    BOARD_ERR_SCHEDULER,
    BOARD_ERR_CAN_FILTER,
    BOARD_ERR_CAN_START,
    BOARD_ERR_CAN_NOTIFICATION,
    BOARD_ERR_STACK_OVERFLOW
} BoardError_t;

/**
 * @brief Initialize DHT11, calibrate MQ5 and bind CAN; sensor initialization failures remain nonfatal.
 */
void bsp_board_init(void);
/**
 * @brief Record the board initialization stage for later failure diagnosis.
 * @param[in] step Board initialization stage currently being executed.
 */
void bsp_board_set_init_step(BoardError_t step);
/**
 * @brief Preserve the first board error or infer it from the recorded initialization stage.
 */
void bsp_board_record_error(void);
/**
 * @brief Record a board failure and enter the non-returning error handler.
 * @param[in] error Board error code to retain for debugger inspection.
 */
void bsp_board_fail(BoardError_t error);
/**
 * @brief Map CAN driver initialization errors to board-level failure codes.
 * @param[in] error CAN driver initialization result to translate.
 * @return BOARD_ERR_NONE for no error, or the corresponding board CAN failure code.
 */
BoardError_t bsp_board_can_error(CanError_t error);

#endif
