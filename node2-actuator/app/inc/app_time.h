#ifndef APP_TIME_H
#define APP_TIME_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

static inline uint32_t app_time_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static inline uint32_t app_time_ms_from_isr(void) {
    return (uint32_t)(xTaskGetTickCountFromISR() * portTICK_PERIOD_MS);
}

#endif
