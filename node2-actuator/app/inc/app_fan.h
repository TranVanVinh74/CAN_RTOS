/**
 * @file app_fan.h
 * @brief Fault-aware fan control policy using shared hysteresis rules.
 */

#ifndef APP_FAN_H
#define APP_FAN_H

#include <stdbool.h>
#include <stdint.h>

#include "alert_thresholds.h"

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
                        const AlertThresholds_t *thresholds);

#endif
