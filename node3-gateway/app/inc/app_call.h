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

void app_call_init(void);
bool app_call_check_modem(void);
bool app_call_modem_ready(void);
CallStage_t app_call_stage(void);
bool app_call_is_active(void);
bool app_call_cooldown_over(uint32_t now_ms);
void app_call_start(const char *phone);
void app_call_request_hangup(void);
void app_call_service(void);
void app_call_poll_modem(void);

#endif
