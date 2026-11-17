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
    CAN_ERR_NONE = 0,
    CAN_ERR_NO_HANDLE = 1,
    CAN_ERR_FILTER = 2,
    CAN_ERR_START = 3,
    CAN_ERR_NOTIFICATION = 4
} CanError_t;

typedef struct {
    uint32_t std_id;
    uint8_t dlc;
    bool is_extended;
    bool is_remote;
    uint8_t data[HAL_CAN_MAX_DLC];
} CanRxFrame_t;

/**
 * @brief Process one received CAN frame in the FIFO0 interrupt context.
 * @param[in] frame Frame valid only during the callback; copy any required data.
 * @param[in,out] task_woken Shared ISR wake flag for FreeRTOS FromISR queue calls.
 * @note Do not block; the CAN driver performs the final portYIELD_FROM_ISR call.
 */
typedef void (*CanRxHandler_t)(const CanRxFrame_t *frame, BaseType_t *task_woken);

extern volatile uint32_t g_canError;
extern volatile uint32_t g_canReadErrorCount;

/**
 * @brief Bind the driver to the CAN peripheral handle.
 * @param[in] hcan CAN peripheral handle initialized by CubeMX; must remain valid while the driver is used.
 */
void hal_can_init(CAN_HandleTypeDef *hcan);
/**
 * @brief Register the application callback invoked for frames read from CAN FIFO0.
 * @param[in] rx_handler Receive callback executed in interrupt context, or NULL to disable application dispatch.
 */
void hal_can_set_rx_handler(CanRxHandler_t rx_handler);
/**
 * @brief Configure the receive filter, start CAN and enable the FIFO0 message-pending interrupt.
 * @return CAN_ERR_NONE on success; otherwise the failing initialization stage.
 */
CanError_t hal_can_start(void);
/**
 * @brief Read the HAL CAN error flags.
 * @return HAL CAN error bitmask, or zero when no handle is bound.
 */
uint32_t hal_can_get_error(void);

#endif
