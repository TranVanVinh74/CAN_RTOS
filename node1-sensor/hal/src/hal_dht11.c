#include "hal_dht11.h"

#include "FreeRTOS.h"
#include "task.h"

#define DHT11_START_LOW_MS 20U
#define DHT11_RESPONSE_TIMEOUT_US 150U
#define DHT11_BIT_TIMEOUT_US 120U
#define DHT11_BIT_ONE_THRESHOLD_US 50U
#define DHT11_BITS_PER_BYTE 8U
#define DHT11_FRAME_BITS (DHT11_FRAME_BYTES * DHT11_BITS_PER_BYTE)
#define DHT11_BYTE_HUMIDITY 0U
#define DHT11_BYTE_TEMPERATURE 2U
#define DHT11_BYTE_CHECKSUM 4U

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

static bool read_response(void) {
    return wait_level(GPIO_PIN_RESET, DHT11_RESPONSE_TIMEOUT_US) &&
           wait_level(GPIO_PIN_SET, DHT11_RESPONSE_TIMEOUT_US) &&
           wait_level(GPIO_PIN_RESET, DHT11_RESPONSE_TIMEOUT_US);
}

static bool read_bits(uint8_t frame[DHT11_FRAME_BYTES]) {
    for (uint8_t bit = 0; bit < DHT11_FRAME_BITS; bit++) {
        if (!wait_level(GPIO_PIN_SET, DHT11_BIT_TIMEOUT_US)) {
            return false;
        }

        uint16_t high_start = now_us();

        if (!wait_level(GPIO_PIN_RESET, DHT11_BIT_TIMEOUT_US)) {
            return false;
        }

        uint16_t high_width_us = (uint16_t)(now_us() - high_start);

        frame[bit / DHT11_BITS_PER_BYTE] <<= 1;
        if (high_width_us > DHT11_BIT_ONE_THRESHOLD_US) {
            frame[bit / DHT11_BITS_PER_BYTE] |= 1U;
        }
    }
    return true;
}

static bool read_frame(uint8_t frame[DHT11_FRAME_BYTES]) {
    HAL_GPIO_WritePin(dht11_port, dht11_pin, GPIO_PIN_SET);
    set_input();
    return read_response() && read_bits(frame);
}

static bool checksum_ok(const uint8_t frame[DHT11_FRAME_BYTES]) {
    uint8_t sum = 0U;
    for (uint8_t i = 0U; i < DHT11_BYTE_CHECKSUM; i++) {
        sum = (uint8_t)(sum + frame[i]);
    }
    return sum == frame[DHT11_BYTE_CHECKSUM];
}

bool hal_dht11_init(TIM_HandleTypeDef *htim, GPIO_TypeDef *port, uint16_t pin) {
    dht11_timer = htim;
    dht11_port = port;
    dht11_pin = pin;

    if ((htim == NULL) || (port == NULL) || (HAL_TIM_Base_Start(htim) != HAL_OK)) {
        return false;
    }

    set_input();
    return true;
}

bool hal_dht11_read(Dht11Reading_t *reading) {
    uint8_t frame[DHT11_FRAME_BYTES] = {0};

    if ((reading == NULL) || !timer_running() || (dht11_port == NULL)) {
        return false;
    }

    set_output_low();
    vTaskDelay(pdMS_TO_TICKS(DHT11_START_LOW_MS) + 1U);

    vTaskSuspendAll();
    bool received = read_frame(frame);
    (void)xTaskResumeAll();

    if (!received || !checksum_ok(frame)) {
        return false;
    }

    reading->humidity_pct = frame[DHT11_BYTE_HUMIDITY];
    reading->temperature_c = frame[DHT11_BYTE_TEMPERATURE];
    return true;
}
