/**
 * @file app_ipc.h
 * @brief Application queue handles, message types and queue allocation.
 */

#ifndef APP_IPC_H
#define APP_IPC_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    uint8_t temperature_c;
    uint8_t humidity_pct;
    bool valid;
} Dht11Sample_t;

typedef struct {
    uint16_t gas_raw;
    bool valid;
} Mq5Sample_t;

extern QueueHandle_t xDht11Queue;
extern QueueHandle_t xMq5Queue;

/**
 * @brief Create the FreeRTOS queues used to exchange application data.
 * @return true if all required queues were created; false on allocation failure.
 */
bool app_ipc_init(void);

#endif
