/**
 * @file bsp_board.c
 * @brief Board pin/peripheral bindings, driver initialization and failure diagnostics.
 */

#include "bsp_board.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

#include "hal_a7680c.h"
#include "hal_can.h"

extern CAN_HandleTypeDef hcan;
extern UART_HandleTypeDef huart2;

volatile uint32_t g_errorCode = BOARD_ERR_NONE;
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
