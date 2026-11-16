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

bool app_settings_load(AlertThresholds_t *settings);
SettingsSaveResult_t app_settings_save(const AlertThresholds_t *settings);

#endif
