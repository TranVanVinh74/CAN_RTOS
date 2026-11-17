/**
 * @file app_time.h
 * @brief Task-context and ISR-safe millisecond clock helpers.
 */

#ifndef APP_TIME_H
#define APP_TIME_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief Convert the FreeRTOS task-context tick count to milliseconds.
 * @return Current scheduler time in milliseconds, with unsigned wraparound.
 */
static inline uint32_t app_time_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/**
 * @brief Convert the ISR-safe FreeRTOS tick count to milliseconds.
 * @return Current scheduler time in milliseconds, with unsigned wraparound.
 */
static inline uint32_t app_time_ms_from_isr(void) {
    return (uint32_t)(xTaskGetTickCountFromISR() * portTICK_PERIOD_MS);
}

#endif
