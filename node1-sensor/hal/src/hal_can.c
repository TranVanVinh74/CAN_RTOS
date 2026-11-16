#include "hal_can.h"

#include <stddef.h>

#define CAN_STANDARD_ID_MAX 0x7FFU

static CAN_HandleTypeDef *can_handle = NULL;

void hal_can_init(CAN_HandleTypeDef *hcan) {
    can_handle = hcan;
}

CanError_t hal_can_start(void) {
    if (can_handle == NULL) {
        return CAN_ERR_NO_HANDLE;
    }

    if (HAL_CAN_Start(can_handle) != HAL_OK) {
        return CAN_ERR_START;
    }

    return CAN_ERR_NONE;
}

bool hal_can_send(uint32_t std_id, const uint8_t *data, uint8_t len) {
    if ((can_handle == NULL) || (data == NULL) || (std_id > CAN_STANDARD_ID_MAX) ||
        (len > HAL_CAN_MAX_DLC) || (HAL_CAN_GetTxMailboxesFreeLevel(can_handle) == 0U)) {
        return false;
    }

    CAN_TxHeaderTypeDef header = {0};
    uint32_t mailbox;

    header.StdId = std_id;
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = len;
    header.TransmitGlobalTime = DISABLE;
    return HAL_CAN_AddTxMessage(can_handle, &header, data, &mailbox) == HAL_OK;
}
