#include "hal_dht11.h"

#include "FreeRTOS.h"
#include "task.h"

#define DHT11_START_LOW_MS 20U
#define DHT11_RESPONSE_TIMEOUT_US 150U
#define DHT11_BIT_TIMEOUT_US 120U
#define DHT11_BIT_ONE_THRESHOLD_US 50U
#define DHT11_FRAME_BITS 40U
#define DHT11_BYTE_HUMIDITY 0U
#define DHT11_BYTE_TEMPERATURE 2U
#define DHT11_BYTE_CHECKSUM 4U

volatile uint8_t g_dhtError = DHT11_ERR_NONE;
volatile uint8_t g_dhtBitIndex = 0;
volatile uint8_t g_dhtBytes[DHT11_FRAME_BYTES] = {0};

static TIM_HandleTypeDef *dht11_timer = NULL;
static GPIO_TypeDef *dht11_port = NULL;
static uint16_t dht11_pin = 0;

static void set_output_low(void) {
    GPIO_InitTypeDef gpio = {0};

    HAL_GPIO_WritePin(dht11_port, dht11_pin, GPIO_PIN_RESET);

    gpio.Pin = dht11_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(dht11_port, &gpio);
}

static void set_input(void) {
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = dht11_pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(dht11_port, &gpio);
}

static uint16_t now_us(void) {
    return (uint16_t)__HAL_TIM_GET_COUNTER(dht11_timer);
}

static bool wait_level(GPIO_PinState level, uint16_t timeout_us) {
    uint16_t start = now_us();

    while (HAL_GPIO_ReadPin(dht11_port, dht11_pin) != level) {
        if ((uint16_t)(now_us() - start) >= timeout_us) {
            return false;
        }
    }

    return true;
}

static bool timer_running(void) {
    return (dht11_timer != NULL) && ((dht11_timer->Instance->CR1 & TIM_CR1_CEN) != 0U);
}

static Dht11Error_t read_response(void) {
    if (!wait_level(GPIO_PIN_RESET, DHT11_RESPONSE_TIMEOUT_US)) {
        return DHT11_ERR_RESPONSE_LOW;
    }
    if (!wait_level(GPIO_PIN_SET, DHT11_RESPONSE_TIMEOUT_US)) {
        return DHT11_ERR_RESPONSE_HIGH;
    }
    if (!wait_level(GPIO_PIN_RESET, DHT11_RESPONSE_TIMEOUT_US)) {
        return DHT11_ERR_RESPONSE_END;
    }
    return DHT11_ERR_NONE;
}

static Dht11Error_t read_bits(uint8_t frame[DHT11_FRAME_BYTES]) {
    for (uint8_t bit = 0; bit < DHT11_FRAME_BITS; bit++) {
        g_dhtBitIndex = bit;

        if (!wait_level(GPIO_PIN_SET, DHT11_BIT_TIMEOUT_US)) {
            return DHT11_ERR_BIT_START;
        }

        uint16_t high_start = now_us();

        if (!wait_level(GPIO_PIN_RESET, DHT11_BIT_TIMEOUT_US)) {
            return DHT11_ERR_BIT_END;
        }

        uint16_t high_width_us = (uint16_t)(now_us() - high_start);

        frame[bit / 8U] <<= 1;
        if (high_width_us > DHT11_BIT_ONE_THRESHOLD_US) {
            frame[bit / 8U] |= 1U;
        }
    }
    return DHT11_ERR_NONE;
}

static Dht11Error_t read_frame(uint8_t frame[DHT11_FRAME_BYTES]) {
    HAL_GPIO_WritePin(dht11_port, dht11_pin, GPIO_PIN_SET);
    set_input();

    Dht11Error_t error = read_response();
    if (error != DHT11_ERR_NONE) {
        return error;
    }
    return read_bits(frame);
}

static bool checksum_ok(const uint8_t frame[DHT11_FRAME_BYTES]) {
    uint8_t sum = (uint8_t)(frame[0] + frame[1] + frame[2] + frame[3]);
    return sum == frame[DHT11_BYTE_CHECKSUM];
}

bool hal_dht11_init(TIM_HandleTypeDef *htim, GPIO_TypeDef *port, uint16_t pin) {
    dht11_timer = htim;
    dht11_port = port;
    dht11_pin = pin;

    if ((htim == NULL) || (port == NULL) || (HAL_TIM_Base_Start(htim) != HAL_OK)) {
        g_dhtError = DHT11_ERR_TIMER;
        return false;
    }

    set_input();
    return true;
}

bool hal_dht11_read(Dht11Reading_t *reading) {
    uint8_t frame[DHT11_FRAME_BYTES] = {0};

    if (reading == NULL) {
        g_dhtError = DHT11_ERR_ARGUMENT;
        return false;
    }

    if (!timer_running() || (dht11_port == NULL)) {
        g_dhtError = DHT11_ERR_TIMER;
        return false;
    }

    g_dhtError = DHT11_ERR_NONE;
    g_dhtBitIndex = 0;
    for (uint8_t i = 0; i < DHT11_FRAME_BYTES; i++) {
        g_dhtBytes[i] = 0;
    }

    set_output_low();
    vTaskDelay(pdMS_TO_TICKS(DHT11_START_LOW_MS) + 1U);

    taskENTER_CRITICAL();
    Dht11Error_t error = read_frame(frame);
    taskEXIT_CRITICAL();

    for (uint8_t i = 0; i < DHT11_FRAME_BYTES; i++) {
        g_dhtBytes[i] = frame[i];
    }

    if (error != DHT11_ERR_NONE) {
        g_dhtError = error;
        return false;
    }

    if (!checksum_ok(frame)) {
        g_dhtError = DHT11_ERR_CHECKSUM;
        return false;
    }

    reading->humidity_pct = frame[DHT11_BYTE_HUMIDITY];
    reading->temperature_c = frame[DHT11_BYTE_TEMPERATURE];
    return true;
}
