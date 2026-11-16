#include "app_settings.h"

#include <stddef.h>

#include "hal_settings_flash.h"

#define SETTINGS_MAGIC_LOW 0x4346U
#define SETTINGS_MAGIC_HIGH 0x4E32U
#define SETTINGS_VERSION 1U
#define SETTINGS_RECORD_WORDS 8U
#define SETTINGS_CHECKSUM_WORDS 6U
#define SETTINGS_CRC_POLYNOMIAL 0xEDB88320U

static AppSettings_t saved;
static bool retry_required;

static uint32_t checksum(const uint16_t words[SETTINGS_RECORD_WORDS]) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0U; i < SETTINGS_CHECKSUM_WORDS; i++) {
        for (uint8_t byte = 0U; byte < sizeof(uint16_t); byte++) {
            crc ^= (words[i] >> (byte * 8U)) & 0xFFU;
            for (uint8_t bit = 0U; bit < 8U; bit++) {
                crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? SETTINGS_CRC_POLYNOMIAL : 0U);
            }
        }
    }
    return ~crc;
}

bool app_settings_load(AppSettings_t *settings) {
    if (settings == NULL) {
        return false;
    }

    uint16_t words[SETTINGS_RECORD_WORDS];
    bool valid = hal_settings_flash_read(words, SETTINGS_RECORD_WORDS);
    AppSettings_t loaded = {
        .gas_limit_raw = ALERT_GAS_DEFAULT_RAW,
        .temperature_limit_c = ALERT_TEMPERATURE_DEFAULT_C,
    };
    if (valid) {
        uint32_t stored_crc = (uint32_t)words[6] | ((uint32_t)words[7] << 16U);
        valid = (words[0] == SETTINGS_MAGIC_LOW) && (words[1] == SETTINGS_MAGIC_HIGH) &&
                (words[2] == SETTINGS_VERSION) && (words[5] == 0U) &&
                (words[4] <= UINT8_MAX) && (checksum(words) == stored_crc);
        if (valid) {
            AppSettings_t candidate = {
                .gas_limit_raw = words[3],
                .temperature_limit_c = (uint8_t)words[4],
            };
            valid = alert_thresholds_valid(&candidate);
            if (valid) {
                loaded = candidate;
            }
        }
    }
    *settings = loaded;
    saved = loaded;
    retry_required = false;
    return valid;
}

SettingsSaveResult_t app_settings_save(const AppSettings_t *settings) {
    if (!alert_thresholds_valid(settings)) {
        return SETTINGS_INVALID;
    }
    if (!retry_required && (settings->gas_limit_raw == saved.gas_limit_raw) &&
        (settings->temperature_limit_c == saved.temperature_limit_c)) {
        return SETTINGS_UNCHANGED;
    }

    uint16_t words[SETTINGS_RECORD_WORDS] = {
        SETTINGS_MAGIC_LOW, SETTINGS_MAGIC_HIGH, SETTINGS_VERSION,
        settings->gas_limit_raw, settings->temperature_limit_c, 0U, 0U, 0U,
    };
    uint32_t crc = checksum(words);
    words[6] = (uint16_t)crc;
    words[7] = (uint16_t)(crc >> 16U);

    retry_required = true;
    if (!hal_settings_flash_write(words, SETTINGS_RECORD_WORDS)) {
        return SETTINGS_SAVE_ERROR;
    }
    saved = *settings;
    retry_required = false;
    return SETTINGS_SAVED;
}
