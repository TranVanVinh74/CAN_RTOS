#include "hal_mq5.h"

#define MQ5_ADC_TIMEOUT_MS 10U

static ADC_HandleTypeDef *mq5_adc = NULL;
static uint16_t mq5_last_value = 0;

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

bool hal_mq5_init(ADC_HandleTypeDef *hadc) {
    mq5_adc = NULL;
    mq5_last_value = 0;

    if ((hadc == NULL) || (HAL_ADCEx_Calibration_Start(hadc) != HAL_OK)) {
        return false;
    }

    mq5_adc = hadc;
    return true;
}

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
    return valid_count == samples;
}
