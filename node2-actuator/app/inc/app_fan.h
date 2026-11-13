#ifndef APP_FAN_H
#define APP_FAN_H

#include <stdbool.h>
#include <stdint.h>

#include "alert_thresholds.h"

bool app_fan_should_run(bool running, uint16_t gas_raw, uint8_t temperature_c,
                        const AlertThresholds_t *thresholds);

#endif
