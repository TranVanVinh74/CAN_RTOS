#include "app_fan.h"

#include "app_cfg.h"

bool app_fan_should_run(bool running, uint16_t gas_level) {
    if (gas_level > FAN_GAS_ON_THRESHOLD) {
        return true;
    }
    if (gas_level < FAN_GAS_OFF_THRESHOLD) {
        return false;
    }
    return running;
}
