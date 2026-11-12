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

void bsp_board_init(void);
void bsp_board_fail(BoardError_t error);
void bsp_board_set_init_step(BoardError_t step);
void bsp_board_record_error(void);

#endif
