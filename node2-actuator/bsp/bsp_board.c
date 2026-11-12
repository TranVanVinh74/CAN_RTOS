#include "bsp_board.h"

#include "main.h"

#include "hal_can.h"
#include "hal_relay.h"
#include "hal_tft.h"

extern CAN_HandleTypeDef hcan;
extern SPI_HandleTypeDef hspi1;

volatile uint32_t g_errorCode = BOARD_ERR_NONE;

static const TftPins_t tft_pins = {
    .cs_port = TFT_CS_GPIO_Port,
    .cs_pin = TFT_CS_Pin,
    .dc_port = TFT_DC_GPIO_Port,
    .dc_pin = TFT_DC_Pin,
    .rst_port = TFT_RST_GPIO_Port,
    .rst_pin = TFT_RST_Pin,
};

void bsp_board_init(void) {
    hal_relay_init(BSP_RELAY_PORT, BSP_RELAY_PIN, BSP_RELAY_ON_LEVEL);
    hal_tft_init(&hspi1, &tft_pins);
    hal_can_init(&hcan);
}

void bsp_board_fail(BoardError_t error) {
    g_errorCode = error;
    Error_Handler();
}
