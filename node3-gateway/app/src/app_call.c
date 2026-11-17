/**
 * @file app_call.c
 * @brief Voice-call commands, response handling, timeout and retry cooldown.
 */

#include "app_call.h"

#include <stdio.h>
#include <string.h>

#include "app_cfg.h"
#include "app_modem_reply.h"
#include "app_time.h"
#include "hal_a7680c.h"

#define CALL_COMMAND_LEN 64U
#define CALL_REPLY_LEN 256U
#define MODEM_CMD_HANGUP "ATH\r\n"
#define MODEM_CMD_DIAL "ATD%s;\r\n"

static CallStage_t call_stage = CALL_STAGE_MODEM_BOOT;
static bool modem_ready = false;
static bool call_active = false;
static bool hangup_requested = false;
static bool call_attempted = false;
static uint32_t call_started_ms = 0;
static uint32_t last_attempt_ms = 0;
static char reply[CALL_REPLY_LEN];
static uint16_t reply_length = 0;

/**
 * @brief Clear the accumulated modem response text and its length.
 */
static void reset_reply(void) {
    memset(reply, 0, sizeof(reply));
    reply_length = 0;
}

/**
 * @brief Append a modem byte, dropping the oldest byte when the reply buffer is full.
 * @param[in] byte Received UART byte to append.
 */
static void append_reply(uint8_t byte) {
    if (reply_length >= (sizeof(reply) - 1U)) {
        memmove(reply, reply + 1, sizeof(reply) - 2U);
        reply_length = sizeof(reply) - 2U;
    }
    reply[reply_length++] = (char)byte;
    reply[reply_length] = '\0';
}

/**
 * @brief Mark the modem unavailable and move the call state to its error stage.
 */
static void fail_modem(void) {
    call_stage = CALL_STAGE_ERROR;
    modem_ready = false;
}

/**
 * @brief Clear the software call-active state and pending hang-up request.
 */
static void mark_inactive(void) {
    call_active = false;
    hangup_requested = false;
}

/**
 * @brief Send ATH once and clear software call state without waiting for modem confirmation.
 * @param[in] final_stage Diagnostic call stage to set when ATH transmission succeeds.
 */
static void hang_up(CallStage_t final_stage) {
    if (hal_a7680c_send(MODEM_CMD_HANGUP)) {
        call_stage = final_stage;
    } else {
        fail_modem();
    }
    mark_inactive();
}

/**
 * @brief Initialize the call stage to modem boot and clear modem readiness.
 */
void app_call_init(void) {
    call_stage = CALL_STAGE_MODEM_BOOT;
    modem_ready = false;
}

/**
 * @brief Drain pending UART data, issue an AT readiness probe and update the call stage.
 * @return true if the modem replies OK; this does not verify SIM or network registration.
 */
bool app_call_check_modem(void) {
    call_stage = CALL_STAGE_AT_CHECK;
    modem_ready = false;

    hal_a7680c_drain();
    if (hal_a7680c_is_ready()) {
        modem_ready = true;
        call_stage = CALL_STAGE_READY;
    }
    return modem_ready;
}

/**
 * @brief Read the cached result of the latest modem readiness probe.
 * @return true if the modem is currently considered ready; otherwise false.
 */
bool app_call_modem_ready(void) {
    return modem_ready;
}

/**
 * @brief Read the current software call lifecycle stage.
 * @return Current CallStage_t value.
 */
CallStage_t app_call_stage(void) {
    return call_stage;
}

/**
 * @brief Read whether the application currently tracks a call attempt as active.
 * @return true during a software-tracked call; this does not confirm the remote party answered.
 */
bool app_call_is_active(void) {
    return call_active;
}

/**
 * @brief Check that no call has been attempted or the minimum interval has elapsed.
 * @param[in] now_ms Current scheduler time in milliseconds.
 * @return true if another call attempt is allowed.
 */
bool app_call_cooldown_over(uint32_t now_ms) {
    return !call_attempted || ((now_ms - last_attempt_ms) >= CALL_INTERVAL_MS);
}

/**
 * @brief Record a call attempt, send ATD and track the call if UART transmission succeeds.
 * @param[in] phone Null-terminated destination phone number inserted into the voice-dial command.
 */
void app_call_start(const char *phone) {
    char command[CALL_COMMAND_LEN];

    last_attempt_ms = app_time_ms();
    call_attempted = true;
    hangup_requested = false;

    hal_a7680c_drain();
    reset_reply();

    snprintf(command, sizeof(command), MODEM_CMD_DIAL, phone);

    call_stage = CALL_STAGE_DIALING;
    if (!hal_a7680c_send(command)) {
        fail_modem();
        return;
    }

    call_started_ms = app_time_ms();
    call_active = true;
    call_stage = CALL_STAGE_IN_CALL;
}

/**
 * @brief Set a hang-up request for the next call service step.
 */
void app_call_request_hangup(void) {
    hangup_requested = true;
}

/**
 * @brief Handle modem reset/end/error responses and request hang-up after 30 seconds or an explicit request.
 */
void app_call_service(void) {
    switch (app_modem_reply_classify(reply)) {
    case MODEM_REPLY_MODEM_RESET:
        /* Boot messages mid-call: the modem rebooted (supply dip) and the call is gone. */
        mark_inactive();
        modem_ready = false;
        call_stage = CALL_STAGE_MODEM_RESET;
        reset_reply();
        return;
    case MODEM_REPLY_CALL_ENDED:
        mark_inactive();
        call_stage = CALL_STAGE_ENDED_BY_MODEM;
        return;
    case MODEM_REPLY_ERROR:
        hang_up(CALL_STAGE_ERROR);
        return;
    case MODEM_REPLY_NONE:
    default:
        break;
    }

    /* Some networks report nothing when the callee rejects, so every call is capped. */
    if (hangup_requested || ((app_time_ms() - call_started_ms) >= CALL_DURATION_MS)) {
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
        if (!call_active && (app_modem_reply_classify(reply) == MODEM_REPLY_MODEM_RESET)) {
            modem_ready = false;
            call_stage = CALL_STAGE_MODEM_BOOT;
            reset_reply();
            return;
        }
    }
}
