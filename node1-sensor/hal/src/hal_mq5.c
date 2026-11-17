/**
 * @file hal_mq5.c
 * @brief ADC calibration, MQ5 sampling and averaging diagnostics.
 */

#include "hal_mq5.h"

#define MQ5_ADC_TIMEOUT_MS 10U

static ADC_HandleTypeDef *mq5_adc = NULL;
static uint16_t mq5_last_value = 0;

/**
 * @brief Start, poll and stop a single MQ5 ADC conversion.
 * @param[out] value Non-NULL pointer receiving the raw ADC result on success.
 * @return true if conversion and stop succeed; otherwise false without updating the output.
 */
static bool read_sample(uint16_t *value) {
    if (mq5_adc == NULL) {
        return false;
    }

    if (HAL_ADC_Start(mq5_adc) != HAL_OK) {
        return false;
    }

    if (HAL_ADC_PollForConversion(mq5_adc, MQ5_ADC_TIMEOUT_MS) != HAL_OK) {
        (void)HAL_ADC_Stop(mq5_adc);
        return false;
    }

    uint16_t sample = (uint16_t)HAL_ADC_GetValue(mq5_adc);

    if (HAL_ADC_Stop(mq5_adc) != HAL_OK) {
        return false;
    }

    *value = sample;
    return true;
}

/**
 * @brief Calibrate the ADC, bind the MQ5 input and reset the retained reading.
 * @param[in] hadc Initialized ADC handle for the MQ5 analog channel.
 * @return true if a non-NULL ADC handle was calibrated successfully; otherwise false.
 */
bool hal_mq5_init(ADC_HandleTypeDef *hadc) {
    mq5_adc = NULL;
    mq5_last_value = 0;

    if ((hadc == NULL) || (HAL_ADCEx_Calibration_Start(hadc) != HAL_OK)) {
        return false;
    }

    mq5_adc = hadc;
    return true;
}

/**
 * @brief Average successful conversions while reporting whether every requested sample succeeded.
 * @param[in] samples Number of ADC conversions to attempt; zero is treated as one.
 * @param[out] gas_raw Non-NULL pointer receiving the average of successful samples, or the retained value if all conversions fail.
 * @return true only if all requested conversions succeeded; false if any conversion failed.
 */
bool hal_mq5_read_average(uint8_t samples, uint16_t *gas_raw) {
    uint32_t sum = 0;
    uint16_t valid_count = 0;

    if (samples == 0U) {
        samples = 1U;
    }

    for (uint16_t i = 0; i < samples; i++) {
        uint16_t sample;

        if (read_sample(&sample)) {
            sum += sample;
            valid_count++;
        }
    }

    if (valid_count > 0U) {
        mq5_last_value = (uint16_t)(sum / valid_count);
    }

    *gas_raw = mq5_last_value;
    /* One failed conversion flags the whole average: it no longer covers all samples. */
    return valid_count == samples;
}
