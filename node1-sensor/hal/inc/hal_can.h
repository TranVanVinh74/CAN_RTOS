#ifndef HAL_CAN_H
#define HAL_CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

typedef enum {
    CAN_ERR_NONE = 0,
    CAN_ERR_NO_HANDLE = 1,
    CAN_ERR_START = 2
} CanError_t;

extern volatile uint8_t g_canError;
extern volatile uint32_t g_canTxCount;
extern volatile uint32_t g_canTxFailCount;

bool hal_can_init(CAN_HandleTypeDef *hcan);
bool hal_can_send(uint32_t std_id, const uint8_t *data, uint8_t len);

#endif
