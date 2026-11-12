#include "app_ui.h"

#ifdef UI_PREVIEW
#include <assert.h>
#endif
#include <stddef.h>

#include "app_ui_assets.h"
#include "hal_tft_graphics.h"

#define UI_GAS_EMA_DIVISOR 4U
#define UI_GAS_DEADBAND_PERCENT 2U
#define UI_GAS_GAUGE_FULL_SCALE_PERCENT 150U
#define UI_GAS_DISPLAY_MAX_PERCENT 999U
#define UI_GAS_THRESHOLD_MIN 1U
#define UI_GAS_FIXED_SCALE 256U

#define UI_BG 0x0842U
#define UI_INK 0xEF9EU
#define UI_MUTED 0x8493U
#define UI_LINE 0x2967U
#define UI_GREEN 0x4F56U
#define UI_AMBER 0xFDC8U
#define UI_RED 0xFA08U
#define UI_CYAN 0x5E9FU
#define UI_PANEL 0x10C4U
#define UI_GAS_MAX 4095U
#define UI_WARNING_PERCENT 80U
#define UI_PERCENT_FULL 100U
#define UI_BLINK_PERIOD_MS 250U
#define UI_FAN_FRAME_MS 120U
#define UI_FAN_FRAME_COUNT 4U
#define UI_GAS_STEP_RAW 50U
#define UI_TEMPERATURE_INITIAL_C 40U
#define UI_TEMPERATURE_MAX_C 125U
#define UI_RED_DIM 0x8004U
#define UI_SELECTED_PANEL 0x1947U
#define UI_HASH_OFFSET 2166136261U
#define UI_HASH_PRIME 16777619U
#define UI_OP_CAPACITY 192U
#define UI_LABEL_CAPACITY 24U
#define UI_TILE_SIZE 8U
#define UI_TILE_COLUMNS (TFT_WIDTH / UI_TILE_SIZE)
#define UI_TILE_COUNT (UI_TILE_COLUMNS * (TFT_HEIGHT / UI_TILE_SIZE))
#define UI_TILES_PER_FRAME 80U
#define UI_SWEEP_MS 1280U
#define UI_LAYOUT_MS 200U
#define UI_WIPE_STEP_MS 40U
#define UI_WIPE_BAND_PX 40U
#define UI_SCREEN_HEIGHT TFT_HEIGHT

typedef enum {
    UI_SAFE,
    UI_WARNING,
    UI_DANGER,
    UI_OFFLINE,
    UI_WAITING,
    UI_BUS_ERROR
} UiState_t;

typedef struct {
    const uint8_t *mask;
    uint16_t color;
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
} UiOp_t;

static UiView_t target;
static bool valid;
static bool clock_started;
static uint32_t start_ms;
static int32_t filtered_gas;
static uint16_t display_gas;
static bool filter_valid;
static uint16_t shown_gas;
static UiOp_t ops[UI_OP_CAPACITY];
static uint8_t op_count;
static char label[UI_LABEL_CAPACITY];
static uint8_t label_length;
static uint32_t tile_hash[UI_TILE_COUNT];
static uint32_t next_tile_hash[UI_TILE_COUNT];
static uint8_t tile_valid[UI_TILE_COUNT / 8U];
static uint16_t tile_pixels[UI_TILE_SIZE * UI_TILE_SIZE];
static uint16_t scan_tile;
static uint16_t pending_tiles;
static uint32_t scene_hash;
static bool blink_on;
static bool lamp_check;
static uint8_t fan_frame;
static uint8_t screen_mode;
static uint8_t reveal_height;
static uint32_t transition_ms;
static bool screen_started;

static void append(const char *value) {
    while (*value != '\0' && label_length < UI_LABEL_CAPACITY - 1U) {
        label[label_length++] = *value++;
    }
    label[label_length] = '\0';
}

static void set_label(const char *value) {
    label_length = 0U;
    append(value);
}

static void number(uint16_t value) {
    uint16_t divisor = 10000U;
    while (divisor > 1U && value < divisor) {
        divisor /= 10U;
    }
    do {
        if (label_length < UI_LABEL_CAPACITY - 1U) {
            label[label_length++] = (char)('0' + value / divisor);
        }
        value %= divisor;
        divisor /= 10U;
    } while (divisor != 0U);
    label[label_length] = '\0';
}

static const UiGlyph_t *glyph(const UiFont_t *font, uint16_t code) {
    for (uint8_t i = 0U; i < font->count; i++) {
        if (font->glyphs[i].code == code) {
            return &font->glyphs[i];
        }
    }
    return NULL;
}

static uint16_t next_code(const char **text) {
    uint16_t code = (uint8_t)*(*text)++;
    if (code == 0xC2U && (uint8_t)**text == 0xB0U) {
        (*text)++;
        code = 0xB0U;
    }
    return code;
}

static uint8_t text_width(const UiFont_t *font, const char *text) {
    uint16_t width = 0U;
    while (*text != '\0') {
        const UiGlyph_t *item = glyph(font, next_code(&text));
        if (item != NULL) {
            width += item->advance;
        }
    }
    return (uint8_t)width;
}

static void begin(void) {
    op_count = 0U;
}

static void add(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                const uint8_t *mask, uint16_t color) {
#ifdef UI_PREVIEW
    assert(op_count < UI_OP_CAPACITY);
    assert((uint16_t)x + width <= TFT_WIDTH);
#endif
    if (op_count < UI_OP_CAPACITY) {
        ops[op_count++] = (UiOp_t){mask, color, x, y, width, height};
    }
}

static void text(uint8_t x, uint8_t y, const char *value, const UiFont_t *font,
                 uint16_t color) {
    while (*value != '\0') {
        const UiGlyph_t *item = glyph(font, next_code(&value));
        if (item != NULL) {
            add(x, y, item->width, font->height, font->mask + item->offset, color);
            x += item->advance;
        }
    }
}

static void centered(uint8_t y, const char *value, const UiFont_t *font, uint16_t color) {
    text((uint8_t)((TFT_WIDTH - text_width(font, value)) / 2U), y, value, font, color);
}

static uint16_t blend(uint16_t fg, uint16_t bg, uint8_t alpha) {
    uint32_t inverse = 15U - alpha;
    uint32_t r = ((fg >> 11) * alpha + (bg >> 11) * inverse + 7U) / 15U;
    uint32_t g = (((fg >> 5) & 63U) * alpha + ((bg >> 5) & 63U) * inverse + 7U) / 15U;
    uint32_t b = ((fg & 31U) * alpha + (bg & 31U) * inverse + 7U) / 15U;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint16_t gas_percent(const UiView_t *view) {
    uint32_t threshold = view->gas_threshold < UI_GAS_THRESHOLD_MIN ?
                         UI_GAS_THRESHOLD_MIN : view->gas_threshold;
    uint32_t percent = ((uint32_t)view->gas_raw * UI_PERCENT_FULL + threshold / 2U) / threshold;
    return percent > UI_GAS_DISPLAY_MAX_PERCENT ? UI_GAS_DISPLAY_MAX_PERCENT : percent;
}

static uint16_t gauge_percent(void) {
    return shown_gas > UI_GAS_GAUGE_FULL_SCALE_PERCENT ?
           UI_GAS_GAUGE_FULL_SCALE_PERCENT : shown_gas;
}

static uint8_t arc_percent(uint8_t segment) {
    return (uint16_t)segment * UI_GAS_GAUGE_FULL_SCALE_PERCENT / (UI_ARC_COUNT - 1U);
}

static UiState_t gas_state(const UiView_t *view) {
    if (view->gas_raw > view->gas_threshold) {
        return UI_DANGER;
    }
    return (uint32_t)view->gas_raw * UI_PERCENT_FULL >=
           (uint32_t)view->gas_threshold * UI_WARNING_PERCENT ? UI_WARNING : UI_SAFE;
}

static UiState_t state(const UiView_t *view) {
    if (view->bus_error) {
        return UI_BUS_ERROR;
    }
    if (!view->has_data) {
        return UI_WAITING;
    }
    if (view->stale) {
        return UI_OFFLINE;
    }
    if (gas_state(view) == UI_DANGER ||
        (view->temperature_threshold_configured &&
         view->temperature > view->temperature_threshold)) {
        return UI_DANGER;
    }
    return gas_state(view);
}

static uint16_t state_color(UiState_t current) {
    static const uint16_t colors[] = {UI_CYAN, UI_AMBER, UI_RED, UI_MUTED, UI_MUTED, UI_AMBER};
    return colors[current];
}

static bool available(const UiView_t *view) {
    return view->has_data && !view->stale && !view->bus_error;
}

static void value_label(uint16_t value, bool present, const char *suffix) {
    set_label("");
    if (present) {
        number(value);
    } else {
        append("--");
    }
    append(suffix);
}

static void main_scene(void) {
    static const char *const words[] = {"SAFE", "WARNING", "DANGER", "OFFLINE", "WAITING"};
    static const uint8_t *const fans[] = {
        ui_icon_fan, ui_icon_fan_1, ui_icon_fan_2, ui_icon_fan_3
    };
    UiState_t current = state(&target);
    bool live = available(&target);
    bool gas_alarm = live && target.gas_raw > target.gas_threshold;
    bool gas_warn = live && (uint32_t)target.gas_raw * UI_PERCENT_FULL >=
                           (uint32_t)target.gas_threshold * UI_WARNING_PERCENT;
    bool hot = live && target.temperature_threshold_configured &&
               target.temperature > target.temperature_threshold;
    bool alarm_lit = blink_on || current != UI_DANGER;
    uint16_t gas_ink = gas_alarm ? UI_RED : UI_AMBER;
    add(17U, 5U, 14U, 14U, ui_icon_gas,
        lamp_check || (gas_warn && alarm_lit) ? gas_ink : UI_LINE);
    add(44U, 5U, 14U, 14U, ui_icon_hot,
        lamp_check || (hot && alarm_lit) ? UI_RED : UI_LINE);
    add(71U, 5U, 14U, 14U, fans[fan_frame],
        lamp_check || (target.has_data && target.fan_on) ? UI_GREEN : UI_LINE);
    add(98U, 5U, 14U, 14U, ui_icon_link,
        lamp_check || live ? UI_CYAN : UI_LINE);
    uint16_t gas = gauge_percent();
    for (uint8_t i = 0U; i < UI_ARC_COUNT; i++) {
        const UiArc_t *arc = &ui_arc[i];
        bool redline = arc_percent(i) >= UI_PERCENT_FULL;
        bool lit = (uint32_t)i * UI_GAS_GAUGE_FULL_SCALE_PERCENT <=
                   (uint32_t)gas * (UI_ARC_COUNT - 1U) &&
                   (live || lamp_check);
        uint16_t color = redline ? (lit ? UI_RED : UI_RED_DIM) : (lit ? UI_CYAN : UI_LINE);
        add(arc->x, arc->y, arc->width, arc->height, arc->mask, color);
    }
    for (uint8_t i = 0U; i < UI_TICK_COUNT; i++) {
        const UiArc_t *tick = &ui_ticks[i];
        bool redline = (uint32_t)i * UI_GAS_GAUGE_FULL_SCALE_PERCENT >=
                       UI_PERCENT_FULL * (UI_TICK_COUNT - 1U);
        add(tick->x, tick->y, tick->width, tick->height, tick->mask,
            redline ? UI_RED : UI_MUTED);
    }
    centered(43U, current == UI_WAITING ? "GAS SAFETY" : "GAS", &ui_font_label, UI_MUTED);
    value_label(lamp_check ? shown_gas : display_gas, live || lamp_check, "");
    text(94U - text_width(&ui_font_gas, label), 56U, label, &ui_font_gas,
         live || lamp_check ? UI_INK : UI_MUTED);
    if (live || lamp_check) {
        text(94U, 72U, "%", &ui_font_label, UI_MUTED);
    }
    uint16_t status_ink = state_color(current);
    if (current == UI_DANGER && !blink_on) {
        status_ink = UI_RED_DIM;
    }
    const char *word = lamp_check ? "READY" : words[current];
    static const uint8_t *const cues[] = {
        ui_state_safe, ui_state_warning, ui_state_danger, ui_state_offline, ui_state_waiting
    };
    uint8_t status_x = (128U - text_width(&ui_font_status, word) - 18U) / 2U;
    add(status_x, 87U, 14U, 14U, cues[lamp_check ? UI_SAFE : current], status_ink);
    text(status_x + 18U, 88U, word, &ui_font_status, status_ink);
    add(8U, 122U, 112U, 1U, NULL, UI_LINE);
    add(74U, 127U, 1U, 27U, NULL, UI_LINE);
    text(8U, 127U, "TEMP", &ui_font_small, UI_MUTED);
    if (target.temperature_threshold_configured) {
        set_label("MAX ");
        number(target.temperature_threshold);
        append("°");
        text(73U - text_width(&ui_font_small, label), 127U, label, &ui_font_small, UI_MUTED);
    }
    text(81U, 125U, "HUM", &ui_font_label, UI_MUTED);
    value_label(target.temperature, live, "°C");
    text(69U - text_width(&ui_font_value, label), 139U, label, &ui_font_value,
         hot ? UI_RED : UI_INK);
    value_label(target.humidity > 100U ? 100U : target.humidity, live, "%");
    text(120U - text_width(&ui_font_value, label), 139U, label, &ui_font_value, UI_INK);
}

static void settings_scene(void) {
    centered(9U, "SETTINGS", &ui_font_status, UI_INK);
    centered(24U, "SAFETY LIMITS", &ui_font_label, UI_MUTED);
    for (uint8_t i = 0U; i < 2U; i++) {
        bool gas = i == 0U;
        bool selected = target.selected_threshold ==
                        (gas ? UI_THRESHOLD_GAS : UI_THRESHOLD_TEMPERATURE);
        uint8_t y = gas ? 43U : 87U;
        add(4U, y, 120U, 39U, NULL, selected ? UI_SELECTED_PANEL : UI_PANEL);
        if (selected) {
            add(4U, y, 4U, 39U, NULL, UI_CYAN);
            add(8U, y, 116U, 1U, NULL, UI_CYAN);
            add(8U, y + 38U, 116U, 1U, NULL, UI_CYAN);
            add(123U, y, 1U, 39U, NULL, UI_CYAN);
            add(99U, y + 2U, 22U, 35U, NULL, UI_LINE);
        }
        text(12U, y + 4U, gas ? "GAS LIMIT" : "TEMP MAX", &ui_font_label,
             selected ? UI_CYAN : UI_MUTED);
        value_label(gas ? target.gas_threshold : target.temperature_threshold,
                    gas || target.temperature_threshold_configured,
                    gas || !target.temperature_threshold_configured ? "" : "°C");
        text(12U, y + 17U, label, &ui_font_value, UI_INK);
        if (gas) {
            text(98U - text_width(&ui_font_small, "NOW"), y + 4U,
                 "NOW", &ui_font_small, UI_MUTED);
            set_label("");
            if (available(&target)) {
                number(target.gas_raw);
            } else {
                append("--");
            }
            text(98U - text_width(&ui_font_small, label), y + 26U,
                 label, &ui_font_small, UI_MUTED);
        }
        if (selected) {
            add(101U, y + 2U, 18U, 16U, ui_icon_up, UI_CYAN);
            add(101U, y + 21U, 18U, 16U, ui_icon_down, UI_CYAN);
        }
    }
    static const uint8_t *const keys[] = {
        ui_icon_next, ui_icon_add, ui_icon_remove, ui_icon_check
    };
    for (uint8_t i = 0U; i < 4U; i++) {
        add(8U + i * 30U, 137U, 22U, 19U, NULL, UI_PANEL);
        add(11U + i * 30U, 138U, 16U, 16U, keys[i], i == 3U ? UI_GREEN : UI_INK);
    }
}

static void bus_error_scene(void) {
    add(57U, 31U, 14U, 14U, ui_state_offline, UI_AMBER);
    centered(59U, "BUS ERROR", &ui_font_status, UI_AMBER);
    add(16U, 81U, 96U, 1U, NULL, UI_LINE);
    centered(95U, "CAN UNAVAILABLE", &ui_font_label, UI_INK);
    centered(113U, "CHECK BUS", &ui_font_label, UI_MUTED);
}

static void compose_tile(uint16_t tile) {
    uint8_t x = (tile % UI_TILE_COLUMNS) * UI_TILE_SIZE;
    uint8_t y = (tile / UI_TILE_COLUMNS) * UI_TILE_SIZE;
    for (uint8_t i = 0U; i < UI_TILE_SIZE * UI_TILE_SIZE; i++) {
        tile_pixels[i] = UI_BG;
    }
    for (uint8_t i = 0U; i < op_count; i++) {
        const UiOp_t *op = &ops[i];
        uint16_t left = op->x > x ? op->x : x;
        uint16_t top = op->y > y ? op->y : y;
        uint16_t right = op->x + op->width < x + UI_TILE_SIZE ?
                         op->x + op->width : x + UI_TILE_SIZE;
        uint16_t bottom = op->y + op->height < y + UI_TILE_SIZE ?
                          op->y + op->height : y + UI_TILE_SIZE;
        for (uint16_t row = top; row < bottom; row++) {
            for (uint16_t col = left; col < right; col++) {
                uint16_t index = (row - op->y) * op->width + col - op->x;
                uint8_t alpha = op->mask == NULL ? 15U :
                    (op->mask[index / 2U] >> ((index & 1U) == 0U ? 4U : 0U)) & 15U;
                uint8_t pixel = (row - y) * UI_TILE_SIZE + col - x;
                tile_pixels[pixel] = blend(op->color, tile_pixels[pixel], alpha);
            }
        }
    }
}

static void tile_row(uint8_t row, uint16_t *pixels) {
    for (uint8_t col = 0U; col < UI_TILE_SIZE; col++) {
        pixels[col] = tile_pixels[row * UI_TILE_SIZE + col];
    }
}

static uint32_t hash_value(uint32_t hash, uint32_t value) {
    return (hash ^ value) * 16777619U;
}

static void stream_scene(void) {
    uint32_t hash = 2166136261U;
    for (uint16_t i = 0U; i < UI_TILE_COUNT; i++) {
        next_tile_hash[i] = 2166136261U;
    }
    for (uint8_t i = 0U; i < op_count; i++) {
        const UiOp_t *op = &ops[i];
        uint32_t signature = hash_value(2166136261U, (uint32_t)(uintptr_t)op->mask);
        signature = hash_value(signature, op->color);
        signature = hash_value(signature, op->x | ((uint32_t)op->y << 8) |
                               ((uint32_t)op->width << 16) | ((uint32_t)op->height << 24));
        hash = hash_value(hash, signature);
        uint8_t last_x = (op->x + op->width - 1U) / UI_TILE_SIZE;
        uint8_t last_y = (op->y + op->height - 1U) / UI_TILE_SIZE;
        for (uint8_t row = op->y / UI_TILE_SIZE; row <= last_y; row++) {
            for (uint8_t col = op->x / UI_TILE_SIZE; col <= last_x; col++) {
                uint16_t tile = row * UI_TILE_COLUMNS + col;
                next_tile_hash[tile] = hash_value(next_tile_hash[tile], signature);
            }
        }
    }
    hash = hash_value(hash, reveal_height);
    if (hash != scene_hash) {
        scene_hash = hash;
        if (pending_tiles == 0U) {
            scan_tile = 0U;
        }
        pending_tiles = UI_TILE_COUNT;
    }
    uint8_t sent = 0U;
    while (pending_tiles != 0U && sent < UI_TILES_PER_FRAME) {
        uint16_t tile = scan_tile;
        scan_tile = (scan_tile + 1U) % UI_TILE_COUNT;
        pending_tiles--;
        if ((tile / UI_TILE_COLUMNS) * UI_TILE_SIZE >= reveal_height) {
            continue;
        }
        uint32_t pixels_hash = next_tile_hash[tile];
        uint8_t bit = (uint8_t)(1U << (tile % 8U));
        if ((tile_valid[tile / 8U] & bit) == 0U || tile_hash[tile] != pixels_hash) {
            compose_tile(tile);
            hal_tft_draw_region((tile % UI_TILE_COLUMNS) * UI_TILE_SIZE,
                                (tile / UI_TILE_COLUMNS) * UI_TILE_SIZE,
                                UI_TILE_SIZE, UI_TILE_SIZE, tile_row);
            if (!hal_tft_ok()) {
                return;
            }
            tile_hash[tile] = pixels_hash;
            tile_valid[tile / 8U] |= bit;
            sent++;
        }
    }
}

void app_ui_reset(void) {
    valid = false;
    clock_started = false;
    screen_started = false;
    reveal_height = UI_SCREEN_HEIGHT;
    filter_valid = false;
    filtered_gas = 0;
    display_gas = 0U;
    shown_gas = 0U;
    scene_hash = 0U;
    pending_tiles = 0U;
    for (uint8_t i = 0U; i < sizeof(tile_valid); i++) {
        tile_valid[i] = 0U;
    }
}

void app_ui_render(const UiView_t *view) {
    if (view != NULL) {
        bool live = available(view);
        bool sample = !valid || view->sample_id != target.sample_id ||
                      view->gas_raw != target.gas_raw;
        bool boundary = !valid || state(view) != state(&target) ||
                        gas_state(view) != gas_state(&target) ||
                        view->gas_threshold != target.gas_threshold;
        if (!live) {
            filter_valid = false;
        } else if (!filter_valid || boundary) {
            display_gas = gas_percent(view);
            filtered_gas = (int32_t)display_gas * UI_GAS_FIXED_SCALE;
            filter_valid = true;
        } else if (sample) {
            uint16_t raw = gas_percent(view);
            filtered_gas += ((int32_t)raw * (int32_t)UI_GAS_FIXED_SCALE - filtered_gas) /
                            (int32_t)UI_GAS_EMA_DIVISOR;
            uint16_t rounded = (filtered_gas + UI_GAS_FIXED_SCALE / 2U) / UI_GAS_FIXED_SCALE;
            int32_t delta = (int32_t)rounded - display_gas;
            if (delta >= (int32_t)UI_GAS_DEADBAND_PERCENT ||
                delta <= -(int32_t)UI_GAS_DEADBAND_PERCENT) {
                display_gas = rounded;
            }
        }
        target = *view;
        valid = true;
    }
}

void app_ui_tick(uint32_t now_ms) {
    if (!valid || !hal_tft_ok()) {
        return;
    }
    if (!clock_started) {
        start_ms = now_ms;
        clock_started = true;
    }
    uint32_t uptime = now_ms - start_ms;
    lamp_check = state(&target) != UI_BUS_ERROR && state(&target) != UI_WARNING &&
                 state(&target) != UI_DANGER && uptime < UI_SWEEP_MS + UI_LAYOUT_MS;
    if (lamp_check) {
        uint32_t sweep = uptime > UI_LAYOUT_MS ? uptime - UI_LAYOUT_MS : 0U;
        uint32_t phase = sweep < UI_SWEEP_MS / 2U ? sweep : UI_SWEEP_MS - sweep;
        shown_gas = (uint16_t)(phase * UI_GAS_GAUGE_FULL_SCALE_PERCENT /
                               (UI_SWEEP_MS / 2U));
    } else {
        shown_gas = available(&target) ? display_gas : 0U;
    }
    blink_on = (now_ms / UI_BLINK_PERIOD_MS) % 2U == 0U;
    fan_frame = target.has_data && target.fan_on ?
                (now_ms / UI_FAN_FRAME_MS) % UI_FAN_FRAME_COUNT : 0U;
    uint8_t next_mode = target.bus_error ? 3U : target.edit_mode ? 1U :
                        (target.stale && target.has_data ? 2U : 0U);
    if (!screen_started) {
        screen_started = true;
        screen_mode = next_mode;
        transition_ms = now_ms - UI_SCREEN_HEIGHT / UI_WIPE_BAND_PX * UI_WIPE_STEP_MS;
    } else if (next_mode != screen_mode) {
        screen_mode = next_mode;
        transition_ms = target.bus_error ?
                        now_ms - UI_SCREEN_HEIGHT / UI_WIPE_BAND_PX * UI_WIPE_STEP_MS : now_ms;
        scan_tile = 0U;
    }
    uint32_t bands = (now_ms - transition_ms) / UI_WIPE_STEP_MS + 1U;
    reveal_height = bands >= UI_SCREEN_HEIGHT / UI_WIPE_BAND_PX ? UI_SCREEN_HEIGHT :
                    (uint8_t)(bands * UI_WIPE_BAND_PX);
    begin();
    if (target.bus_error) {
        bus_error_scene();
    } else if (target.edit_mode) {
        settings_scene();
    } else {
        main_scene();
    }
    stream_scene();
}

void app_ui_button(UiView_t *view, UiButton_t button) {
    if (view == NULL) {
        return;
    }
    if (button == UI_BUTTON_SAVE) {
        view->edit_mode = false;
    } else if (button == UI_BUTTON_SELECT) {
        if (!view->edit_mode) {
            view->edit_mode = true;
            view->selected_threshold = UI_THRESHOLD_GAS;
        } else {
            view->selected_threshold = view->selected_threshold == UI_THRESHOLD_GAS ?
                                       UI_THRESHOLD_TEMPERATURE : UI_THRESHOLD_GAS;
        }
    } else if (view->edit_mode) {
        bool increase = button == UI_BUTTON_PLUS;
        if (view->selected_threshold == UI_THRESHOLD_GAS) {
            uint16_t gas = view->gas_threshold > UI_GAS_MAX ? UI_GAS_MAX : view->gas_threshold;
            view->gas_threshold = increase ?
                (gas < UI_GAS_MAX - UI_GAS_STEP_RAW ? gas + UI_GAS_STEP_RAW : UI_GAS_MAX) :
                (gas > UI_GAS_STEP_RAW ? gas - UI_GAS_STEP_RAW : 0U);
        } else if (!view->temperature_threshold_configured) {
            view->temperature_threshold = UI_TEMPERATURE_INITIAL_C;
            view->temperature_threshold_configured = true;
        } else if (increase && view->temperature_threshold < UI_TEMPERATURE_MAX_C) {
            view->temperature_threshold++;
        } else if (!increase && view->temperature_threshold != 0U) {
            view->temperature_threshold--;
        } else if (!increase) {
            view->temperature_threshold_configured = false;
        }
    }
    app_ui_render(view);
}

#ifdef UI_PREVIEW
uint16_t app_ui_preview_gas(void) {
    return display_gas;
}

uint16_t app_ui_preview_gauge(void) {
    return gauge_percent();
}

uint8_t app_ui_preview_arc_percent(uint8_t segment) {
    return arc_percent(segment);
}

uint16_t app_ui_preview_arc_color(uint8_t segment) {
    assert(segment < UI_ARC_COUNT);
    return ops[4U + segment].color;
}

uint8_t app_ui_preview_state(void) {
    return (uint8_t)state(&target);
}

void app_ui_preview_redraw(void) {
    for (uint8_t i = 0U; i < sizeof(tile_valid); i++) {
        tile_valid[i] = 0U;
    }
    scene_hash = 0U;
}
#endif
