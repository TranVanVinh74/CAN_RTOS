#ifndef APP_UI_ASSETS_H
#define APP_UI_ASSETS_H

#include <stdint.h>

typedef struct {
    uint16_t code;
    uint16_t offset;
    uint8_t width;
    uint8_t advance;
} UiGlyph_t;

typedef struct {
    const uint8_t *mask;
    const UiGlyph_t *glyphs;
    uint8_t count;
    uint8_t height;
} UiFont_t;

extern const UiFont_t ui_font_small;
extern const UiFont_t ui_font_label;
extern const UiFont_t ui_font_value;
extern const UiFont_t ui_font_status;
extern const UiFont_t ui_font_gas;
typedef struct {
    const uint8_t *mask;
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
} UiArc_t;
extern const UiArc_t ui_arc[61];
extern const UiArc_t ui_ticks[13];
extern const uint8_t ui_icon_gas[98];
extern const uint8_t ui_icon_hot[98];
extern const uint8_t ui_icon_link[98];
extern const uint8_t ui_icon_fan[98];
extern const uint8_t ui_icon_fan_1[98];
extern const uint8_t ui_icon_fan_2[98];
extern const uint8_t ui_icon_fan_3[98];
extern const uint8_t ui_icon_up[144];
extern const uint8_t ui_icon_down[144];
extern const uint8_t ui_icon_next[128];
extern const uint8_t ui_icon_add[128];
extern const uint8_t ui_icon_remove[128];
extern const uint8_t ui_icon_check[128];
extern const uint8_t ui_state_safe[98];
extern const uint8_t ui_state_warning[98];
extern const uint8_t ui_state_danger[98];
extern const uint8_t ui_state_offline[98];
extern const uint8_t ui_state_waiting[98];

#endif
