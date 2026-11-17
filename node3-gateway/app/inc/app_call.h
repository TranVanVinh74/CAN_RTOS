/**
 * @file app_call.h
 * @brief Voice-call commands, response handling, timeout and retry cooldown.
 */

#ifndef APP_CALL_H
#define APP_CALL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CALL_STAGE_NOT_STARTED = 0,
    CALL_STAGE_MODEM_BOOT = 1,
    CALL_STAGE_AT_CHECK = 2,
    CALL_STAGE_READY = 3,
    CALL_STAGE_DIALING = 4,
    CALL_STAGE_IN_CALL = 5,
    CALL_STAGE_HUNG_UP = 6,
    CALL_STAGE_ENDED_BY_MODEM = 7,
    CALL_STAGE_MODEM_RESET = 8,
    CALL_STAGE_ERROR = 90
} CallStage_t;

extern volatile uint32_t g_callStage;
extern volatile uint32_t g_atOK;
extern volatile uint32_t g_callRequest;
extern volatile uint32_t g_hangupRequest;

/**
 * @brief Read whether the application currently tracks a call attempt as active.
 * @return true during a software-tracked call; this does not confirm the remote party answered.
 */
bool app_call_is_active(void);
/**
 * @brief Check whether no call has been attempted or 60 seconds have elapsed since the last attempt.
 * @param[in] now Current scheduler time in milliseconds.
 * @return true if another call attempt is permitted.
 */
bool app_call_cooldown_over(uint32_t now);
/**
 * @brief Update the diagnostic number of milliseconds remaining before another call attempt.
 * @param[in] now Current scheduler time in milliseconds.
 */
void app_call_update_cooldown(uint32_t now);
/**
 * @brief Record an attempt, send ATD and mark the call active if UART transmission succeeds.
 * @param[in] phone Null-terminated destination phone number inserted into the ATD voice-call command.
 */
void app_call_start(const char *phone);
/**
 * @brief Handle modem reset/end/error responses and request hang-up after 30 seconds or an explicit request.
 */
void app_call_service(void);
/**
 * @brief Consume buffered UART bytes and detect modem reset indications while idle.
 */
void app_call_poll_modem(void);

#endif
