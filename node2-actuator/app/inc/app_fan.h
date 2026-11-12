#ifndef APP_FAN_H
#define APP_FAN_H

#include <stdbool.h>
#include <stdint.h>

bool app_fan_should_run(bool running, uint16_t gas_level);

#endif
