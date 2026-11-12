#include "bsp_board.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

#include "hal_can.h"
#include "hal_dht11.h"
#include "hal_mq5.h"

extern ADC_HandleTypeDef hadc1;
extern CAN_HandleTypeDef hcan;
extern TIM_HandleTypeDef htim2;

volatile char g_stackOverflowTask[configMAX_TASK_NAME_LEN];

void bsp_board_init(void) {
    hal_dht11_init(&htim2, BSP_DHT11_PORT, BSP_DHT11_PIN);
    hal_mq5_init(&hadc1);
    hal_can_init(&hcan);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    for (uint32_t i = 0; i < (sizeof(g_stackOverflowTask) - 1U) && (pcTaskName[i] != '\0'); i++) {
        g_stackOverflowTask[i] = pcTaskName[i];
    }
    Error_Handler();
}
