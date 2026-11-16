#ifndef HAL_CAN_H
#define HAL_CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

#define HAL_CAN_MAX_DLC 8U

typedef enum {
    CAN_ERR_NONE,
    CAN_ERR_NO_HANDLE,
    CAN_ERR_FILTER,
    CAN_ERR_START,
    CAN_ERR_NOTIFICATION
} CanError_t;

void hal_can_init(CAN_HandleTypeDef *hcan);
CanError_t hal_can_start(void);
bool hal_can_send(uint32_t std_id, const uint8_t *data, uint8_t len);

#endif
