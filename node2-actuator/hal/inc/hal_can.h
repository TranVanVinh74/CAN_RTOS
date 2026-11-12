#ifndef HAL_CAN_H
#define HAL_CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "stm32f1xx_hal.h"

#define HAL_CAN_MAX_DLC 8U

typedef enum {
    CAN_ERR_NONE = 0,
    CAN_ERR_NO_HANDLE = 1,
    CAN_ERR_FILTER = 2,
    CAN_ERR_START = 3,
    CAN_ERR_NOTIFICATION = 4
} CanError_t;

typedef struct {
    uint32_t std_id;
    uint8_t dlc;
    bool is_extended;
    bool is_remote;
    uint8_t data[HAL_CAN_MAX_DLC];
} CanRxFrame_t;

typedef void (*CanRxHandler_t)(const CanRxFrame_t *frame, BaseType_t *task_woken);

extern volatile uint32_t g_canError;
extern volatile uint32_t g_canReadErrorCount;

void hal_can_init(CAN_HandleTypeDef *hcan);
void hal_can_set_rx_handler(CanRxHandler_t rx_handler);
CanError_t hal_can_start(void);
uint32_t hal_can_get_error(void);
bool hal_can_bus_fault(void);

#endif
