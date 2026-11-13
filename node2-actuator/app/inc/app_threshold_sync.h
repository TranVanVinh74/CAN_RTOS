#ifndef APP_THRESHOLD_SYNC_H
#define APP_THRESHOLD_SYNC_H

#include <stdint.h>

#include "alert_thresholds.h"

extern volatile uint32_t g_thresholdTxQueuedCount;
extern volatile uint32_t g_thresholdTxFailCount;

void app_threshold_sync_init(void);
void app_threshold_sync_update(const AlertThresholds_t *thresholds, uint32_t now_ms);

#endif
