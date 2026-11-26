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

#include "alert_thresholds.h"

typedef struct {
    uint8_t temperature_c;
    uint8_t humidity_pct;
    uint16_t gas_raw;
    uint8_t sensor_faults;
    uint32_t received_ms;
} SensorData_t;

typedef struct {
    uint32_t received_ms;
    bool wants_call;
} AlarmState_t;

#define AT_COMMAND_TEXT_LEN 64U

typedef enum {
    AT_CMD_CHECK = 0,
    AT_CMD_DIAL,
    AT_CMD_HANGUP,
    AT_CMD_NONE
} AtCommandType_t;

typedef struct {
    AtCommandType_t type;
    uint32_t timeout_ms;
    char text[AT_COMMAND_TEXT_LEN];
} AtCommand_t;

typedef enum {
    MODEM_EVENT_OK = 0,
    MODEM_EVENT_CALL_ENDED,
    MODEM_EVENT_ERROR,
    MODEM_EVENT_RESET,
    MODEM_EVENT_TIMEOUT,
    MODEM_EVENT_TX_FAILED
} ModemEventType_t;

/* command is AT_CMD_NONE for unsolicited modem output received while no command was pending. */
typedef struct {
    ModemEventType_t type;
    AtCommandType_t command;
} ModemEvent_t;

extern QueueHandle_t xSensorQueue;
extern QueueHandle_t xThresholdQueue;
extern QueueHandle_t xAlarmQueue;
extern QueueHandle_t xAtCmdQueue;
extern QueueHandle_t xModemEventQueue;

/**
 * @brief Create the FreeRTOS queues used to exchange application data.
 * @return true if all required queues were created; false on allocation failure.
 */
bool app_ipc_init(void);

#endif
