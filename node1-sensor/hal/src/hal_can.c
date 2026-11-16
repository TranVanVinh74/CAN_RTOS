#include "hal_can.h"

#define CAN_MAX_DLC 8U

static CAN_HandleTypeDef *can_handle = NULL;

bool hal_can_init(CAN_HandleTypeDef *hcan) {
    if (hcan == NULL) {
        return false;
    }

    if (HAL_CAN_Start(hcan) != HAL_OK) {
        return false;
    }

    can_handle = hcan;
    return true;
}

bool hal_can_send(uint32_t std_id, const uint8_t *data, uint8_t len) {
    CAN_TxHeaderTypeDef header = {0};
    uint32_t mailbox;

    if ((can_handle == NULL) || (data == NULL) || (len > CAN_MAX_DLC)) {
        return false;
    }

    if (HAL_CAN_GetTxMailboxesFreeLevel(can_handle) == 0U) {
        return false;
    }

    header.StdId = std_id;
    header.ExtId = 0U;
    header.RTR = CAN_RTR_DATA;
    header.IDE = CAN_ID_STD;
    header.DLC = len;
    header.TransmitGlobalTime = DISABLE;

    if (HAL_CAN_AddTxMessage(can_handle, &header, data, &mailbox) != HAL_OK) {
        return false;
    }

    return true;
}
