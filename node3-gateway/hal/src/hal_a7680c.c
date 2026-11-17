/**
 * @file hal_a7680c.c
 * @brief A7680C AT transmission and interrupt-fed UART receive buffering.
 */

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

/**
 * @brief Advance a modem receive ring-buffer index with wraparound.
 * @param[in] index Current ring-buffer index.
 * @return Next valid ring-buffer index.
 */
static uint16_t next_index(uint16_t index) {
    return (uint16_t)((index + 1U) % MODEM_RX_BUFFER_LEN);
}

/**
 * @brief Transmit a modem command and poll its receive buffer for an expected substring.
 * @param[in] command Null-terminated AT command including its line terminator.
 * @param[in] expected Null-terminated response substring to search for.
 * @param[in] timeout_ms Maximum response wait in milliseconds.
 * @return true if the expected response appears before timeout; false on transmit failure or timeout.
 */
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

/**
 * @brief Bind the modem UART, clear the receive ring and enable receive-not-empty interrupts.
 * @param[in] huart Initialized UART handle connected to the A7680C modem.
 */
void hal_a7680c_init(UART_HandleTypeDef *huart) {
    modem_uart = huart;
    rx_head = 0;
    rx_tail = 0;

    if (huart == NULL) {
        return;
    }

    __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
}

/**
 * @brief Transmit a modem command synchronously through the bound UART.
 * @param[in] text Null-terminated command bytes, including any required CR/LF terminator.
 * @return true if the blocking HAL UART transmission succeeds; otherwise false.
 */
bool hal_a7680c_send(const char *text) {
    if (modem_uart == NULL) {
        return false;
    }

    return HAL_UART_Transmit(modem_uart, (const uint8_t *)text, (uint16_t)strlen(text),
                             MODEM_TX_TIMEOUT_MS) == HAL_OK;
}

/**
 * @brief Consume one byte from the interrupt-fed modem receive ring.
 * @param[out] byte Output pointer receiving the next byte; must not be NULL.
 * @return true if a byte was available and copied; false if the ring was empty.
 */
bool hal_a7680c_read_byte(uint8_t *byte) {
    uint16_t tail = rx_tail;

    if (tail == rx_head) {
        return false;
    }

    *byte = rx_buffer[tail];
    rx_tail = next_index(tail);
    return true;
}

/**
 * @brief Discard all bytes currently buffered from the modem.
 */
void hal_a7680c_drain(void) {
    rx_tail = rx_head;
}

/**
 * @brief Send AT and check for an OK response within one second.
 * @return true if AT receives OK; this does not verify SIM or network registration.
 */
bool hal_a7680c_is_ready(void) {
    if (modem_uart == NULL) {
        return false;
    }
    return send_and_wait(MODEM_CMD_AT, MODEM_REPLY_OK, MODEM_AT_TIMEOUT_MS);
}

/**
 * @brief Read UART receive/error flags and buffer incoming bytes, dropping bytes when the ring is full.
 */
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
