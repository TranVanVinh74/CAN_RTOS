#include "hal_relay.h"

#include <stddef.h>

volatile uint32_t g_relayState = 0;
volatile uint32_t g_relayPinLevel = 0;
volatile uint32_t g_relaySwitchCount = 0;

static GPIO_TypeDef *relay_port = NULL;
static uint16_t relay_pin = 0;
static GPIO_PinState relay_on_level = GPIO_PIN_SET;

static GPIO_PinState off_level(void) {
    return (relay_on_level == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

void hal_relay_init(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState on_level) {
    relay_port = port;
    relay_pin = pin;
    relay_on_level = on_level;
    hal_relay_set(false);
}

void hal_relay_set(bool on) {
    if (relay_port == NULL) {
        return;
    }

    uint32_t next_state = on ? 1U : 0U;

    HAL_GPIO_WritePin(relay_port, relay_pin, on ? relay_on_level : off_level());

    if (g_relayState != next_state) {
        g_relaySwitchCount++;
    }
    g_relayState = next_state;
    g_relayPinLevel = ((relay_port->ODR & relay_pin) != 0U) ? 1U : 0U;
}

bool hal_relay_is_on(void) {
    return g_relayState != 0U;
}
