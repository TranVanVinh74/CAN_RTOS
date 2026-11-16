#ifndef HAL_A7680C_H
#define HAL_A7680C_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

void hal_a7680c_init(UART_HandleTypeDef *huart);
bool hal_a7680c_send(const char *text);
bool hal_a7680c_read_byte(uint8_t *byte);
void hal_a7680c_drain(void);
bool hal_a7680c_is_ready(void);
void hal_a7680c_irq_handler(void);

#endif
