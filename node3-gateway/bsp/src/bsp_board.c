/**
 * @file bsp_board.c
 * @brief Board pin/peripheral bindings, driver initialization and failure diagnostics.
 */

#include "bsp_board.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

#include "hal_a7680c.h"

extern CAN_HandleTypeDef hcan;
extern UART_HandleTypeDef huart2;

/* Read with a debugger once the board stops in Error_Handler. */
volatile BoardError_t g_errorCode = BOARD_ERR_NONE;
volatile char g_stackOverflowTask[configMAX_TASK_NAME_LEN];

static BoardError_t init_step = BOARD_ERR_NONE;

/**
 * @brief Bind CAN and initialize the A7680C driver on USART2.
 */
void bsp_board_init(void) {
    init_step = BOARD_ERR_NONE;
    hal_can_init(&hcan);
    hal_a7680c_init(&huart2);
}

/**
 * @brief Record the board initialization stage for later failure diagnosis.
 * @param[in] step Board initialization stage currently being executed.
 */
void bsp_board_set_init_step(BoardError_t step) {
    init_step = step;
}

/**
 * @brief Preserve the first board error or infer it from the recorded initialization stage.
 */
void bsp_board_record_error(void) {
    if (g_errorCode != BOARD_ERR_NONE) {
        return;
    }

    /* PLL already locked during the oscillator step: the clock tree setup failed. */
    if ((init_step == BOARD_ERR_OSCILLATOR) && ((RCC->CR & RCC_CR_PLLRDY) != 0U)) {
        g_errorCode = BOARD_ERR_CLOCK;
    } else {
        g_errorCode = init_step;
    }
}

/**
 * @brief Record a board failure and enter the non-returning error handler.
 * @param[in] error Board error code to retain for debugger inspection.
 */
void bsp_board_fail(BoardError_t error) {
    g_errorCode = error;
    Error_Handler();
}

/**
 * @brief Map CAN driver initialization errors to board-level failure codes.
 * @param[in] error CAN driver initialization result to translate.
 * @return BOARD_ERR_NONE for no error, or the corresponding board CAN failure code.
 */
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

/**
 * @brief Capture the overflowing task name and stop execution through the error handler.
 * @param[in] xTask Handle of the task whose stack overflowed; unused by this implementation.
 * @param[in] pcTaskName FreeRTOS task name copied into the diagnostic buffer.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    for (uint32_t i = 0; (i < sizeof(g_stackOverflowTask) - 1U) && (pcTaskName[i] != '\0'); i++) {
        g_stackOverflowTask[i] = pcTaskName[i];
    }
    bsp_board_fail(BOARD_ERR_STACK_OVERFLOW);
}
