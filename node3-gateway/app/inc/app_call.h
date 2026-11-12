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

bool app_call_is_active(void);
bool app_call_cooldown_over(uint32_t now);
void app_call_update_cooldown(uint32_t now);
void app_call_start(const char *phone);
void app_call_service(void);
void app_call_poll_modem(void);

#endif
