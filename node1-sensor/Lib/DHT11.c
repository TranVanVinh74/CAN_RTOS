#include "DHT11.h"
#include "FreeRTOS.h"
#include "task.h"

#define DHT11_PORT GPIOA
#define DHT11_PIN  GPIO_PIN_2

static TIM_HandleTypeDef *dht11_timer = NULL;


 
volatile uint8_t g_dhtError = 0;
volatile uint8_t g_dhtBitIndex = 0;
volatile uint8_t g_dhtBytes[5] = {0};

/* Chuan bi output push-pull, muc LOW. */
static void DHT11_SetOutputLow(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Nap muc LOW truoc khi chuyen sang output. */
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);

    gpio.Pin = DHT11_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(DHT11_PORT, &gpio);
}

/* Chuyen sang input de cam bien dieu khien DATA. */
static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = DHT11_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(DHT11_PORT, &gpio);
}

/*
 * TIM2 phai chay lien tuc o 1 MHz, ARR = 65535.
 * Phep tru uint16_t xu ly duoc mot lan tran bo dem.
 */
static uint16_t DHT11_NowUs(void)
{
    return (uint16_t)__HAL_TIM_GET_COUNTER(dht11_timer);
}

/* Cho DATA dat muc mong muon, co timeout bang timer. */
static uint8_t DHT11_WaitLevel(GPIO_PinState level,
                               uint16_t timeout_us)
{
    uint16_t start = DHT11_NowUs();

    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != level)
    {
        if ((uint16_t)(DHT11_NowUs() - start) >= timeout_us)
        {
            return 0;
        }
    }

    return 1;
}

void DHT11_Init(TIM_HandleTypeDef *htim)
{
    dht11_timer = htim;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Khi nghi, nhuong DATA cho dien tro keo len. */
    DHT11_SetInput();
}

/*
 * Chi goi tu task sau khi scheduler da chay.
 * Chi mot task su dung driver nay.
 *
 * Tra ve:
 * 1 = thanh cong
 * 0 = that bai, xem g_dhtError
 */
uint8_t DHT11_Read(DHT11_Data_t *DHT_Data)
{
    uint8_t data[5] = {0};
    uint8_t error = 0;
    uint16_t highStart;
    uint16_t highWidth;

    if (DHT_Data == NULL)
    {
        g_dhtError = 8;
        return 0;
    }

    if (dht11_timer == NULL)
    {
        g_dhtError = 1;
        return 0;
    }

    if ((dht11_timer->Instance->CR1 & TIM_CR1_CEN) == 0U)
    {
        g_dhtError = 1;
        return 0;
    }

    g_dhtError = 0;
    g_dhtBitIndex = 0;

    for (uint8_t i = 0; i < 5; i++)
    {
        g_dhtBytes[i] = 0;
    }

    /* 1. STM32 phat tin hieu bat dau: LOW khoang 20 ms. */
    DHT11_SetOutputLow();

    /*
     * Cho o ngoai critical section de task khac van chay.
     * Cong 1 tick de tranh thoi gian cho ngan do lech pha tick.
     */
    vTaskDelay(pdMS_TO_TICKS(20) + 1U);

    /*
     * 2. Bao ve pha bat phan hoi va doc 40 bit.
     * Moi vong cho deu co timeout TIM2.
     */
    taskENTER_CRITICAL();

    /* Dua len HIGH roi chuyen ngay sang input. */
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    DHT11_SetInput();

    /* Cam bien phan hoi: LOW -> HIGH -> LOW. */
    if (!DHT11_WaitLevel(GPIO_PIN_RESET, 150))
    {
        error = 2;
        goto read_done;
    }

    if (!DHT11_WaitLevel(GPIO_PIN_SET, 150))
    {
        error = 3;
        goto read_done;
    }

    if (!DHT11_WaitLevel(GPIO_PIN_RESET, 150))
    {
        error = 4;
        goto read_done;
    }

    /* 3. Doc 40 bit, byte cao truoc, bit MSB truoc. */
    for (uint8_t bit = 0; bit < 40; bit++)
    {
        g_dhtBitIndex = bit;

        /* Ket thuc muc LOW dau bit, bat dau muc HIGH. */
        if (!DHT11_WaitLevel(GPIO_PIN_SET, 120))
        {
            error = 5;
            goto read_done;
        }

        highStart = DHT11_NowUs();

        /* Do do rong muc HIGH. */
        if (!DHT11_WaitLevel(GPIO_PIN_RESET, 120))
        {
            error = 6;
            goto read_done;
        }

        highWidth = (uint16_t)(DHT11_NowUs() - highStart);

        data[bit / 8] <<= 1;

        /* HIGH ngan: bit 0; HIGH dai: bit 1. */
        if (highWidth > 50U)
        {
            data[bit / 8] |= 1U;
        }
    }

read_done:
    /*
     * Ca thanh cong va loi deu thoat critical section.
     * DATA van la input, khong chu dong keo HIGH.
     */
    taskEXIT_CRITICAL();

    for (uint8_t i = 0; i < 5; i++)
    {
        g_dhtBytes[i] = data[i];
    }

    if (error != 0)
    {
        g_dhtError = error;
        return 0;
    }

    /* 4. Checksum la 8 bit thap cua tong 4 byte dau. */
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3])
        != data[4])
    {
        g_dhtError = 7;
        return 0;
    }

    /* Chi cap nhat ket qua khi khung hop le. */
    DHT_Data->Humidity =
        (float)data[0] + (float)data[1] / 10.0f;

    DHT_Data->Temperature =
        (float)data[2] + (float)data[3] / 10.0f;

    return 1;
}