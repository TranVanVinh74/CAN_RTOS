#include "MQ5.h"

static ADC_HandleTypeDef *mq5_adc = NULL;

/* Gia tri hop le gan nhat */
static uint16_t mq5_last_value = 0;


volatile uint8_t g_mq5Error = 0;
volatile uint32_t g_mq5ReadErrors = 0;

/* Doc mot mau, tach trang thai loi khoi gia tri ADC. */
static HAL_StatusTypeDef MQ5_ReadSample(uint16_t *value)
{
    HAL_StatusTypeDef status;

    if (mq5_adc == NULL)
    {
        g_mq5Error = 1;
        return HAL_ERROR;
    }

    status = HAL_ADC_Start(mq5_adc);

    if (status != HAL_OK)
    {
        g_mq5Error = 3;
        g_mq5ReadErrors++;
        return status;
    }

    status = HAL_ADC_PollForConversion(mq5_adc, 10);

    if (status != HAL_OK)
    {
        HAL_ADC_Stop(mq5_adc);

        g_mq5Error = 4;
        g_mq5ReadErrors++;
        return status;
    }

    uint16_t sample = (uint16_t)HAL_ADC_GetValue(mq5_adc);

    status = HAL_ADC_Stop(mq5_adc);

    if (status != HAL_OK)
    {
        g_mq5Error = 5;
        g_mq5ReadErrors++;
        return status;
    }

    *value = sample;
    return HAL_OK;
}

void MQ5_Init(ADC_HandleTypeDef *hadc)
{
    mq5_adc = NULL;
    mq5_last_value = 0;
    g_mq5Error = 0;
    g_mq5ReadErrors = 0;

    if (hadc == NULL)
    {
        g_mq5Error = 1;
        return;
    }

    if (HAL_ADCEx_Calibration_Start(hadc) != HAL_OK)
    {
        g_mq5Error = 2;
        return;
    }

    mq5_adc = hadc;
}

uint16_t MQ5_ReadRaw(void)
{
    uint16_t sample;

    if (MQ5_ReadSample(&sample) == HAL_OK)
    {
        mq5_last_value = sample;
        g_mq5Error = 0;
    }

    /* Neu loi: giu gia tri cu, khong tao mau 0 gia. */
    return mq5_last_value;
}

uint16_t MQ5_ReadAverage(uint8_t samples)
{
    uint32_t sum = 0;
    uint16_t sample;
    uint16_t valid_count = 0;

    if (samples == 0)
    {
        samples = 1;
    }

    /* Loi cua lan doc trung binh hien tai */
    g_mq5Error = 0;

    for (uint16_t i = 0; i < samples; i++)
    {
        if (MQ5_ReadSample(&sample) == HAL_OK)
        {
            sum += sample;
            valid_count++;
        }
    }

    if (valid_count > 0)
    {
        mq5_last_value = (uint16_t)(sum / valid_count);
    }

    /*
     * Chi tinh trung binh cac mau thanh cong.
     * Neu tat ca that bai: giu gia tri hop le gan nhat.
     * g_mq5Error van bao loi neu co mau that bai.
     */
    return mq5_last_value;
}

float MQ5_ReadVoltage(void)
{
    uint16_t raw_adc = MQ5_ReadAverage(10);

    float v_pin = ((float)raw_adc * 3.3f) / 4095.0f;

    return v_pin * MQ5_VOLTAGE_DIVIDER_RATIO;
}