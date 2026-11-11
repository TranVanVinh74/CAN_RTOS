#ifndef APP_IPC_H
#define APP_IPC_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    uint8_t fan_state;
    uint8_t temperature;
    uint8_t humidity;
    uint16_t gas_level;
} ActuatorCommand_t;

extern QueueHandle_t xActuatorQueue;

bool app_ipc_init(void);

#endif
