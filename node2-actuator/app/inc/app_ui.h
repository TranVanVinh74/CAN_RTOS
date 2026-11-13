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

void app_ui_button(UiView_t *view, UiButton_t button);
void app_ui_tick(uint32_t now_ms);
void app_ui_reset(void);
void app_ui_render(const UiView_t *view);

#ifdef UI_PREVIEW
uint16_t app_ui_preview_gas(void);
uint8_t app_ui_preview_state(void);
uint16_t app_ui_preview_gauge(void);
uint8_t app_ui_preview_arc_percent(uint8_t segment);
uint16_t app_ui_preview_arc_color(uint8_t segment);
void app_ui_preview_redraw(void);
#endif

#endif
