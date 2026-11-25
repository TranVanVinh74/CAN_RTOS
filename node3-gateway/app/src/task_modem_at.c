/**
 * @file task_modem_at.c
 * @brief The only task that accesses the A7680C USART2 interface.
 */

#include "task_modem_at.h"

#include <string.h>

#include "app_cfg.h"
#include "app_ipc.h"
#include "app_modem_reply.h"
#include "hal_a7680c.h"

#define MODEM_REPLY_BUFFER_LEN 256U

static TaskHandle_t modem_at_task_handle = NULL;

static void publish_event(ModemEventType_t type, AtCommandType_t command) {
    ModemEvent_t event = {.type = type, .command = command};
    (void)xQueueSend(xModemEventQueue, &event, 0U);
}

static void parse_rx(AtCommandType_t pending_command, bool command_pending) {
    static char reply[MODEM_REPLY_BUFFER_LEN];
    static uint16_t reply_length = 0U;
    uint8_t byte;

    while (hal_a7680c_read_byte(&byte)) {
        if (reply_length >= (sizeof(reply) - 1U)) {
            memmove(reply, reply + 1, sizeof(reply) - 2U);
            reply_length = sizeof(reply) - 2U;
        }
        reply[reply_length++] = (char)byte;
        reply[reply_length] = '\0';

        ModemReply_t classified = app_modem_reply_classify(reply);
        if (classified == MODEM_REPLY_MODEM_RESET) {
            publish_event(MODEM_EVENT_RESET, pending_command);
            reply_length = 0U;
            reply[0] = '\0';
        } else if (classified == MODEM_REPLY_CALL_ENDED) {
            publish_event(MODEM_EVENT_CALL_ENDED, pending_command);
            reply_length = 0U;
            reply[0] = '\0';
        } else if (classified == MODEM_REPLY_ERROR) {
            publish_event(MODEM_EVENT_ERROR, pending_command);
            reply_length = 0U;
            reply[0] = '\0';
        } else if (strstr(reply, "OK") != NULL && command_pending) {
            publish_event(MODEM_EVENT_OK, pending_command);
            reply_length = 0U;
            reply[0] = '\0';
        }
    }
}

static void prvModemAtTask(void *argument) {
    (void)argument;
    AtCommand_t command;
    AtCommandType_t pending_command = AT_CMD_CHECK;
    bool command_pending = false;
    TickType_t command_started = 0U;
    TickType_t pending_timeout = pdMS_TO_TICKS(MODEM_COMMAND_TIMEOUT_MS);

    hal_a7680c_set_rx_task(modem_at_task_handle);

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TASK_MODEM_AT_POLL_MS));

        if (!command_pending && xQueueReceive(xAtCmdQueue, &command, 0U) == pdPASS) {
            hal_a7680c_drain();
            if (!hal_a7680c_send(command.text)) {
                publish_event(MODEM_EVENT_TX_FAILED, command.type);
            } else {
                pending_command = command.type;
                command_pending = true;
                command_started = xTaskGetTickCount();
                pending_timeout = pdMS_TO_TICKS(command.timeout_ms);
            }
        }

        parse_rx(pending_command, command_pending);

        if (command_pending &&
            ((xTaskGetTickCount() - command_started) >= pending_timeout)) {
            publish_event(MODEM_EVENT_TIMEOUT, pending_command);
            command_pending = false;
        }

        /* Terminal events complete the command currently being serviced. */
        if (command_pending) {
            ModemEvent_t event;
            if (xQueuePeek(xModemEventQueue, &event, 0U) == pdPASS) {
                command_pending = false;
            }
        }
    }
}

BaseType_t task_modem_at_create(void) {
    return xTaskCreate(prvModemAtTask, "MODEM_AT", TASK_MODEM_AT_STACK_WORDS, NULL,
                       TASK_MODEM_AT_PRIO, &modem_at_task_handle);
}
