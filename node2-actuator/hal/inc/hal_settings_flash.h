#ifndef HAL_SETTINGS_FLASH_H
#define HAL_SETTINGS_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool hal_settings_flash_read(uint16_t *data, size_t words);
bool hal_settings_flash_write(const uint16_t *data, size_t words);

#endif
