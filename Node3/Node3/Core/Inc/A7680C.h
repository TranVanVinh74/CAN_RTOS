#ifndef A7680C_H
#define A7680C_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

void A7680C_Init(UART_HandleTypeDef *huart, GPIO_TypeDef *pwrPort, uint16_t pwrPin);
void A7680C_PowerOn(void);
bool A7680C_IsReady(void);
bool A7680C_SendSMS(const char *phone, const char *msg);

#endif /* A7680C_H */