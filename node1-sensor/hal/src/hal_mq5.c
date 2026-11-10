#include "hal_mq5.h"

#define MQ5_ADC_TIMEOUT_MS 10U

volatile uint8_t g_mq5Error = MQ5_ERR_NONE;
volatile uint32_t g_mq5ReadErrors = 0;

static ADC_HandleTypeDef *mq5_adc = NULL;
static uint16_t mq5_last_value = 0;

static void record_read_error(Mq5Error_t error) {
    g_mq5Error = error;
    g_mq5ReadErrors++;
}

static HAL_StatusTypeDef read_sample(uint16_t *value) {
    if (mq5_adc == NULL) {
        g_mq5Error = MQ5_ERR_NO_ADC;
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status = HAL_ADC_Start(mq5_adc);
    if (status != HAL_OK) {
        record_read_error(MQ5_ERR_START);
        return status;
    }

    status = HAL_ADC_PollForConversion(mq5_adc, MQ5_ADC_TIMEOUT_MS);
    if (status != HAL_OK) {
        HAL_ADC_Stop(mq5_adc);
        record_read_error(MQ5_ERR_POLL);
        return status;
    }

    uint16_t sample = (uint16_t)HAL_ADC_GetValue(mq5_adc);

    status = HAL_ADC_Stop(mq5_adc);
    if (status != HAL_OK) {
        record_read_error(MQ5_ERR_STOP);
        return status;
    }

    *value = sample;
    return HAL_OK;
}

void hal_mq5_init(ADC_HandleTypeDef *hadc) {
    mq5_adc = NULL;
    mq5_last_value = 0;
    g_mq5Error = MQ5_ERR_NONE;
    g_mq5ReadErrors = 0;

    if (hadc == NULL) {
        g_mq5Error = MQ5_ERR_NO_ADC;
        return;
    }

    if (HAL_ADCEx_Calibration_Start(hadc) != HAL_OK) {
        g_mq5Error = MQ5_ERR_CALIBRATION;
        return;
    }

    mq5_adc = hadc;
}

uint16_t hal_mq5_read_average(uint8_t samples) {
    uint32_t sum = 0;
    uint16_t valid_count = 0;

    if (samples == 0U) {
        samples = 1U;
    }

    g_mq5Error = MQ5_ERR_NONE;

    for (uint16_t i = 0; i < samples; i++) {
        uint16_t sample;

        if (read_sample(&sample) == HAL_OK) {
            sum += sample;
            valid_count++;
        }
    }

    if (valid_count > 0U) {
        mq5_last_value = (uint16_t)(sum / valid_count);
    }

    return mq5_last_value;
}
