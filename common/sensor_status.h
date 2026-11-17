/**
 * @file sensor_status.h
 * @brief Sensor fault flags and validity-aware fan and alarm policies.
 */

#ifndef SENSOR_STATUS_H
#define SENSOR_STATUS_H

#include "alert_thresholds.h"

#define SENSOR_FAULT_GAS 0x01U
#define SENSOR_FAULT_DHT 0x02U
#define SENSOR_FAULT_MASK (SENSOR_FAULT_GAS | SENSOR_FAULT_DHT)

/**
 * @brief Reject reserved sensor fault bits outside the GAS and DHT flags.
 * @param[in] faults Sensor fault bitmask: bit 0 for GAS and bit 1 for DHT.
 * @return true if only defined fault bits are set; otherwise false.
 */
static inline bool sensor_faults_valid(uint8_t faults) {
    return (faults & (uint8_t)~SENSOR_FAULT_MASK) == 0U;
}

/**
 * @brief Check whether any valid sensor channel exceeds its activation threshold.
 * @param[in] gas_raw Raw MQ5 ADC reading, before any ppm conversion.
 * @param[in] temperature_c Temperature in whole degrees Celsius.
 * @param[in] faults Sensor fault bitmask: bit 0 for GAS and bit 1 for DHT.
 * @param[in] thresholds Input gas and temperature activation thresholds; must be valid for policy evaluation.
 * @return true if at least one non-faulty channel exceeds its limit; otherwise false.
 */
static inline bool sensor_above_limit(uint16_t gas_raw, uint8_t temperature_c,
                                      uint8_t faults, const AlertThresholds_t *thresholds) {
    return (((faults & SENSOR_FAULT_GAS) == 0U) &&
            (gas_raw > thresholds->gas_limit_raw)) ||
           (((faults & SENSOR_FAULT_DHT) == 0U) &&
            (temperature_c > thresholds->temperature_limit_c));
}

/* With a sensor fault the fan never switches off: a missing reading cannot prove safety. */
/**
 * @brief Apply fan hysteresis and retain an active fan while sensor faults prevent safe deactivation.
 * @param[in] running Previous active state used by hysteresis.
 * @param[in] gas_raw Raw MQ5 ADC reading, before any ppm conversion.
 * @param[in] temperature_c Temperature in whole degrees Celsius.
 * @param[in] faults Sensor fault bitmask: bit 0 for GAS and bit 1 for DHT.
 * @param[in] thresholds Input gas and temperature activation thresholds; must be valid for policy evaluation.
 * @return Next fan state based on valid channels and the previous state.
 */
static inline bool sensor_fan_should_run(bool running, uint16_t gas_raw,
                                        uint8_t temperature_c, uint8_t faults,
                                        const AlertThresholds_t *thresholds) {
    if (faults == 0U) {
        return alert_should_run(running, gas_raw, temperature_c, thresholds);
    }
    return running || sensor_above_limit(gas_raw, temperature_c, faults, thresholds);
}

/*
 * With a sensor fault only the valid channels count. A valid reading above its limit
 * raises the call alarm. A running alarm keeps the hysteresis on the valid channels: it
 * stops only when every valid reading is below its switch-off level, so a single faulty
 * frame or a reading just under the limit does not hang up a call. A faulty channel never
 * holds the alarm on, so two faulty channels stop it: held readings cannot keep a call.
 */
/**
 * @brief Apply alarm hysteresis to valid channels; faulty channels cannot hold an alarm active.
 * @param[in] running Previous alarm state.
 * @param[in] gas_raw Raw gas ADC reading.
 * @param[in] temperature_c Temperature in whole degrees Celsius.
 * @param[in] faults Sensor fault bitmask: GAS bit 0 and DHT bit 1.
 * @param[in] thresholds Non-NULL pointer to valid gas and temperature activation thresholds.
 * @return Next alarm state; with faults, clear only when all valid channels are below their off thresholds, or when both channels are faulty.
 */
static inline bool sensor_alarm_should_run(bool running, uint16_t gas_raw,
                                          uint8_t temperature_c, uint8_t faults,
                                          const AlertThresholds_t *thresholds) {
    if (faults == 0U) {
        return alert_should_run(running, gas_raw, temperature_c, thresholds);
    }
    if (sensor_above_limit(gas_raw, temperature_c, faults, thresholds)) {
        return true;
    }
    bool gas_below_off = ((faults & SENSOR_FAULT_GAS) != 0U) ||
                         (gas_raw < thresholds->gas_limit_raw - ALERT_GAS_HYSTERESIS_RAW);
    bool temperature_below_off =
        ((faults & SENSOR_FAULT_DHT) != 0U) ||
        (temperature_c < thresholds->temperature_limit_c - ALERT_TEMPERATURE_HYSTERESIS_C);
    return running && !(gas_below_off && temperature_below_off);
}

#endif
