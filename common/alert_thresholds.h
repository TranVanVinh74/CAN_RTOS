/**
 * @file alert_thresholds.h
 * @brief Shared threshold defaults, validation and hysteresis policy.
 */

#ifndef ALERT_THRESHOLDS_H
#define ALERT_THRESHOLDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Widest usable ranges: each minimum keeps its switch-off level (limit - hysteresis) above 0,
 * otherwise the alarm could never clear; 4050 is the last gas step within the 12-bit ADC and
 * 50 C is the DHT11 measuring limit.
 */
#define ALERT_GAS_DEFAULT_RAW 2000U
#define ALERT_GAS_MIN_RAW 250U
#define ALERT_GAS_MAX_RAW 4050U
#define ALERT_GAS_STEP_RAW 50U
#define ALERT_GAS_HYSTERESIS_RAW 200U
#define ALERT_TEMPERATURE_DEFAULT_C 40U
#define ALERT_TEMPERATURE_MIN_C 3U
#define ALERT_TEMPERATURE_MAX_C 50U
#define ALERT_TEMPERATURE_STEP_C 1U
#define ALERT_TEMPERATURE_HYSTERESIS_C 2U

typedef struct {
    uint16_t gas_limit_raw;
    uint8_t temperature_limit_c;
} AlertThresholds_t;

/**
 * @brief Validate the supported gas range and step and the temperature range.
 * @param[in] thresholds Input gas and temperature thresholds to validate.
 * @return true if the pointer and threshold values are valid; otherwise false.
 */
static inline bool alert_thresholds_valid(const AlertThresholds_t *thresholds) {
    return (thresholds != NULL) &&
           (thresholds->gas_limit_raw >= ALERT_GAS_MIN_RAW) &&
           (thresholds->gas_limit_raw <= ALERT_GAS_MAX_RAW) &&
           ((thresholds->gas_limit_raw - ALERT_GAS_MIN_RAW) % ALERT_GAS_STEP_RAW == 0U) &&
           (thresholds->temperature_limit_c >= ALERT_TEMPERATURE_MIN_C) &&
           (thresholds->temperature_limit_c <= ALERT_TEMPERATURE_MAX_C);
}

/*
 * Hysteresis: switch on strictly above a limit, switch off only when both readings are
 * below their limit minus the hysteresis, otherwise keep the previous state. Readings
 * that hover around a limit must not toggle the fan or start a new call every frame.
 */
/**
 * @brief Apply OR activation and AND deactivation with gas and temperature hysteresis.
 * @param[in] running Previous active state used by hysteresis.
 * @param[in] gas_raw Raw MQ5 ADC reading, before any ppm conversion.
 * @param[in] temperature_c Temperature in whole degrees Celsius.
 * @param[in] thresholds Input gas and temperature activation thresholds; must be valid for policy evaluation.
 * @return Next active state; equality and the hysteresis band preserve the previous state.
 */
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
