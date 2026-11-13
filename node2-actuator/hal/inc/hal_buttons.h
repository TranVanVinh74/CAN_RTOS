#ifndef HAL_BUTTONS_H
#define HAL_BUTTONS_H

#include <stdint.h>

#include "stm32f1xx_hal.h"

typedef enum {
    BUTTON_SELECT,
    BUTTON_PLUS,
    BUTTON_MINUS,
    BUTTON_SAVE,
    BUTTON_COUNT
} Button_t;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} ButtonPin_t;

void hal_buttons_init(const ButtonPin_t pins[BUTTON_COUNT]);
uint8_t hal_buttons_poll(uint32_t now_ms);

#endif
