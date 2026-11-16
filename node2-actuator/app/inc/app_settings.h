#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "alert_thresholds.h"

typedef AlertThresholds_t AppSettings_t;

typedef enum {
    SETTINGS_UNCHANGED,
    SETTINGS_SAVED,
    SETTINGS_SAVE_ERROR,
    SETTINGS_INVALID
} SettingsSaveResult_t;

bool app_settings_load(AppSettings_t *settings);
SettingsSaveResult_t app_settings_save(const AppSettings_t *settings);

#endif
