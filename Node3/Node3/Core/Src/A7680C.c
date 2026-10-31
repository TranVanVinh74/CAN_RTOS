#include "A7680C.h"
#include "cmsis_os.h"

static UART_HandleTypeDef *sim_uart;
static GPIO_TypeDef *pwr_port;
static uint16_t pwr_pin;

void A7680C_Init(UART_HandleTypeDef *huart, GPIO_TypeDef *pwrPort, uint16_t pwrPin) {
    sim_uart = huart;
    pwr_port = pwrPort;
    pwr_pin = pwrPin;
}

void A7680C_PowerOn(void) {
    // Ð?m b?o m?c High ban d?u
    HAL_GPIO_WritePin(pwr_port, pwr_pin, GPIO_PIN_SET);
    osDelay(500);
    // Kéo Low 2 giây d? b?t module
    HAL_GPIO_WritePin(pwr_port, pwr_pin, GPIO_PIN_RESET);
    osDelay(2000);
    HAL_GPIO_WritePin(pwr_port, pwr_pin, GPIO_PIN_SET);
    osDelay(3000); // Ð?i modem b?t m?ng
}

static bool A7680C_SendCmdWait(const char *cmd, const char *expected, uint32_t timeout) {
    char rx_buf[128] = {0};
    HAL_UART_Transmit(sim_uart, (uint8_t*)cmd, strlen(cmd), 1000);
    
    uint32_t start = HAL_GetTick();
    uint16_t idx = 0;
    while (HAL_GetTick() - start < timeout) {
        uint8_t ch;
        if (HAL_UART_Receive(sim_uart, &ch, 1, 10) == HAL_OK) {
            if (idx < sizeof(rx_buf) - 1) {
                rx_buf[idx++] = ch;
                rx_buf[idx] = '\0';
                if (strstr(rx_buf, expected) != NULL) return true;
            }
        }
    }
    return false;
}

bool A7680C_IsReady(void) {
    return A7680C_SendCmdWait("AT\r\n", "OK", 1000);
}

bool A7680C_SendSMS(const char *phone, const char *msg) {
    // 1. C?u hình SMS d?ng Text
    if (!A7680C_SendCmdWait("AT+CMGF=1\r\n", "OK", 1000)) return false;

    // 2. Cài d?t s? di?n tho?i
    char cmd[40];
    sprintf(cmd, "AT+CMGS=\"%s\"\r\n", phone);
    if (!A7680C_SendCmdWait(cmd, ">", 3000)) return false;

    // 3. G?i n?i dung tin nh?n và k?t thúc b?ng ký t? Ctrl+Z (ASCII 26)
    HAL_UART_Transmit(sim_uart, (uint8_t*)msg, strlen(msg), 1000);
    uint8_t ctrlZ = 26;
    HAL_UART_Transmit(sim_uart, &ctrlZ, 1, 1000);

    // 4. Ch? xác nh?n dã g?i t? tr?m BTS
    return A7680C_SendCmdWait("", "+CMGS:", 10000);
}