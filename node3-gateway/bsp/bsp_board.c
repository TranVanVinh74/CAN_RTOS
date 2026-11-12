#include "bsp_board.h"

#include "main.h"

#include "hal_a7680c.h"
#include "hal_can.h"

extern CAN_HandleTypeDef hcan;
extern UART_HandleTypeDef huart2;

volatile uint32_t g_errorCode = BOARD_ERR_NONE;

void bsp_board_init(void) {
    hal_can_init(&hcan);
    hal_a7680c_init(&huart2);
}

void bsp_board_fail(BoardError_t error) {
    g_errorCode = error;
    Error_Handler();
}
