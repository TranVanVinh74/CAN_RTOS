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

static TaskHandle_t xModemAtTaskHandle = NULL;
static char reply[MODEM_REPLY_BUFFER_LEN];
static uint16_t reply_length = 0U;

static void publish_event(ModemEventType_t type, AtCommandType_t command) {
    ModemEvent_t event = {.type = type, .command = command};
    (void)xQueueSend(xModemEventQueue, &event, 0U);
}

static void reset_reply(void) {
    reply_length = 0U;
    reply[0] = '\0';
}

/* Keeps the unfinished last line, including its leading newline that the RDY marker needs. */
static void drop_completed_lines(void) {
    const char *last_newline = strrchr(reply, '\n');
    if (last_newline == NULL) {
        return;
    }
    size_t keep = strlen(last_newline);
    memmove(reply, last_newline, keep + 1U);
    reply_length = (uint16_t)keep;
}

static void append_reply(uint8_t byte) {
    if (reply_length >= (sizeof(reply) - 1U)) {
        memmove(reply, reply + 1, sizeof(reply) - 2U);
        reply_length = sizeof(reply) - 2U;
    }
    reply[reply_length++] = (char)byte;
    reply[reply_length] = '\0';
}

/**
 * @brief Classify buffered modem output and publish one event per recognized response.
 * @param[in] command_pending true while a sent command is waiting for its final response.
 * @param[in] pending_command Command the next final response belongs to.
 * @param[out] saw_reset Set to true if a modem reboot indication was received.
 * @return true if the pending command received its final response.
 */
static bool parse_rx(bool command_pending, AtCommandType_t pending_command, bool *saw_reset) {
    bool completed = false;
    uint8_t byte;

    while (hal_a7680c_read_byte(&byte)) {
        append_reply(byte);

        bool awaiting = command_pending && !completed;
        ModemEventType_t type;
        ModemReply_t classified = app_modem_reply_classify(reply);
        if (classified == MODEM_REPLY_MODEM_RESET) {
            type = MODEM_EVENT_RESET;
            *saw_reset = true;
        } else if (classified == MODEM_REPLY_CALL_ENDED) {
            type = MODEM_EVENT_CALL_ENDED;
        } else if (classified == MODEM_REPLY_ERROR) {
            type = MODEM_EVENT_ERROR;
        } else if (awaiting && (strstr(reply, "OK") != NULL)) {
            type = MODEM_EVENT_OK;
        } else {
            continue;
        }

        /* Output after the final response is unsolicited, so CALL cannot pair it with a command. */
        publish_event(type, awaiting ? pending_command : AT_CMD_NONE);
        reset_reply();
        if (awaiting) {
            completed = true;
        }
    }
    return completed;
}

static void prvModemAtTask(void *argument) {
    (void)argument;
    AtCommand_t command;
    AtCommandType_t pending_command = AT_CMD_NONE;
    bool command_pending = false;
    TickType_t command_started = 0U;
    TickType_t pending_timeout = pdMS_TO_TICKS(MODEM_COMMAND_TIMEOUT_MS);
    bool modem_rebooting = false;
    TickType_t reset_seen = 0U;

    hal_a7680c_set_rx_task(xModemAtTaskHandle);

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TASK_MODEM_AT_POLL_MS));

        /* Bytes already received (RDY, NO CARRIER) are classified before a new command starts. */
        bool saw_reset = false;
        if (parse_rx(command_pending, pending_command, &saw_reset)) {
            command_pending = false;
        }
        if (saw_reset) {
            modem_rebooting = true;
            reset_seen = xTaskGetTickCount();
        } else if (modem_rebooting &&
                   ((xTaskGetTickCount() - reset_seen) >= pdMS_TO_TICKS(MODEM_BOOT_WAIT_MS))) {
            modem_rebooting = false;
        }

        if (command_pending &&
            ((xTaskGetTickCount() - command_started) >= pending_timeout)) {
            publish_event(MODEM_EVENT_TIMEOUT, pending_command);
            command_pending = false;
        }

        if (modem_rebooting) {
            /*
             * Commands queued before CALL handled the reset would run untracked (an ATD would ring
             * with no 30 s cap). CALL waits the same boot time, so nothing valid is dropped.
             */
            while (xQueueReceive(xAtCmdQueue, &command, 0U) == pdPASS) {
            }
        } else if (!command_pending && xQueueReceive(xAtCmdQueue, &command, 0U) == pdPASS) {
            /* A late "OK" from an earlier command must not satisfy the new one. */
            drop_completed_lines();
            if (!hal_a7680c_send(command.text)) {
                publish_event(MODEM_EVENT_TX_FAILED, command.type);
            } else {
                pending_command = command.type;
                command_pending = true;
                command_started = xTaskGetTickCount();
                pending_timeout = pdMS_TO_TICKS(command.timeout_ms);
            }
        }
    }
}

BaseType_t task_modem_at_create(void) {
    return xTaskCreate(prvModemAtTask, "MODEM_AT", TASK_MODEM_AT_STACK_WORDS, NULL,
                       TASK_MODEM_AT_PRIO, &xModemAtTaskHandle);
}
