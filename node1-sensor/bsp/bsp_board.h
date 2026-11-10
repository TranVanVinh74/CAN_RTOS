#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "stm32f1xx_hal.h"

#define BSP_DHT11_PORT GPIOA
#define BSP_DHT11_PIN GPIO_PIN_2

void bsp_board_init(void);

#endif
