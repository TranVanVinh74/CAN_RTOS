/**
 * @file bsp_board.h
 * @brief Board pin/peripheral bindings, driver initialization and failure diagnostics.
 */

#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "stm32f1xx_hal.h"

#define BSP_DHT11_PORT GPIOA
#define BSP_DHT11_PIN GPIO_PIN_2

/**
 * @brief Bind DHT11 to TIM2/PA2, initialize MQ5 on ADC1 and start CAN transmission.
 */
void bsp_board_init(void);

#endif
