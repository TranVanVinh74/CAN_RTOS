#include "hal_can.h"

#include <stddef.h>

#define CAN_ERROR_PASSIVE_COUNT 128U
#define CAN_ESR_COUNTER_MASK 0xFFU
#define CAN_ESR_TX_COUNTER_SHIFT 16U
#define CAN_ESR_RX_COUNTER_SHIFT 24U

#define CAN_FILTER_BANK 0U
#define CAN_SLAVE_START_FILTER_BANK 14U

volatile uint32_t g_canError = 0;
volatile uint32_t g_canReadErrorCount = 0;

static CAN_HandleTypeDef *can_handle = NULL;
static CanRxHandler_t can_rx_handler = NULL;

static HAL_StatusTypeDef config_accept_all_filter(void) {
    CAN_FilterTypeDef filter = {0};

    filter.FilterBank = CAN_FILTER_BANK;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = 0x0000U;
    filter.FilterIdLow = 0x0000U;
    filter.FilterMaskIdHigh = 0x0000U;
    filter.FilterMaskIdLow = 0x0000U;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = CAN_SLAVE_START_FILTER_BANK;
    return HAL_CAN_ConfigFilter(can_handle, &filter);
}

void hal_can_init(CAN_HandleTypeDef *hcan) {
    can_handle = hcan;
}

void hal_can_set_rx_handler(CanRxHandler_t rx_handler) {
    can_rx_handler = rx_handler;
}

CanError_t hal_can_start(void) {
    if (can_handle == NULL) {
        return CAN_ERR_NO_HANDLE;
    }

    if (config_accept_all_filter() != HAL_OK) {
        g_canError = HAL_CAN_GetError(can_handle);
        return CAN_ERR_FILTER;
    }

    if (HAL_CAN_Start(can_handle) != HAL_OK) {
        g_canError = HAL_CAN_GetError(can_handle);
        return CAN_ERR_START;
    }

    if (HAL_CAN_ActivateNotification(can_handle, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        g_canError = HAL_CAN_GetError(can_handle);
        return CAN_ERR_NOTIFICATION;
    }

    return CAN_ERR_NONE;
}

uint32_t hal_can_get_error(void) {
    return (can_handle != NULL) ? HAL_CAN_GetError(can_handle) : 0U;
}

bool hal_can_bus_fault(void) {
    if (can_handle == NULL) {
        return true;
    }
    uint32_t status = can_handle->Instance->ESR;
    uint32_t tx_errors = (status >> CAN_ESR_TX_COUNTER_SHIFT) & CAN_ESR_COUNTER_MASK;
    uint32_t rx_errors = (status >> CAN_ESR_RX_COUNTER_SHIFT) & CAN_ESR_COUNTER_MASK;
    return (status & (CAN_ESR_BOFF | CAN_ESR_EPVF)) != 0U ||
           tx_errors >= CAN_ERROR_PASSIVE_COUNT || rx_errors >= CAN_ERROR_PASSIVE_COUNT;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    BaseType_t task_woken = pdFALSE;

    if (hcan != can_handle) {
        return;
    }

    while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) > 0U) {
        CAN_RxHeaderTypeDef header;
        CanRxFrame_t frame;

        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, frame.data) != HAL_OK) {
            g_canReadErrorCount++;
            break;
        }

        frame.std_id = header.StdId;
        frame.dlc = (uint8_t)header.DLC;
        frame.is_extended = (header.IDE != CAN_ID_STD);
        frame.is_remote = (header.RTR != CAN_RTR_DATA);

        if (can_rx_handler != NULL) {
            can_rx_handler(&frame, &task_woken);
        }
    }

    portYIELD_FROM_ISR(task_woken);
}
