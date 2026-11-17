/**
 * @file app_call.h
 * @brief Voice-call commands, response handling, timeout and retry cooldown.
 */

#ifndef APP_CALL_H
#define APP_CALL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CALL_STAGE_MODEM_BOOT,
    CALL_STAGE_AT_CHECK,
    CALL_STAGE_READY,
    CALL_STAGE_DIALING,
    CALL_STAGE_IN_CALL,
    CALL_STAGE_HUNG_UP,
    CALL_STAGE_ENDED_BY_MODEM,
    CALL_STAGE_MODEM_RESET,
    CALL_STAGE_ERROR
} CallStage_t;

/**
 * @brief Initialize the call stage to modem boot and clear modem readiness.
 */
void app_call_init(void);
/**
 * @brief Drain pending UART data, issue an AT readiness probe and update the call stage.
 * @return true if the modem replies OK; this does not verify SIM or network registration.
 */
bool app_call_check_modem(void);
/**
 * @brief Read the cached result of the latest modem readiness probe.
 * @return true if the modem is currently considered ready; otherwise false.
 */
bool app_call_modem_ready(void);
/**
 * @brief Read the current software call lifecycle stage.
 * @return Current CallStage_t value.
 */
CallStage_t app_call_stage(void);
/**
 * @brief Read whether the application currently tracks a call attempt as active.
 * @return true during a software-tracked call; this does not confirm the remote party answered.
 */
bool app_call_is_active(void);
/**
 * @brief Check that no call has been attempted or the minimum interval has elapsed.
 * @param[in] now_ms Current scheduler time in milliseconds.
 * @return true if another call attempt is allowed.
 */
bool app_call_cooldown_over(uint32_t now_ms);
/**
 * @brief Record a call attempt, send ATD and track the call if UART transmission succeeds.
 * @param[in] phone Null-terminated destination phone number inserted into the voice-dial command.
 */
void app_call_start(const char *phone);
/**
 * @brief Set a hang-up request for the next call service step.
 */
void app_call_request_hangup(void);
/**
 * @brief Handle modem reset/end/error responses and request hang-up after 30 seconds or an explicit request.
 */
void app_call_service(void);
/**
 * @brief Consume buffered UART bytes and detect modem reset indications while idle.
 */
void app_call_poll_modem(void);

#endif
