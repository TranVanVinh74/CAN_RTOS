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

bool hal_can_init(CAN_HandleTypeDef *hcan);
bool hal_can_send(uint32_t std_id, const uint8_t *data, uint8_t len);

#endif
