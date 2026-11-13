#ifndef SENSOR_STATUS_H
#define SENSOR_STATUS_H

#include "alert_thresholds.h"

#define SENSOR_FAULT_GAS 0x01U
#define SENSOR_FAULT_DHT 0x02U
#define SENSOR_FAULT_MASK (SENSOR_FAULT_GAS | SENSOR_FAULT_DHT)

static inline bool sensor_faults_valid(uint8_t faults) {
    return (faults & (uint8_t)~SENSOR_FAULT_MASK) == 0U;
}

static inline bool sensor_above_limit(uint16_t gas_raw, uint8_t temperature_c,
                                      uint8_t faults, const AlertThresholds_t *thresholds) {
    return (((faults & SENSOR_FAULT_GAS) == 0U) &&
            (gas_raw > thresholds->gas_limit_raw)) ||
           (((faults & SENSOR_FAULT_DHT) == 0U) &&
            (temperature_c > thresholds->temperature_limit_c));
}

static inline bool sensor_fan_should_run(bool running, uint16_t gas_raw,
                                        uint8_t temperature_c, uint8_t faults,
                                        const AlertThresholds_t *thresholds) {
    if (faults == 0U) {
        return alert_should_run(running, gas_raw, temperature_c, thresholds);
    }
    return running || sensor_above_limit(gas_raw, temperature_c, faults, thresholds);
}

static inline bool sensor_alarm_should_run(bool running, uint16_t gas_raw,
                                          uint8_t temperature_c, uint8_t faults,
                                          const AlertThresholds_t *thresholds) {
    if (faults == 0U) {
        return alert_should_run(running, gas_raw, temperature_c, thresholds);
    }
    return sensor_above_limit(gas_raw, temperature_c, faults, thresholds);
}

#endif
