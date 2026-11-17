/**
 * @file hal_can.h
 * @brief CAN peripheral binding and frame transfer support.
 */

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

/**
 * @brief Bind a CAN handle without starting the peripheral.
 * @param[in] hcan Initialized CAN handle; call hal_can_start after application queues and callbacks are ready.
 */
void hal_can_init(CAN_HandleTypeDef *hcan);
/**
 * @brief Start transmit-only CAN without configuring an RX filter or RX interrupts.
 * @return CAN_ERR_NONE on success; CAN_ERR_NO_HANDLE or CAN_ERR_START on failure.
 */
CanError_t hal_can_start(void);
/**
 * @brief Load a standard data frame into a free CAN transmit mailbox without waiting for bus transmission.
 * @param[in] std_id Standard 11-bit CAN identifier; callers must use a value from 0x000 to 0x7FF.
 * @param[in] data Input payload containing at least len bytes.
 * @param[in] len Payload length in bytes, from 0 to 8.
 * @return true if HAL accepted the frame into a mailbox; false for invalid arguments, no free mailbox or HAL failure.
 */
bool hal_can_send(uint32_t std_id, const uint8_t *data, uint8_t len);

#endif
