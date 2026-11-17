/**
 * @file hal_buttons.c
 * @brief Active-low button polling and debounced press events.
 */

#include "hal_buttons.h"

#include <stdbool.h>

#define BUTTON_DEBOUNCE_MS 40U

typedef struct {
    ButtonPin_t pin;
    uint32_t changed_ms;
    bool candidate_pressed;
    bool stable_pressed;
} ButtonState_t;

static ButtonState_t buttons[BUTTON_COUNT];

/**
 * @brief Bind active-low button pins and initialize debounce states from their current levels.
 * @param[in] pins Input GPIO mapping for all BUTTON_COUNT buttons in Button_t order.
 */
void hal_buttons_init(const ButtonPin_t pins[BUTTON_COUNT]) {
    for (uint8_t i = 0U; i < BUTTON_COUNT; i++) {
        bool pressed = HAL_GPIO_ReadPin(pins[i].port, pins[i].pin) == GPIO_PIN_RESET;
        buttons[i] = (ButtonState_t){
            .pin = pins[i],
            .candidate_pressed = pressed,
            .stable_pressed = pressed,
        };
    }
}

/**
 * @brief Debounce button inputs and report newly confirmed presses after 40 ms.
 * @param[in] now_ms Current monotonic time in milliseconds; unsigned subtraction handles wraparound.
 * @return Bitmask of new press events; bit i corresponds to Button_t value i.
 */
uint8_t hal_buttons_poll(uint32_t now_ms) {
    uint8_t events = 0U;

    for (uint8_t i = 0U; i < BUTTON_COUNT; i++) {
        ButtonState_t *button = &buttons[i];
        bool pressed = HAL_GPIO_ReadPin(button->pin.port, button->pin.pin) == GPIO_PIN_RESET;

        if (pressed != button->candidate_pressed) {
            button->candidate_pressed = pressed;
            button->changed_ms = now_ms;
        } else if ((pressed != button->stable_pressed) &&
                   ((uint32_t)(now_ms - button->changed_ms) >= BUTTON_DEBOUNCE_MS)) {
            button->stable_pressed = pressed;
            if (pressed) {
                events |= (uint8_t)(1U << i);
            }
        }
    }

    return events;
}
