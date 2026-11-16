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

static void reset_reply(void) {
    memset(reply, 0, sizeof(reply));
    reply_length = 0;
}

static void append_reply(uint8_t byte) {
    if (reply_length >= (sizeof(reply) - 1U)) {
        memmove(reply, reply + 1, sizeof(reply) - 2U);
        reply_length = sizeof(reply) - 2U;
    }
    reply[reply_length++] = (char)byte;
    reply[reply_length] = '\0';
}

static void fail_modem(void) {
    call_stage = CALL_STAGE_ERROR;
    modem_ready = false;
}

static void mark_inactive(void) {
    call_active = false;
    hangup_requested = false;
}

static void hang_up(CallStage_t final_stage) {
    if (hal_a7680c_send(MODEM_CMD_HANGUP)) {
        call_stage = final_stage;
    } else {
        fail_modem();
    }
    mark_inactive();
}

void app_call_init(void) {
    call_stage = CALL_STAGE_MODEM_BOOT;
    modem_ready = false;
}

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

bool app_call_modem_ready(void) {
    return modem_ready;
}

CallStage_t app_call_stage(void) {
    return call_stage;
}

bool app_call_is_active(void) {
    return call_active;
}

bool app_call_cooldown_over(uint32_t now_ms) {
    return !call_attempted || ((now_ms - last_attempt_ms) >= CALL_INTERVAL_MS);
}

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

void app_call_request_hangup(void) {
    hangup_requested = true;
}

void app_call_service(void) {
    switch (app_modem_reply_classify(reply)) {
    case MODEM_REPLY_MODEM_RESET:
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

    if (hangup_requested || ((app_time_ms() - call_started_ms) >= CALL_DURATION_MS)) {
        hang_up(CALL_STAGE_HUNG_UP);
    }
}

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
