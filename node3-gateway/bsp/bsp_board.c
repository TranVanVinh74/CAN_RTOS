#include "bsp_board.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

#include "hal_a7680c.h"

extern CAN_HandleTypeDef hcan;
extern UART_HandleTypeDef huart2;

volatile BoardError_t g_errorCode = BOARD_ERR_NONE;
volatile char g_stackOverflowTask[configMAX_TASK_NAME_LEN];

static BoardError_t init_step = BOARD_ERR_NONE;

void bsp_board_init(void) {
    init_step = BOARD_ERR_NONE;
    hal_can_init(&hcan);
    hal_a7680c_init(&huart2);
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

void bsp_board_fail(BoardError_t error) {
    g_errorCode = error;
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
