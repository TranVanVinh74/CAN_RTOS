/**
 * @file hal_can.c
 * @brief CAN peripheral binding and frame transfer support.
 */

#include "hal_can.h"

#define CAN_MAX_DLC 8U

volatile uint8_t g_canError = CAN_ERR_NONE;
volatile uint32_t g_canTxCount = 0;
volatile uint32_t g_canTxFailCount = 0;

static CAN_HandleTypeDef *can_handle = NULL;

/**
 * @brief Start the transmit-only CAN driver and retain its peripheral handle; no receive filter is configured.
 * @param[in] hcan CAN peripheral handle initialized by CubeMX; must remain valid while the driver is used.
 * @return true if the CAN handle is valid and HAL_CAN_Start succeeds; otherwise false.
 */
bool hal_can_init(CAN_HandleTypeDef *hcan) {
    if (hcan == NULL) {
        g_canError = CAN_ERR_NO_HANDLE;
        return false;
    }

    if (HAL_CAN_Start(hcan) != HAL_OK) {
        g_canError = CAN_ERR_START;
        return false;
    }

    can_handle = hcan;
    g_canError = CAN_ERR_NONE;
    return true;
}

/**
 * @brief Load a standard data frame into a free CAN transmit mailbox without waiting for bus transmission.
 * @param[in] std_id Standard 11-bit CAN identifier; callers must use a value from 0x000 to 0x7FF.
 * @param[in] data Input payload containing at least len bytes.
 * @param[in] len Payload length in bytes, from 0 to 8.
 * @return true if HAL accepted the frame into a mailbox; false for invalid arguments, no free mailbox or HAL failure.
 */
bool hal_can_send(uint32_t std_id, const uint8_t *data, uint8_t len) {
    CAN_TxHeaderTypeDef header = {0};
    uint32_t mailbox;

    if ((can_handle == NULL) || (data == NULL) || (len > CAN_MAX_DLC)) {
        g_canTxFailCount++;
        return false;
    }

    if (HAL_CAN_GetTxMailboxesFreeLevel(can_handle) == 0U) {
        g_canTxFailCount++;
        return false;
    }

    header.StdId = std_id;
    header.ExtId = 0U;
    header.RTR = CAN_RTR_DATA;
    header.IDE = CAN_ID_STD;
    header.DLC = len;
    header.TransmitGlobalTime = DISABLE;

    if (HAL_CAN_AddTxMessage(can_handle, &header, data, &mailbox) != HAL_OK) {
        g_canTxFailCount++;
        return false;
    }

    g_canTxCount++;
    return true;
}
