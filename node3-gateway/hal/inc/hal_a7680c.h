/**
 * @file hal_a7680c.h
 * @brief A7680C AT transmission and interrupt-fed UART receive buffering.
 */

#ifndef HAL_A7680C_H
#define HAL_A7680C_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

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
 * @brief Read UART receive/error flags and buffer incoming bytes.
 * @note Bytes are dropped while the ring is full.
 */
void hal_a7680c_irq_handler(void);
/**
 * @brief Select the task notified from the receive ISR for every buffered byte.
 * @param[in] task Single ring-buffer consumer task, or NULL to stop notifications.
 */
void hal_a7680c_set_rx_task(TaskHandle_t task);

#endif
