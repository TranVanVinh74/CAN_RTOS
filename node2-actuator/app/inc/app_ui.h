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
    UI_THRESHOLD_TEMPERATURE
} UiThreshold_t;

typedef struct {
    uint32_t sample_id;
    bool bus_error;
    bool has_data;
    bool stale;
    uint8_t temperature;
    uint8_t humidity;
    uint16_t gas_raw;
    uint8_t sensor_faults;
    bool fan_on;
    uint16_t gas_threshold;
    uint8_t temperature_threshold;
    bool temperature_threshold_configured;
    bool edit_mode;
    bool settings_save_error;
    UiThreshold_t selected_threshold;
} UiView_t;

typedef enum {
    UI_BUTTON_SELECT,
    UI_BUTTON_PLUS,
    UI_BUTTON_MINUS,
    UI_BUTTON_SAVE
} UiButton_t;

/**
 * @brief Apply a UI button to draft settings and edit mode; persistence is handled by the actuator task.
 * @param[in,out] view Input/output UI state; button actions modify draft thresholds or edit mode.
 * @param[in] button UI button event to apply.
 */
void app_ui_button(UiView_t *view, UiButton_t button);
/**
 * @brief Advance startup, blinking, fan and transition animations and render changed tiles.
 * @param[in] now_ms Current monotonic time in milliseconds; unsigned subtraction handles wraparound.
 */
void app_ui_tick(uint32_t now_ms);
/**
 * @brief Reset rendering, startup animation, filtering and tile-cache state.
 */
void app_ui_reset(void);
/**
 * @brief Store the target view and update the display-only gas smoothing filter without drawing.
 * @param[in] view Input UI snapshot containing readings, thresholds and connection state.
 */
void app_ui_render(const UiView_t *view);

#ifdef UI_PREVIEW
/**
 * @brief Expose the filtered numeric gas percentage for host previews.
 * @return Filtered numeric gas percentage.
 */
uint16_t app_ui_preview_gas(void);
/**
 * @brief Expose the target UI state for host previews.
 * @return Current UiState_t value encoded as uint8_t.
 */
uint8_t app_ui_preview_state(void);
/**
 * @brief Expose the clamped animated gauge percentage for host previews.
 * @return Gauge percentage clamped to 150.
 */
uint16_t app_ui_preview_gauge(void);
/**
 * @brief Expose the percentage associated with a gauge arc segment for host previews.
 * @param[in] segment Zero-based gauge arc index; must be less than UI_ARC_COUNT.
 * @return Percentage at the selected arc segment.
 */
uint8_t app_ui_preview_arc_percent(uint8_t segment);
/**
 * @brief Expose a composed gauge arc color for host previews.
 * @param[in] segment Zero-based gauge arc index; must be less than UI_ARC_COUNT.
 * @return RGB565 color of the selected segment.
 */
uint16_t app_ui_preview_arc_color(uint8_t segment);
/**
 * @brief Invalidate all cached tile hashes so the host preview redraws the scene.
 */
void app_ui_preview_redraw(void);
#endif

#endif
