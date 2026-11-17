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

/**
 * @brief Apply alarm hysteresis; during sensor faults require a valid channel above its activation limit.
 * @param[in] running Previous active state used by hysteresis.
 * @param[in] gas_raw Raw MQ5 ADC reading, before any ppm conversion.
 * @param[in] temperature_c Temperature in whole degrees Celsius.
 * @param[in] faults Sensor fault bitmask: bit 0 for GAS and bit 1 for DHT.
 * @param[in] thresholds Input gas and temperature activation thresholds; must be valid for policy evaluation.
 * @return Next alarm state; faulty channels cannot trigger an alarm from retained readings.
 */
static inline bool sensor_alarm_should_run(bool running, uint16_t gas_raw,
                                          uint8_t temperature_c, uint8_t faults,
                                          const AlertThresholds_t *thresholds) {
    if (faults == 0U) {
        return alert_should_run(running, gas_raw, temperature_c, thresholds);
    }
    return sensor_above_limit(gas_raw, temperature_c, faults, thresholds);
}

#endif
