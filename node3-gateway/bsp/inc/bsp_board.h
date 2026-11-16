#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "hal_can.h"

typedef enum {
    BOARD_ERR_NONE,
    BOARD_ERR_OSCILLATOR,
    BOARD_ERR_CLOCK,
    BOARD_ERR_CAN_INIT,
    BOARD_ERR_UART1_INIT,
    BOARD_ERR_UART2_INIT,
    BOARD_ERR_QUEUE,
    BOARD_ERR_TASK,
    BOARD_ERR_SCHEDULER,
    BOARD_ERR_CAN_FILTER,
    BOARD_ERR_CAN_START,
    BOARD_ERR_CAN_NOTIFICATION,
    BOARD_ERR_STACK_OVERFLOW
} BoardError_t;

void bsp_board_init(void);
void bsp_board_set_init_step(BoardError_t step);
void bsp_board_record_error(void);
void bsp_board_fail(BoardError_t error);
BoardError_t bsp_board_can_error(CanError_t error);

#endif
