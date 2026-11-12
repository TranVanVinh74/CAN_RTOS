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

void bsp_board_init(void) {
    hal_can_init(&hcan);
    hal_a7680c_init(&huart2);
}

void bsp_board_fail(BoardError_t error) {
    g_errorCode = error;
    Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    for (uint32_t i = 0; i < (sizeof(g_stackOverflowTask) - 1U) && (pcTaskName[i] != '\0'); i++) {
        g_stackOverflowTask[i] = pcTaskName[i];
    }
    bsp_board_fail(BOARD_ERR_STACK_OVERFLOW);
}
