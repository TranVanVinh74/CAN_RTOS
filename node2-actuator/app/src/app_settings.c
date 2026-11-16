#include "app_settings.h"

#include <stddef.h>

#include "hal_settings_flash.h"

#define SETTINGS_MAGIC_LOW 0x4346U
#define SETTINGS_MAGIC_HIGH 0x4E32U
#define SETTINGS_VERSION 1U
#define SETTINGS_RESERVED 0U
#define SETTINGS_CRC_POLYNOMIAL 0xEDB88320U
#define SETTINGS_BITS_PER_BYTE 8U
#define SETTINGS_BYTE_MASK 0xFFU
#define SETTINGS_WORD_BITS 16U

typedef enum {
    SETTINGS_WORD_MAGIC_LOW,
    SETTINGS_WORD_MAGIC_HIGH,
    SETTINGS_WORD_VERSION,
    SETTINGS_WORD_GAS,
    SETTINGS_WORD_TEMPERATURE,
    SETTINGS_WORD_RESERVED,
    SETTINGS_WORD_CRC_LOW,
    SETTINGS_WORD_CRC_HIGH,
    SETTINGS_RECORD_WORDS
} SettingsWord_t;

static AppSettings_t saved;
static bool retry_required;

static uint32_t checksum(const uint16_t words[SETTINGS_RECORD_WORDS]) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0U; i < SETTINGS_WORD_CRC_LOW; i++) {
        for (uint8_t byte = 0U; byte < sizeof(uint16_t); byte++) {
            crc ^= (words[i] >> (byte * SETTINGS_BITS_PER_BYTE)) & SETTINGS_BYTE_MASK;
            for (uint8_t bit = 0U; bit < SETTINGS_BITS_PER_BYTE; bit++) {
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
        uint32_t stored_crc = (uint32_t)words[SETTINGS_WORD_CRC_LOW] |
                              ((uint32_t)words[SETTINGS_WORD_CRC_HIGH] << SETTINGS_WORD_BITS);
        valid = (words[SETTINGS_WORD_MAGIC_LOW] == SETTINGS_MAGIC_LOW) &&
                (words[SETTINGS_WORD_MAGIC_HIGH] == SETTINGS_MAGIC_HIGH) &&
                (words[SETTINGS_WORD_VERSION] == SETTINGS_VERSION) &&
                (words[SETTINGS_WORD_RESERVED] == SETTINGS_RESERVED) &&
                (words[SETTINGS_WORD_TEMPERATURE] <= UINT8_MAX) &&
                (checksum(words) == stored_crc);
        if (valid) {
            AppSettings_t candidate = {
                .gas_limit_raw = words[SETTINGS_WORD_GAS],
                .temperature_limit_c = (uint8_t)words[SETTINGS_WORD_TEMPERATURE],
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
        [SETTINGS_WORD_MAGIC_LOW] = SETTINGS_MAGIC_LOW,
        [SETTINGS_WORD_MAGIC_HIGH] = SETTINGS_MAGIC_HIGH,
        [SETTINGS_WORD_VERSION] = SETTINGS_VERSION,
        [SETTINGS_WORD_GAS] = settings->gas_limit_raw,
        [SETTINGS_WORD_TEMPERATURE] = settings->temperature_limit_c,
        [SETTINGS_WORD_RESERVED] = SETTINGS_RESERVED,
    };
    uint32_t crc = checksum(words);
    words[SETTINGS_WORD_CRC_LOW] = (uint16_t)crc;
    words[SETTINGS_WORD_CRC_HIGH] = (uint16_t)(crc >> SETTINGS_WORD_BITS);

    retry_required = true;
    if (!hal_settings_flash_write(words, SETTINGS_RECORD_WORDS)) {
        return SETTINGS_SAVE_ERROR;
    }
    saved = *settings;
    retry_required = false;
    return SETTINGS_SAVED;
}
