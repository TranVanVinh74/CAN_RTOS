#ifndef ALERT_THRESHOLDS_H
#define ALERT_THRESHOLDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ALERT_GAS_DEFAULT_RAW 2000U
#define ALERT_GAS_MIN_RAW 500U
#define ALERT_GAS_MAX_RAW 3800U
#define ALERT_GAS_STEP_RAW 50U
#define ALERT_GAS_HYSTERESIS_RAW 200U
#define ALERT_TEMPERATURE_DEFAULT_C 40U
#define ALERT_TEMPERATURE_MIN_C 30U
#define ALERT_TEMPERATURE_MAX_C 50U
#define ALERT_TEMPERATURE_STEP_C 1U
#define ALERT_TEMPERATURE_HYSTERESIS_C 2U

typedef struct {
    uint16_t gas_limit_raw;
    uint8_t temperature_limit_c;
} AlertThresholds_t;

static inline bool alert_thresholds_valid(const AlertThresholds_t *thresholds) {
    return (thresholds != NULL) &&
           (thresholds->gas_limit_raw >= ALERT_GAS_MIN_RAW) &&
           (thresholds->gas_limit_raw <= ALERT_GAS_MAX_RAW) &&
           ((thresholds->gas_limit_raw - ALERT_GAS_MIN_RAW) % ALERT_GAS_STEP_RAW == 0U) &&
           (thresholds->temperature_limit_c >= ALERT_TEMPERATURE_MIN_C) &&
           (thresholds->temperature_limit_c <= ALERT_TEMPERATURE_MAX_C);
}

static inline bool alert_should_run(bool running, uint16_t gas_raw, uint8_t temperature_c,
                                   const AlertThresholds_t *thresholds) {
    if ((gas_raw > thresholds->gas_limit_raw) ||
        (temperature_c > thresholds->temperature_limit_c)) {
        return true;
    }
    if ((gas_raw < thresholds->gas_limit_raw - ALERT_GAS_HYSTERESIS_RAW) &&
        (temperature_c < thresholds->temperature_limit_c - ALERT_TEMPERATURE_HYSTERESIS_C)) {
        return false;
    }
    return running;
}

#endif
