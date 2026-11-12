#include "app_call.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "app_cfg.h"
#include "app_modem_reply.h"
#include "hal_a7680c.h"

#define CALL_COMMAND_LEN 64U
#define CALL_REPLY_LEN 256U

volatile uint32_t g_callStage = CALL_STAGE_NOT_STARTED;
volatile uint32_t g_atOK = 0;
volatile uint32_t g_callRequest = 0;
volatile uint32_t g_hangupRequest = 0;
volatile uint32_t g_callActive = 0;
volatile uint32_t g_callAttempts = 0;
volatile uint32_t g_callElapsedMs = 0;
volatile uint32_t g_callCooldownMs = 0;
volatile uint32_t g_hangupCount = 0;
volatile uint32_t g_modemResetCount = 0;

char g_lastCommand[CALL_COMMAND_LEN];
char g_callRx[CALL_REPLY_LEN];

static uint16_t reply_length = 0;
static bool call_active = false;
static bool call_attempted = false;
static uint32_t call_started_ms = 0;
static uint32_t last_attempt_ms = 0;

static uint32_t now_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void reset_reply(void) {
    memset(g_callRx, 0, sizeof(g_callRx));
    reply_length = 0;
}

static void append_reply(uint8_t byte) {
    if (reply_length >= (sizeof(g_callRx) - 1U)) {
        memmove(g_callRx, g_callRx + 1, sizeof(g_callRx) - 2U);
        reply_length = sizeof(g_callRx) - 2U;
    }
    g_callRx[reply_length++] = (char)byte;
    g_callRx[reply_length] = '\0';
}

static bool send_command(const char *command) {
    snprintf(g_lastCommand, sizeof(g_lastCommand), "%s", command);
    return hal_a7680c_send(command);
}

static void mark_inactive(void) {
    call_active = false;
    g_callActive = 0;
    g_hangupRequest = 0;
}

static void hang_up(CallStage_t final_stage) {
    if (send_command("ATH\r\n")) {
        g_hangupCount++;
        g_callStage = final_stage;
    } else {
        g_callStage = CALL_STAGE_ERROR;
        g_atOK = 0;
    }
    mark_inactive();
}

bool app_call_is_active(void) {
    return call_active;
}

bool app_call_cooldown_over(uint32_t now) {
    return !call_attempted || ((now - last_attempt_ms) >= CALL_INTERVAL_MS);
}

void app_call_update_cooldown(uint32_t now) {
    if (!call_attempted) {
        g_callCooldownMs = 0;
        return;
    }

    uint32_t elapsed = now - last_attempt_ms;
    g_callCooldownMs = (elapsed < CALL_INTERVAL_MS) ? (CALL_INTERVAL_MS - elapsed) : 0U;
}

void app_call_start(const char *phone) {
    char command[CALL_COMMAND_LEN];

    last_attempt_ms = now_ms();
    call_attempted = true;
    g_callAttempts++;

    g_callRequest = 0;
    g_hangupRequest = 0;
    g_callElapsedMs = 0;

    hal_a7680c_drain();
    reset_reply();

    snprintf(command, sizeof(command), "ATD%s;\r\n", phone);

    g_callStage = CALL_STAGE_DIALING;
    if (!send_command(command)) {
        g_callStage = CALL_STAGE_ERROR;
        g_atOK = 0;
        return;
    }

    call_started_ms = now_ms();
    call_active = true;
    g_callActive = 1;
    g_callStage = CALL_STAGE_IN_CALL;
}

void app_call_service(void) {
    g_callElapsedMs = now_ms() - call_started_ms;

    switch (app_modem_classify_reply(g_callRx)) {
    case MODEM_REPLY_MODEM_RESET:
        mark_inactive();
        g_modemResetCount++;
        g_atOK = 0;
        g_callStage = CALL_STAGE_MODEM_RESET;
        reset_reply();
        return;
    case MODEM_REPLY_CALL_ENDED:
        mark_inactive();
        g_callStage = CALL_STAGE_ENDED_BY_MODEM;
        return;
    case MODEM_REPLY_ERROR:
        hang_up(CALL_STAGE_ERROR);
        return;
    case MODEM_REPLY_NONE:
    default:
        break;
    }

    if ((g_hangupRequest != 0U) || (g_callElapsedMs >= CALL_DURATION_MS)) {
        hang_up(CALL_STAGE_HUNG_UP);
    }
}

void app_call_poll_modem(void) {
    uint8_t byte;

    while (hal_a7680c_read_byte(&byte)) {
        append_reply(byte);
        if (!call_active && (app_modem_classify_reply(g_callRx) == MODEM_REPLY_MODEM_RESET)) {
            g_modemResetCount++;
            g_atOK = 0;
            g_callStage = CALL_STAGE_MODEM_BOOT;
            reset_reply();
            return;
        }
    }
}
