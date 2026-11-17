/**
 * @file hal_buttons.h
 * @brief Active-low button polling and debounced press events.
 */

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

/**
 * @brief Bind active-low button pins and initialize debounce states from their current levels.
 * @param[in] pins Input GPIO mapping for all BUTTON_COUNT buttons in Button_t order.
 */
void hal_buttons_init(const ButtonPin_t pins[BUTTON_COUNT]);
/**
 * @brief Debounce button inputs and report newly confirmed presses after 40 ms.
 * @param[in] now_ms Current monotonic time in milliseconds; unsigned subtraction handles wraparound.
 * @return Bitmask of new press events; bit i corresponds to Button_t value i.
 */
uint8_t hal_buttons_poll(uint32_t now_ms);

#endif
