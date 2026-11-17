/**
 * @file hal_settings_flash.h
 * @brief Read, erase, program and verify the reserved settings flash page.
 */

#ifndef HAL_SETTINGS_FLASH_H
#define HAL_SETTINGS_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Copy a settings record from the linker-reserved flash page.
 * @param[out] data Output buffer with room for words 16-bit halfwords.
 * @param[in] words Number of 16-bit halfwords to read.
 * @return true if the destination and record size are valid; this does not validate the record CRC.
 */
bool hal_settings_flash_read(uint16_t *data, size_t words);
/**
 * @brief Erase the settings page, write the record with commit words last, verify it and lock flash.
 * @param[in] data Input settings record; the first two halfwords are programmed last as commit markers.
 * @param[in] words Record length in 16-bit halfwords.
 * @return true if erase, programming, readback and relocking succeed; otherwise false.
 */
bool hal_settings_flash_write(const uint16_t *data, size_t words);

#endif
