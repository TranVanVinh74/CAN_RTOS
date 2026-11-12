#include "hal_a7680c.h"

#include <stddef.h>
#include <string.h>

#define MODEM_TX_TIMEOUT_MS 1000U
#define MODEM_DRAIN_WINDOW_MS 50U
#define MODEM_DRAIN_BYTE_TIMEOUT_MS 2U
#define MODEM_AT_TIMEOUT_MS 1000U
#define MODEM_AT_BYTE_TIMEOUT_MS 10U
#define MODEM_AT_REPLY_LEN 128U

volatile uint32_t g_uartTxStatus = HAL_OK;
volatile uint32_t g_uartError = 0;

static UART_HandleTypeDef *modem_uart = NULL;

static bool send_and_wait(const char *command, const char *expected, uint32_t timeout_ms) {
    char reply[MODEM_AT_REPLY_LEN] = {0};
    uint16_t length = 0;

    HAL_UART_Transmit(modem_uart, (const uint8_t *)command, (uint16_t)strlen(command),
                      MODEM_TX_TIMEOUT_MS);

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms) {
        uint8_t ch;

        if (HAL_UART_Receive(modem_uart, &ch, 1U, MODEM_AT_BYTE_TIMEOUT_MS) != HAL_OK) {
            continue;
        }
        if (length < (sizeof(reply) - 1U)) {
            reply[length++] = (char)ch;
            reply[length] = '\0';
            if (strstr(reply, expected) != NULL) {
                return true;
            }
        }
    }
    return false;
}

void hal_a7680c_init(UART_HandleTypeDef *huart) {
    modem_uart = huart;
}

bool hal_a7680c_send(const char *text) {
    if (modem_uart == NULL) {
        return false;
    }

    HAL_StatusTypeDef status = HAL_UART_Transmit(modem_uart, (const uint8_t *)text,
                                                 (uint16_t)strlen(text), MODEM_TX_TIMEOUT_MS);

    g_uartTxStatus = (uint32_t)status;
    g_uartError = HAL_UART_GetError(modem_uart);
    return status == HAL_OK;
}

ModemRead_t hal_a7680c_read_byte(uint8_t *byte, uint32_t timeout_ms) {
    if (modem_uart == NULL) {
        return MODEM_READ_ERROR;
    }

    HAL_StatusTypeDef status = HAL_UART_Receive(modem_uart, byte, 1U, timeout_ms);

    if (status == HAL_OK) {
        return MODEM_READ_BYTE;
    }
    if (status == HAL_ERROR) {
        g_uartError = HAL_UART_GetError(modem_uart);
        __HAL_UART_CLEAR_OREFLAG(modem_uart);
        return MODEM_READ_ERROR;
    }
    return MODEM_READ_NONE;
}

void hal_a7680c_drain(void) {
    if (modem_uart == NULL) {
        return;
    }

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < MODEM_DRAIN_WINDOW_MS) {
        uint8_t byte;

        if (HAL_UART_Receive(modem_uart, &byte, 1U, MODEM_DRAIN_BYTE_TIMEOUT_MS) != HAL_OK) {
            break;
        }
    }

    __HAL_UART_CLEAR_OREFLAG(modem_uart);
}

bool hal_a7680c_is_ready(void) {
    if (modem_uart == NULL) {
        return false;
    }
    return send_and_wait("AT\r\n", "OK", MODEM_AT_TIMEOUT_MS);
}
