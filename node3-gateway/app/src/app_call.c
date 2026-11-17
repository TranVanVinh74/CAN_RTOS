/**
 * @file app_call.c
 * @brief Voice-call commands, response handling, timeout and retry cooldown.
 */

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

/**
 * @brief Convert the FreeRTOS tick count into milliseconds.
 * @return Current scheduler time in milliseconds, with unsigned wraparound.
 */
static uint32_t now_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/**
 * @brief Clear the accumulated modem response text and its length.
 */
static void reset_reply(void) {
    memset(g_callRx, 0, sizeof(g_callRx));
    reply_length = 0;
}

/**
 * @brief Append a modem byte, dropping the oldest byte when the reply buffer is full.
 * @param[in] byte Received UART byte to append.
 */
static void append_reply(uint8_t byte) {
    if (reply_length >= (sizeof(g_callRx) - 1U)) {
        memmove(g_callRx, g_callRx + 1, sizeof(g_callRx) - 2U);
        reply_length = sizeof(g_callRx) - 2U;
    }
    g_callRx[reply_length++] = (char)byte;
    g_callRx[reply_length] = '\0';
}

/**
 * @brief Record the command in diagnostics and transmit it to the modem.
 * @param[in] command Null-terminated modem command including the required line terminator.
 * @return true if the UART command transmission succeeds; otherwise false.
 */
static bool send_command(const char *command) {
    snprintf(g_lastCommand, sizeof(g_lastCommand), "%s", command);
    return hal_a7680c_send(command);
}

/**
 * @brief Clear the software call-active state and pending hang-up request.
 */
static void mark_inactive(void) {
    call_active = false;
    g_callActive = 0;
    g_hangupRequest = 0;
}

/**
 * @brief Send ATH once and clear software call state without waiting for modem confirmation.
 * @param[in] final_stage Diagnostic call stage to set when ATH transmission succeeds.
 */
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

/**
 * @brief Read whether the application currently tracks a call attempt as active.
 * @return true during a software-tracked call; this does not confirm the remote party answered.
 */
bool app_call_is_active(void) {
    return call_active;
}

/**
 * @brief Check whether no call has been attempted or 60 seconds have elapsed since the last attempt.
 * @param[in] now Current scheduler time in milliseconds.
 * @return true if another call attempt is permitted.
 */
bool app_call_cooldown_over(uint32_t now) {
    return !call_attempted || ((now - last_attempt_ms) >= CALL_INTERVAL_MS);
}

/**
 * @brief Update the diagnostic number of milliseconds remaining before another call attempt.
 * @param[in] now Current scheduler time in milliseconds.
 */
void app_call_update_cooldown(uint32_t now) {
    if (!call_attempted) {
        g_callCooldownMs = 0;
        return;
    }

    uint32_t elapsed = now - last_attempt_ms;
    g_callCooldownMs = (elapsed < CALL_INTERVAL_MS) ? (CALL_INTERVAL_MS - elapsed) : 0U;
}

/**
 * @brief Record an attempt, send ATD and mark the call active if UART transmission succeeds.
 * @param[in] phone Null-terminated destination phone number inserted into the ATD voice-call command.
 */
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

/**
 * @brief Handle modem reset/end/error responses and request hang-up after 30 seconds or an explicit request.
 */
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

/**
 * @brief Consume buffered UART bytes and detect modem reset indications while idle.
 */
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
