#ifndef HAL_RELAY_H
#define HAL_RELAY_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

extern volatile uint32_t g_relayState;
extern volatile uint32_t g_relayPinLevel;
extern volatile uint32_t g_relaySwitchCount;

void hal_relay_init(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState on_level);
void hal_relay_set(bool on);
bool hal_relay_is_on(void);

#endif
