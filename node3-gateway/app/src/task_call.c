/**
 * @file task_call.c
 * @brief Call state machine and call policy.
 */

#include "task_call.h"

#include <stdio.h>

#include "app_alert_phone.h"
#include "app_cfg.h"
#include "app_ipc.h"
#include "app_time.h"

typedef enum {
    CALL_BOOT_WAIT = 0,
    CALL_WAIT_AT,
    CALL_READY,
    CALL_IN_PROGRESS
} CallState_t;

typedef struct {
    CallState_t state;
    TickType_t boot_started;
    TickType_t last_at_check;
    TickType_t last_idle_check;
    TickType_t call_started;
    TickType_t last_call;
    bool call_attempted;
    bool command_pending;
    AtCommandType_t pending_command;
    TickType_t pending_since;
    TickType_t pending_limit;
    bool hangup_requested;
} CallContext_t;

static bool alarm_wants_call(void) {
    AlarmState_t state;
    return xQueuePeek(xAlarmQueue, &state, 0U) == pdPASS && state.wants_call &&
           (app_time_ms() - state.received_ms <= SENSOR_FRESH_TIMEOUT_MS);
}

static bool enqueue_command(CallContext_t *ctx, AtCommandType_t type, const char *text,
                            uint32_t timeout_ms) {
    AtCommand_t command = {.type = type, .timeout_ms = timeout_ms};
    (void)snprintf(command.text, sizeof(command.text), "%s", text);
    if (xQueueSend(xAtCmdQueue, &command, 0U) != pdPASS) {
        return false;
    }
    ctx->command_pending = true;
    ctx->pending_command = type;
    ctx->pending_since = xTaskGetTickCount();
    ctx->pending_limit = pdMS_TO_TICKS(timeout_ms + MODEM_EVENT_GUARD_MS);
    return true;
}

static void retry_at_check(CallContext_t *ctx, TickType_t now) {
    ctx->state = CALL_WAIT_AT;
    ctx->hangup_requested = false;
    ctx->last_at_check = now - pdMS_TO_TICKS(MODEM_AT_RETRY_MS);
}

static void on_command_ok(CallContext_t *ctx, AtCommandType_t command, TickType_t now) {
    if (command == AT_CMD_CHECK) {
        ctx->state = CALL_READY;
        ctx->last_idle_check = now;
    } else if (command == AT_CMD_DIAL) {
        ctx->state = CALL_IN_PROGRESS;
    } else if (command == AT_CMD_HANGUP) {
        ctx->state = CALL_READY;
        ctx->hangup_requested = false;
    }
}

static void on_command_failed(CallContext_t *ctx, AtCommandType_t command,
                              ModemEventType_t type, TickType_t now) {
    if (command == AT_CMD_DIAL) {
        /*
         * A late or failed ATD reply does not prove the modem is not ringing, so the call stays
         * tracked: the 30 s cap and ATH on alarm clear still apply. ERROR/TX failure hang up now.
         */
        ctx->state = CALL_IN_PROGRESS;
        ctx->hangup_requested = (type != MODEM_EVENT_TIMEOUT);
    } else {
        retry_at_check(ctx, now);
    }
}

static void handle_event(CallContext_t *ctx, const ModemEvent_t *event, TickType_t now) {
    if (event->type == MODEM_EVENT_RESET) {
        /* A modem reboot drops any call; give it the boot time again before probing. */
        ctx->state = CALL_BOOT_WAIT;
        ctx->boot_started = now;
        ctx->command_pending = false;
        ctx->hangup_requested = false;
        return;
    }

    AtCommandType_t command = ctx->pending_command;
    bool completes = ctx->command_pending && (event->command == command);
    if (completes) {
        ctx->command_pending = false;
    }

    if (event->type == MODEM_EVENT_CALL_ENDED) {
        if ((ctx->state == CALL_IN_PROGRESS) || (completes && (command != AT_CMD_CHECK))) {
            ctx->state = CALL_READY;
            ctx->hangup_requested = false;
        } else if (completes) {
            retry_at_check(ctx, now);
        }
        return;
    }

    if (!completes) {
        /* An unsolicited error during a call leaves the call state unknown, so hang up. */
        if ((event->type == MODEM_EVENT_ERROR) && (ctx->state == CALL_IN_PROGRESS)) {
            ctx->hangup_requested = true;
        }
        return;
    }
    if (event->type == MODEM_EVENT_OK) {
        on_command_ok(ctx, command, now);
    } else {
        on_command_failed(ctx, command, event->type, now);
    }
}

static void prvCallTask(void *argument) {
    (void)argument;
    CallContext_t ctx = {
        .state = CALL_BOOT_WAIT,
        .boot_started = xTaskGetTickCount(),
        .pending_command = AT_CMD_NONE,
    };

    for (;;) {
        ModemEvent_t event;
        while (xQueueReceive(xModemEventQueue, &event, 0U) == pdPASS) {
            handle_event(&ctx, &event, xTaskGetTickCount());
        }

        TickType_t now = xTaskGetTickCount();
        /* The completion event is lost if the event queue was full; never wait on it forever. */
        if (ctx.command_pending && ((now - ctx.pending_since) >= ctx.pending_limit)) {
            ctx.command_pending = false;
            on_command_failed(&ctx, ctx.pending_command, MODEM_EVENT_TIMEOUT, now);
        }

        if (ctx.state == CALL_BOOT_WAIT) {
            if ((now - ctx.boot_started) >= pdMS_TO_TICKS(MODEM_BOOT_WAIT_MS)) {
                retry_at_check(&ctx, now);
            }
        } else if (ctx.state == CALL_WAIT_AT) {
            if (!ctx.command_pending &&
                (now - ctx.last_at_check) >= pdMS_TO_TICKS(MODEM_AT_RETRY_MS)) {
                if (enqueue_command(&ctx, AT_CMD_CHECK, "AT\r\n", MODEM_COMMAND_TIMEOUT_MS)) {
                    ctx.last_at_check = now;
                }
            }
        } else if (ctx.state == CALL_READY) {
            if (!ctx.command_pending && !alarm_wants_call() &&
                (now - ctx.last_idle_check) >= pdMS_TO_TICKS(MODEM_IDLE_CHECK_MS)) {
                if (enqueue_command(&ctx, AT_CMD_CHECK, "AT\r\n", MODEM_COMMAND_TIMEOUT_MS)) {
                    ctx.last_idle_check = now;
                    ctx.state = CALL_WAIT_AT;
                }
            }
            if (!ctx.command_pending && alarm_wants_call() &&
                (!ctx.call_attempted || (now - ctx.last_call) >= pdMS_TO_TICKS(CALL_INTERVAL_MS))) {
                char command[AT_COMMAND_TEXT_LEN];
                (void)snprintf(command, sizeof(command), "ATD%s;\r\n", APP_ALERT_PHONE);
                if (enqueue_command(&ctx, AT_CMD_DIAL, command, MODEM_DIAL_TIMEOUT_MS)) {
                    ctx.call_attempted = true;
                    ctx.last_call = now;
                    /* The 30 s cap counts from dialing, not from the OK that may never come. */
                    ctx.call_started = now;
                }
            }
        } else if (ctx.state == CALL_IN_PROGRESS) {
            if (!ctx.command_pending &&
                (ctx.hangup_requested || !alarm_wants_call() ||
                 (now - ctx.call_started) >= pdMS_TO_TICKS(CALL_DURATION_MS))) {
                if (enqueue_command(&ctx, AT_CMD_HANGUP, "ATH\r\n", MODEM_COMMAND_TIMEOUT_MS)) {
                    ctx.hangup_requested = false;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_CALL_POLL_MS));
    }
}

BaseType_t task_call_create(void) {
    return xTaskCreate(prvCallTask, "CALL", TASK_CALL_STACK_WORDS, NULL, TASK_CALL_PRIO, NULL);
}
