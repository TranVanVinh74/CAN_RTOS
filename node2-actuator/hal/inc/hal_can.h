/**
 * @file hal_can.h
 * @brief CAN peripheral binding and frame transfer support.
 */

#ifndef HAL_CAN_H
#define HAL_CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "stm32f1xx_hal.h"

#define HAL_CAN_MAX_DLC 8U

typedef enum {
    CAN_ERR_NONE,
    CAN_ERR_NO_HANDLE,
    CAN_ERR_FILTER,
    CAN_ERR_START,
    CAN_ERR_NOTIFICATION
} CanError_t;

typedef struct {
    uint32_t std_id;
    uint8_t dlc;
    bool is_extended;
    bool is_remote;
    uint8_t data[HAL_CAN_MAX_DLC];
} CanRxFrame_t;

/**
 * @brief Process a CAN frame in FIFO0 interrupt context without blocking.
 * @param[in] frame Frame valid only during the callback; copy required data before returning.
 * @param[in,out] task_woken Shared wake flag for FreeRTOS FromISR queue operations.
 * @note The CAN driver performs the final portYIELD_FROM_ISR call.
 */
typedef void (*CanRxHandler_t)(const CanRxFrame_t *frame, BaseType_t *task_woken);

/**
 * @brief Bind a CAN handle without starting the peripheral.
 * @param[in] hcan Initialized CAN handle; call hal_can_start after application queues and callbacks are ready.
 */
void hal_can_init(CAN_HandleTypeDef *hcan);
/**
 * @brief Register the application callback invoked for frames read from CAN FIFO0.
 * @param[in] rx_handler Receive callback executed in interrupt context, or NULL to disable application dispatch.
 */
void hal_can_set_rx_handler(CanRxHandler_t rx_handler);
/**
 * @brief Configure CAN reception, start the peripheral and enable the FIFO0 pending interrupt.
 * @return CAN_ERR_NONE on success; otherwise the failed initialization stage.
 */
CanError_t hal_can_start(void);
/**
 * @brief Check CAN bus-off, error-passive state and transmit/receive error counters.
 * @return true if CAN is unavailable or in a bus fault state; otherwise false.
 */
bool hal_can_bus_fault(void);
/**
 * @brief Load a standard data frame into a free CAN transmit mailbox without waiting for bus transmission.
 * @param[in] std_id Standard 11-bit CAN identifier; callers must use a value from 0x000 to 0x7FF.
 * @param[in] data Input payload containing at least len bytes.
 * @param[in] len Payload length in bytes, from 0 to 8.
 * @return true if HAL accepted the frame into a mailbox; false for invalid arguments, no free mailbox or HAL failure.
 */
bool hal_can_send(uint32_t std_id, const uint8_t *data, uint8_t len);

#endif
