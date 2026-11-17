/**
 * @file hal_can.c
 * @brief CAN peripheral binding and frame transfer support.
 */

#include "hal_can.h"

#include <stddef.h>

#define CAN_STANDARD_ID_MAX 0x7FFU

static CAN_HandleTypeDef *can_handle = NULL;

/**
 * @brief Bind a CAN handle without starting the peripheral.
 * @param[in] hcan Initialized CAN handle; call hal_can_start after application queues and callbacks are ready.
 */
void hal_can_init(CAN_HandleTypeDef *hcan) {
    can_handle = hcan;
}

/*
 * Node1 only transmits, so no RX filter and no RX interrupt are configured. Enabling
 * FIFO0 reception without a reader keeps the interrupt pending and starves both tasks.
 */
/**
 * @brief Start transmit-only CAN without configuring an RX filter or RX interrupts.
 * @return CAN_ERR_NONE on success; CAN_ERR_NO_HANDLE or CAN_ERR_START on failure.
 */
CanError_t hal_can_start(void) {
    if (can_handle == NULL) {
        return CAN_ERR_NO_HANDLE;
    }

    if (HAL_CAN_Start(can_handle) != HAL_OK) {
        return CAN_ERR_START;
    }

    return CAN_ERR_NONE;
}

/**
 * @brief Load a standard data frame into a free CAN transmit mailbox without waiting for bus transmission.
 * @param[in] std_id Standard 11-bit CAN identifier; callers must use a value from 0x000 to 0x7FF.
 * @param[in] data Input payload containing at least len bytes.
 * @param[in] len Payload length in bytes, from 0 to 8.
 * @return true if HAL accepted the frame into a mailbox; false for invalid arguments, no free mailbox or HAL failure.
 */
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
