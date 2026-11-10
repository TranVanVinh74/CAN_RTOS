#ifndef MQ5_H
#define MQ5_H

#include "stm32f1xx_hal.h"

// T? l? phân áp ph?n c?ng: (R1 + R2) / R2
// Ví d?: R1 = 1k, R2 = 2k -> H? s? nhân là 1.5f (d? khôi ph?c t? 3.3V v? 5V)
#define MQ5_VOLTAGE_DIVIDER_RATIO   1.5f 

void MQ5_Init(ADC_HandleTypeDef *hadc);
uint16_t MQ5_ReadRaw(void);
uint16_t MQ5_ReadAverage(uint8_t samples);
float MQ5_ReadVoltage(void); // Hàm m?i d?c di?n áp th?c t? c?m bi?n

#endif /* MQ5_H */