/**
 * @file app_settings.h
 * @brief Persistent actuator thresholds with range validation and CRC32 checking.
 */

#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "alert_thresholds.h"

typedef enum {
    SETTINGS_UNCHANGED,
    SETTINGS_SAVED,
    SETTINGS_SAVE_ERROR,
    SETTINGS_INVALID
} SettingsSaveResult_t;

/**
 * @brief Load and validate saved settings, falling back to defaults for an invalid record.
 * @param[out] settings Output active settings; defaults are written when flash validation fails.
 * @return true if a valid flash record was loaded; false for invalid storage or a NULL output.
 */
bool app_settings_load(AlertThresholds_t *settings);
/**
 * @brief Save changed valid settings with a CRC32 and avoid redundant flash writes.
 * @param[in] settings Input gas and temperature limits to persist.
 * @return SETTINGS_SAVED, SETTINGS_UNCHANGED, SETTINGS_INVALID or SETTINGS_SAVE_ERROR.
 */
SettingsSaveResult_t app_settings_save(const AlertThresholds_t *settings);

#endif
