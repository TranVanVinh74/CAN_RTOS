/**
 * @file app_threshold_sync.c
 * @brief Periodic and change-triggered CAN publication of active thresholds.
 */

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

/**
 * @brief Reset transmission tracking so the next update sends the active thresholds.
 */
void app_threshold_sync_init(void) {
    sent = false;
    attempted = false;
}

/**
 * @brief Publish changed thresholds or a one-second refresh, spacing failed attempts by at least 100 ms.
 * @param[in] thresholds Input gas and temperature activation thresholds; must be valid for policy evaluation.
 * @param[in] now_ms Current monotonic time in milliseconds; unsigned subtraction handles wraparound.
 */
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
