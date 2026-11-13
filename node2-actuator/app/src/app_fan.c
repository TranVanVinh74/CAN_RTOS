#include "app_fan.h"

bool app_fan_should_run(bool running, uint16_t gas_raw, uint8_t temperature_c,
                        const AlertThresholds_t *thresholds) {
    return alert_should_run(running, gas_raw, temperature_c, thresholds);
}
