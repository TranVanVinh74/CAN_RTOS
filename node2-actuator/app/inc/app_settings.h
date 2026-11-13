#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t gas_limit_raw;
    uint8_t temperature_limit_c;
} AppSettings_t;

typedef enum {
    SETTINGS_UNCHANGED,
    SETTINGS_SAVED,
    SETTINGS_SAVE_ERROR,
    SETTINGS_INVALID
} SettingsSaveResult_t;

extern volatile uint32_t g_settingsLoadValid;
extern volatile uint32_t g_settingsWriteCount;
extern volatile uint32_t g_settingsSaveErrors;
extern volatile SettingsSaveResult_t g_settingsLastSave;

bool app_settings_load(AppSettings_t *settings);
SettingsSaveResult_t app_settings_save(const AppSettings_t *settings);

#endif
