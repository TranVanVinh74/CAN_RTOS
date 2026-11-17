/**
 * @file hal_settings_flash.c
 * @brief Read, erase, program and verify the reserved settings flash page.
 */

#include "hal_settings_flash.h"

#include "stm32f1xx_hal.h"

#define SETTINGS_PAGE_BYTES 1024U
#define SETTINGS_COMMIT_WORDS 2U

extern const uint16_t __settings_start__[];
extern const uint16_t __settings_end__[];

/**
 * @brief Check the linker-reserved settings page size and requested record length.
 * @param[in] words Record length in 16-bit halfwords.
 * @return true if the reserved page is 1 KB and the record fits with its commit words.
 */
static bool valid_size(size_t words) {
    return ((uintptr_t)__settings_end__ - (uintptr_t)__settings_start__ ==
            SETTINGS_PAGE_BYTES) &&
           (words >= SETTINGS_COMMIT_WORDS) &&
           (words <= SETTINGS_PAGE_BYTES / sizeof(uint16_t));
}

/**
 * @brief Copy a settings record from the linker-reserved flash page.
 * @param[out] data Output buffer with room for words 16-bit halfwords.
 * @param[in] words Number of 16-bit halfwords to read.
 * @return true if the destination and record size are valid; this does not validate the record CRC.
 */
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

/**
 * @brief Erase the settings page, write the record with commit words last, verify it and lock flash.
 * @param[in] data Input settings record; the first two halfwords are programmed last as commit markers.
 * @param[in] words Record length in 16-bit halfwords.
 * @return true if erase, programming, readback and relocking succeed; otherwise false.
 */
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

    /*
     * The first words hold the record magic and are programmed last. A reset during
     * erase or programming leaves them erased, so the record is rejected at boot.
     */
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
