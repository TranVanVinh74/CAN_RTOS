/**
 * @file hal_dht11.c
 * @brief GPIO pulse capture and checksum validation for DHT11 readings.
 */

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

volatile uint8_t g_dhtError = DHT11_ERR_NONE;
volatile uint8_t g_dhtBitIndex = 0;
volatile uint8_t g_dhtBytes[DHT11_FRAME_BYTES] = {0};

static TIM_HandleTypeDef *dht11_timer = NULL;
static GPIO_TypeDef *dht11_port = NULL;
static uint16_t dht11_pin = 0;

/**
 * @brief Configure the DHT11 data pin as an output and drive it low for the start signal.
 */
static void set_output_low(void) {
    GPIO_InitTypeDef gpio = {0};

    HAL_GPIO_WritePin(dht11_port, dht11_pin, GPIO_PIN_RESET);

    gpio.Pin = dht11_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(dht11_port, &gpio);
}

/**
 * @brief Release the DHT11 data line by configuring it as an input with a pull-up.
 */
static void set_input(void) {
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = dht11_pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(dht11_port, &gpio);
}

/**
 * @brief Read the free-running DHT11 timer counter.
 * @return Microsecond counter value; wraps at the configured timer period.
 */
static uint16_t now_us(void) {
    return (uint16_t)__HAL_TIM_GET_COUNTER(dht11_timer);
}

/**
 * @brief Wait until the DHT11 pin reaches the requested level or a timer timeout expires.
 * @param[in] level GPIO level to wait for.
 * @param[in] timeout_us Maximum polling duration in microseconds.
 * @return true if the level is observed before timeout; otherwise false.
 */
static bool wait_level(GPIO_PinState level, uint16_t timeout_us) {
    uint16_t start = now_us();

    while (HAL_GPIO_ReadPin(dht11_port, dht11_pin) != level) {
        if ((uint16_t)(now_us() - start) >= timeout_us) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Check that a DHT11 timer is bound and its counter is enabled.
 * @return true if the timer counter is running; otherwise false.
 */
static bool timer_running(void) {
    return (dht11_timer != NULL) && ((dht11_timer->Instance->CR1 & TIM_CR1_CEN) != 0U);
}

/**
 * @brief Wait for the low/high/low sequence of the DHT11 response.
 * @return DHT11_ERR_NONE on success; otherwise the response phase that timed out.
 */
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

/**
 * @brief Decode all 40 DHT11 bits by measuring their high pulse widths.
 * @param[out] frame Output five-byte frame, initially zeroed by the caller; may be partially filled on failure.
 * @return DHT11_ERR_NONE on success; otherwise the bit timing error.
 */
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

        frame[bit / DHT11_BITS_PER_BYTE] <<= 1;
        if (high_width_us > DHT11_BIT_ONE_THRESHOLD_US) {
            frame[bit / DHT11_BITS_PER_BYTE] |= 1U;
        }
    }
    return DHT11_ERR_NONE;
}

/**
 * @brief Release the data line, receive the DHT11 response and capture its data bits.
 * @param[out] frame Output five-byte frame initialized to zero before reception.
 * @return DHT11_ERR_NONE on success; otherwise a response or bit timing error.
 */
static Dht11Error_t read_frame(uint8_t frame[DHT11_FRAME_BYTES]) {
    HAL_GPIO_WritePin(dht11_port, dht11_pin, GPIO_PIN_SET);
    set_input();

    Dht11Error_t error = read_response();
    if (error != DHT11_ERR_NONE) {
        return error;
    }
    return read_bits(frame);
}

/**
 * @brief Compare the DHT11 checksum byte with the sum of its four data bytes.
 * @param[in] frame Input five-byte DHT11 frame.
 * @return true if the checksum matches; otherwise false.
 */
static bool checksum_ok(const uint8_t frame[DHT11_FRAME_BYTES]) {
    uint8_t sum = (uint8_t)(frame[0] + frame[1] + frame[2] + frame[3]);
    return sum == frame[DHT11_BYTE_CHECKSUM];
}

/**
 * @brief Bind the DHT11 GPIO and start its microsecond timer.
 * @param[in] htim Initialized timer configured with a 1 MHz counter for pulse timing.
 * @param[in] port GPIO port connected to the DHT11 data line.
 * @param[in] pin GPIO pin mask connected to the DHT11 data line.
 * @return true if the timer starts and the GPIO is configured; otherwise false.
 */
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

/**
 * @brief Read and checksum a DHT11 frame, suspending task scheduling during pulse capture.
 * @param[out] reading Output temperature in degrees Celsius and humidity in percent; updated only on success.
 * @return true if a valid frame was decoded; false on invalid arguments, timer failure, timeout or checksum error.
 */
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

    vTaskSuspendAll();
    Dht11Error_t error = read_frame(frame);
    (void)xTaskResumeAll();

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
