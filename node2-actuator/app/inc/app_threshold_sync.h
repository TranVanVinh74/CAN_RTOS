/**
 * @file app_threshold_sync.h
 * @brief Periodic and change-triggered CAN publication of active thresholds.
 */

#ifndef APP_THRESHOLD_SYNC_H
#define APP_THRESHOLD_SYNC_H

#include <stdint.h>

#include "alert_thresholds.h"

extern volatile uint32_t g_thresholdTxQueuedCount;
extern volatile uint32_t g_thresholdTxFailCount;

/**
 * @brief Reset transmission tracking so the next update sends the active thresholds.
 */
void app_threshold_sync_init(void);
/**
 * @brief Publish changed thresholds or a one-second refresh, spacing failed attempts by at least 100 ms.
 * @param[in] thresholds Input gas and temperature activation thresholds; must be valid for policy evaluation.
 * @param[in] now_ms Current monotonic time in milliseconds; unsigned subtraction handles wraparound.
 */
void app_threshold_sync_update(const AlertThresholds_t *thresholds, uint32_t now_ms);

#endif
