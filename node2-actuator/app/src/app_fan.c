/**
 * @file app_fan.c
 * @brief Fault-aware fan control policy using shared hysteresis rules.
 */

#include "app_fan.h"
#include "sensor_status.h"

/**
 * @brief Evaluate the shared fault-aware fan policy for the actuator node.
 * @param[in] running Previous active state used by hysteresis.
 * @param[in] gas_raw Raw MQ5 ADC reading, before any ppm conversion.
 * @param[in] temperature_c Temperature in whole degrees Celsius.
 * @param[in] sensor_faults Sensor fault bitmask: bit 0 for GAS and bit 1 for DHT.
 * @param[in] thresholds Input gas and temperature activation thresholds; must be valid for policy evaluation.
 * @return Next fan state, including hysteresis and fault handling.
 */
bool app_fan_should_run(bool running, uint16_t gas_raw, uint8_t temperature_c, uint8_t sensor_faults,
                        const AlertThresholds_t *thresholds) {
    return sensor_fan_should_run(running, gas_raw, temperature_c, sensor_faults, thresholds);
}
