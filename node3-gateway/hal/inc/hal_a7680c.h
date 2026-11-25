/**
 * @file hal_a7680c.h
 * @brief A7680C AT transmission and interrupt-fed UART receive buffering.
 */

#ifndef HAL_A7680C_H
#define HAL_A7680C_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

/**
 * @brief Bind the modem UART, clear the receive ring and enable receive-not-empty interrupts.
 * @param[in] huart Initialized UART handle connected to the A7680C modem.
 */
void hal_a7680c_init(UART_HandleTypeDef *huart);
/**
 * @brief Transmit a modem command synchronously through the bound UART.
 * @param[in] text Null-terminated command bytes, including any required CR/LF terminator.
 * @return true if the blocking HAL UART transmission succeeds; otherwise false.
 */
bool hal_a7680c_send(const char *text);
/**
 * @brief Consume one byte from the interrupt-fed modem receive ring.
 * @param[out] byte Output pointer receiving the next byte; must not be NULL.
 * @return true if a byte was available and copied; false if the ring was empty.
 */
bool hal_a7680c_read_byte(uint8_t *byte);
/**
 * @brief Discard all bytes currently buffered from the modem.
 */
void hal_a7680c_drain(void);
/**
 * @brief Send AT and check for an OK response within one second.
 * @return true if AT receives OK; this does not verify SIM or network registration.
 */
bool hal_a7680c_is_ready(void);
/**
 * @brief Read UART receive/error flags and buffer incoming bytes, dropping bytes when the ring is full.
 */
void hal_a7680c_irq_handler(void);

void hal_a7680c_set_rx_task(void *task);

#endif
