/**
 * @file hal_can.h
 * @brief CAN peripheral binding and frame transfer support.
 */

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

/**
 * @brief Start the transmit-only CAN driver and retain its peripheral handle; no receive filter is configured.
 * @param[in] hcan CAN peripheral handle initialized by CubeMX; must remain valid while the driver is used.
 * @return true if the CAN handle is valid and HAL_CAN_Start succeeds; otherwise false.
 */
bool hal_can_init(CAN_HandleTypeDef *hcan);
/**
 * @brief Load a standard data frame into a free CAN transmit mailbox without waiting for bus transmission.
 * @param[in] std_id Standard 11-bit CAN identifier; callers must use a value from 0x000 to 0x7FF.
 * @param[in] data Input payload containing at least len bytes.
 * @param[in] len Payload length in bytes, from 0 to 8.
 * @return true if HAL accepted the frame into a mailbox; false for invalid arguments, no free mailbox or HAL failure.
 */
bool hal_can_send(uint32_t std_id, const uint8_t *data, uint8_t len);

#endif
