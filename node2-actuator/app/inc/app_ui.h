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

void app_ui_reset(void);
void app_ui_update(const UiView_t *view);
void app_ui_draw(uint32_t now_ms);
void app_ui_button(UiView_t *view, UiButton_t button);

#endif
