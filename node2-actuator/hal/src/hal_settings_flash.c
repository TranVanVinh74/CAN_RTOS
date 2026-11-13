#include "hal_settings_flash.h"

#include "stm32f1xx_hal.h"

#define SETTINGS_PAGE_BYTES 1024U
#define SETTINGS_COMMIT_WORDS 2U

extern const uint16_t __settings_start__[];
extern const uint16_t __settings_end__[];

static bool valid_size(size_t words) {
    return ((uintptr_t)__settings_end__ - (uintptr_t)__settings_start__ ==
            SETTINGS_PAGE_BYTES) &&
           (words >= SETTINGS_COMMIT_WORDS) &&
           (words <= SETTINGS_PAGE_BYTES / sizeof(uint16_t));
}

bool hal_settings_flash_read(uint16_t *data, size_t words) {
    if ((data == NULL) || !valid_size(words)) {
        return false;
    }

    const volatile uint16_t *flash = __settings_start__;
    for (size_t i = 0U; i < words; i++) {
        data[i] = flash[i];
    }
    return true;
}

bool hal_settings_flash_write(const uint16_t *data, size_t words) {
    if ((data == NULL) || !valid_size(words)) {
        return false;
    }

    if (HAL_FLASH_Unlock() != HAL_OK) {
        (void)HAL_FLASH_Lock();
        return false;
    }

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .PageAddress = (uint32_t)(uintptr_t)__settings_start__,
        .NbPages = 1U,
    };
    uint32_t page_error = UINT32_MAX;
    bool ok = HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK;

    for (size_t i = SETTINGS_COMMIT_WORDS; ok && (i < words); i++) {
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              (uint32_t)(uintptr_t)&__settings_start__[i], data[i]) == HAL_OK;
    }
    for (size_t i = 0U; ok && (i < SETTINGS_COMMIT_WORDS); i++) {
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              (uint32_t)(uintptr_t)&__settings_start__[i], data[i]) == HAL_OK;
    }

    const volatile uint16_t *flash = __settings_start__;
    for (size_t i = 0U; ok && (i < words); i++) {
        ok = flash[i] == data[i];
    }
    if (HAL_FLASH_Lock() != HAL_OK) {
        ok = false;
    }
    return ok;
}
