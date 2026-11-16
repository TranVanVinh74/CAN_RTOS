#include "hal_a7680c.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#define MODEM_TX_TIMEOUT_MS 1000U
#define MODEM_AT_TIMEOUT_MS 1000U
#define MODEM_AT_POLL_MS 10U
#define MODEM_AT_REPLY_LEN 128U
#define MODEM_RX_BUFFER_LEN 256U
#define MODEM_CMD_AT "AT\r\n"
#define MODEM_REPLY_OK "OK"

static UART_HandleTypeDef *modem_uart = NULL;
static uint8_t rx_buffer[MODEM_RX_BUFFER_LEN];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

static uint16_t next_index(uint16_t index) {
    return (uint16_t)((index + 1U) % MODEM_RX_BUFFER_LEN);
}

static bool send_and_wait(const char *command, const char *expected, uint32_t timeout_ms) {
    char reply[MODEM_AT_REPLY_LEN] = {0};
    uint16_t length = 0;

    if (!hal_a7680c_send(command)) {
        return false;
    }

    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        uint8_t ch;

        while (hal_a7680c_read_byte(&ch)) {
            if (length >= (sizeof(reply) - 1U)) {
                memmove(reply, reply + 1, sizeof(reply) - 2U);
                length = sizeof(reply) - 2U;
            }
            reply[length++] = (char)ch;
            reply[length] = '\0';
            if (strstr(reply, expected) != NULL) {
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(MODEM_AT_POLL_MS));
    }
    return false;
}

void hal_a7680c_init(UART_HandleTypeDef *huart) {
    modem_uart = huart;
    rx_head = 0;
    rx_tail = 0;

    if (huart == NULL) {
        return;
    }

    __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
}

bool hal_a7680c_send(const char *text) {
    if (modem_uart == NULL) {
        return false;
    }

    return HAL_UART_Transmit(modem_uart, (const uint8_t *)text, (uint16_t)strlen(text),
                             MODEM_TX_TIMEOUT_MS) == HAL_OK;
}

bool hal_a7680c_read_byte(uint8_t *byte) {
    uint16_t tail = rx_tail;

    if (tail == rx_head) {
        return false;
    }

    *byte = rx_buffer[tail];
    rx_tail = next_index(tail);
    return true;
}

void hal_a7680c_drain(void) {
    rx_tail = rx_head;
}

bool hal_a7680c_is_ready(void) {
    if (modem_uart == NULL) {
        return false;
    }
    return send_and_wait(MODEM_CMD_AT, MODEM_REPLY_OK, MODEM_AT_TIMEOUT_MS);
}

void hal_a7680c_irq_handler(void) {
    if (modem_uart == NULL) {
        return;
    }

    USART_TypeDef *uart = modem_uart->Instance;
    uint32_t status = uart->SR;

    if ((status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE | USART_SR_FE)) == 0U) {
        return;
    }

    /*
     * Reading DR clears RXNE and ORE. The ring buffer has one producer (this ISR, writes
     * rx_head) and one consumer (the COMM task, writes rx_tail).
     */
    uint8_t byte = (uint8_t)uart->DR;

    if ((status & USART_SR_RXNE) == 0U) {
        return;
    }

    uint16_t head = rx_head;
    uint16_t next = next_index(head);

    if (next == rx_tail) {
        return;
    }

    rx_buffer[head] = byte;
    rx_head = next;
}
