/**
 * @file app_ui.h
 * @brief Display scenes, draft settings and incremental animated UI rendering.
 */

#ifndef APP_UI_H
#define APP_UI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_THRESHOLD_GAS,
    UI_THRESHOLD_TEMPERATURE,
    UI_THRESHOLD_COUNT
} UiThreshold_t;

typedef struct {
    uint32_t sample_id;
    bool bus_error;
    bool has_data;
    bool stale;
    uint8_t temperature_c;
    uint8_t humidity_pct;
    uint16_t gas_raw;
    uint8_t sensor_faults;
    bool fan_on;
    uint16_t gas_limit_raw;
    uint8_t temperature_limit_c;
    bool edit_mode;
    bool settings_save_error;
    UiThreshold_t selected_threshold;
} UiView_t;

typedef enum {
    UI_BUTTON_SELECT,
    UI_BUTTON_PLUS,
    UI_BUTTON_MINUS,
    UI_BUTTON_SAVE,
    UI_BUTTON_COUNT
} UiButton_t;

/**
 * @brief Reset rendering, startup animation, filtering and tile-cache state.
 */
void app_ui_reset(void);
/**
 * @brief Store the target UI snapshot and update display-only gas smoothing without drawing.
 * @param[in] view Input snapshot of readings, active or draft thresholds, sensor faults and connection state.
 */
void app_ui_update(const UiView_t *view);
/**
 * @brief Advance animations and stream changed UI tiles to the TFT.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void app_ui_draw(uint32_t now_ms);
/**
 * @brief Apply a button to draft settings and edit mode in the supplied view only.
 * @param[in,out] view Input/output UI state; button actions modify draft thresholds or edit mode.
 * @param[in] button UI button event to apply.
 */
void app_ui_button(UiView_t *view, UiButton_t button);

#endif
