#include "app_threshold_sync.h"

#include <stdbool.h>

#include "app_cfg.h"
#include "can_protocol.h"
#include "hal_can.h"

volatile uint32_t g_thresholdTxQueuedCount = 0U;
volatile uint32_t g_thresholdTxFailCount = 0U;

static AlertThresholds_t last_sent;
static uint32_t last_sent_ms;
static uint32_t last_attempt_ms;
static bool sent;
static bool attempted;

void app_threshold_sync_init(void) {
    sent = false;
    attempted = false;
}

void app_threshold_sync_update(const AlertThresholds_t *thresholds, uint32_t now_ms) {
    if (!alert_thresholds_valid(thresholds)) {
        return;
    }
    bool changed = !sent || (thresholds->gas_limit_raw != last_sent.gas_limit_raw) ||
                   (thresholds->temperature_limit_c != last_sent.temperature_limit_c);
    if ((!changed && ((uint32_t)(now_ms - last_sent_ms) < THRESHOLD_SYNC_INTERVAL_MS)) ||
        (attempted && ((uint32_t)(now_ms - last_attempt_ms) < THRESHOLD_SYNC_RETRY_MS))) {
        return;
    }

    uint8_t data[CAN_DLC_THRESHOLDS];
    can_pack_thresholds(thresholds->gas_limit_raw, thresholds->temperature_limit_c, data);
    last_attempt_ms = now_ms;
    attempted = true;
    if (hal_can_send(CAN_ID_THRESHOLDS, data, CAN_DLC_THRESHOLDS)) {
        last_sent = *thresholds;
        last_sent_ms = now_ms;
        sent = true;
        attempted = false;
        g_thresholdTxQueuedCount++;
    } else {
        g_thresholdTxFailCount++;
    }
}
