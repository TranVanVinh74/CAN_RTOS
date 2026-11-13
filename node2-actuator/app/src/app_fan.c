#include "app_fan.h"
#include "sensor_status.h"

bool app_fan_should_run(bool running, uint16_t gas_raw, uint8_t temperature_c, uint8_t sensor_faults,
                        const AlertThresholds_t *thresholds) {
    return sensor_fan_should_run(running, gas_raw, temperature_c, sensor_faults, thresholds);
}
