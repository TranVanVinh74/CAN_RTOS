#ifndef HAL_A7680C_H
#define HAL_A7680C_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

typedef enum {
    MODEM_READ_NONE = 0,
    MODEM_READ_BYTE = 1,
    MODEM_READ_ERROR = 2
} ModemRead_t;

extern volatile uint32_t g_uartTxStatus;
extern volatile uint32_t g_uartError;

void hal_a7680c_init(UART_HandleTypeDef *huart);
bool hal_a7680c_send(const char *text);
ModemRead_t hal_a7680c_read_byte(uint8_t *byte, uint32_t timeout_ms);
void hal_a7680c_drain(void);
bool hal_a7680c_is_ready(void);

#endif
