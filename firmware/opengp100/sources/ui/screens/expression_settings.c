#include "expression_settings.h"

#include "storage/expression_calibration_record.h"
#include "ui/ui_palette.h"

#include <string.h>

static const char *const menu_names[GP100_EXPRESSION_MENU_ITEMS] = {
    "VOL Range", "Calibrate"
};

static bool copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length;

    if (destination == NULL || capacity == 0u || source == NULL) {
        return false;
    }
    length = strlen(source);
    if (length >= capacity) {
        length = capacity - 1u;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
    return true;
}

static void format_unsigned(char *output, size_t capacity, unsigned value)
{
    char reversed[11];
    size_t count = 0u;
    size_t index = 0u;

    if (capacity == 0u) {
        return;
    }
    do {
        reversed[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u && count < sizeof(reversed));
    while (count != 0u && index + 1u < capacity) {
        output[index++] = reversed[--count];
    }
    output[index] = '\0';
}

static bool set_control(gp100_expression_control_t *control,
                        const char *label,
                        const char *value)
{
    if (!copy_text(control->label, sizeof(control->label), label) ||
        !copy_text(control->value, sizeof(control->value), value)) {
        return false;
    }
    control->present = true;
    return true;
}

const char *gp100_expression_menu_item_name(
    gp100_expression_menu_item_t item)
{
    if ((unsigned)item >= GP100_EXPRESSION_MENU_ITEMS) {
        return NULL;
    }
    return menu_names[(unsigned)item];
}

static bool fill_volume_range(gp100_expression_settings_snapshot_t *snapshot,
                              const gp100_preset_record_t *preset)
{
    float minimum;
    float maximum;
    char minimum_text[8];
    char maximum_text[8];
    gp100_volume_pedal_position_t position;

    if (!gp100_preset_control_range(preset, GP100_CONTROL_RANGE_VOLUME,
                                   &minimum, &maximum) ||
        !gp100_preset_volume_pedal_position(preset, &position)) {
        return false;
    }
    format_unsigned(minimum_text, sizeof(minimum_text), (unsigned)(minimum + 0.5f));
    format_unsigned(maximum_text, sizeof(maximum_text), (unsigned)(maximum + 0.5f));
    return set_control(&snapshot->controls[0], "Min", minimum_text) &&
           set_control(&snapshot->controls[1], "Position",
                       position == GP100_VOLUME_PEDAL_PRE ? "Pre" : "Post") &&
           set_control(&snapshot->controls[2], "Max", maximum_text);
}

bool gp100_expression_settings_snapshot_from_preset(
    gp100_expression_settings_snapshot_t *snapshot,
    const gp100_preset_record_t *preset,
    gp100_expression_menu_item_t selected_item)
{
    if (snapshot == NULL || preset == NULL ||
        (unsigned)selected_item >= GP100_EXPRESSION_MENU_ITEMS) {
        return false;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->selected_item = selected_item;
    switch (selected_item) {
    case GP100_EXPRESSION_VOLUME_RANGE:
        return fill_volume_range(snapshot, preset);
    case GP100_EXPRESSION_CALIBRATE:
        return set_control(&snapshot->controls[1], "", "Enter Setting");
    default:
        return false;
    }
}

bool gp100_expression_adjust_volume_range(gp100_preset_record_t *preset,
                                          bool maximum, int step)
{
    float minimum;
    float maximum_value;
    float *selected;

    if (!gp100_preset_control_range(
            preset, GP100_CONTROL_RANGE_VOLUME, &minimum, &maximum_value)) {
        return false;
    }
    selected = maximum ? &maximum_value : &minimum;
    *selected += (float)step;
    if (*selected < 0.0f) {
        *selected = 0.0f;
    } else if (*selected > 99.0f) {
        *selected = 99.0f;
    }
    return gp100_preset_set_control_range(
        preset, GP100_CONTROL_RANGE_VOLUME, minimum, maximum_value);
}

void gp100_expression_calibration_init(
    gp100_expression_calibration_t *calibration)
{
    if (calibration != NULL) {
        memset(calibration, 0, sizeof(*calibration));
        calibration->step = GP100_EXPRESSION_CALIBRATE_HEEL;
    }
}

bool gp100_expression_calibration_advance(
    gp100_expression_calibration_t *calibration,
    uint16_t raw_value,
    bool *committed)
{
    if (calibration == NULL || committed == NULL ||
        (unsigned)calibration->step >
            GP100_EXPRESSION_CALIBRATE_SAVE_FAILED) {
        return false;
    }
    *committed = false;
    switch (calibration->step) {
    case GP100_EXPRESSION_CALIBRATE_HEEL:
        calibration->heel = raw_value;
        calibration->step = GP100_EXPRESSION_CALIBRATE_TOE;
        break;
    case GP100_EXPRESSION_CALIBRATE_TOE:
        calibration->toe = raw_value;
        calibration->step = GP100_EXPRESSION_CALIBRATE_SWITCH;
        break;
    case GP100_EXPRESSION_CALIBRATE_SWITCH: {
        calibration->trigger = raw_value;
        const gp100_expression_calibration_values_t values = {
            calibration->heel, calibration->toe, calibration->trigger
        };
        if (!gp100_expression_calibration_values_are_usable(&values)) {
            calibration->step = GP100_EXPRESSION_CALIBRATE_FAILED;
            break;
        }
        *committed = true;
        break;
    }
    case GP100_EXPRESSION_CALIBRATE_SAVE_FAILED:
        *committed = true;
        break;
    case GP100_EXPRESSION_CALIBRATE_COMPLETE:
        break;
    case GP100_EXPRESSION_CALIBRATE_FAILED:
        gp100_expression_calibration_init(calibration);
        break;
    default:
        return false;
    }
    return true;
}

bool gp100_expression_settings_default_theme(
    gp100_expression_settings_theme_t *theme,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font)
{
    if (theme == NULL || regular_font == NULL) {
        return false;
    }
    theme->regular_font = regular_font;
    theme->small_font = small_font != NULL ? small_font : regular_font;
    theme->background_rgb565 = GP100_UI_BACKGROUND;
    theme->foreground_rgb565 = GP100_UI_FOREGROUND;
    theme->selected_rgb565 = GP100_UI_ACCENT;
    theme->panel_rgb565 = GP100_UI_PANEL;
    theme->value_rgb565 = GP100_UI_CONTROL;
    theme->track_rgb565 = GP100_UI_TRACK;
    return true;
}

static bool draw_text(gp100_display_canvas_t *canvas,
                      const gp100_bitmap_font_t *font,
                      int32_t x,
                      int32_t y,
                      const char *text,
                      gp100_text_alignment_t alignment,
                      uint16_t foreground,
                      uint16_t background)
{
    gp100_text_style_t style = {
        foreground, background, alignment, true
    };
    return gp100_display_draw_text(canvas, font, x, y, text, &style);
}

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_expression_settings_render(
    gp100_display_canvas_t *canvas,
    const gp100_expression_settings_snapshot_t *snapshot,
    const gp100_expression_settings_theme_t *theme)
{
    static const int32_t centers[3] = {27, 80, 133};
    size_t row;
    size_t index;

    if (canvas == NULL || snapshot == NULL || theme == NULL ||
        theme->regular_font == NULL || theme->small_font == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT ||
        (unsigned)snapshot->selected_item >= GP100_EXPRESSION_MENU_ITEMS) {
        return false;
    }
    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, theme->background_rgb565);
    if (!draw_text(canvas, theme->regular_font, 5, 7, "< EXP Setting",
                   GP100_TEXT_ALIGN_LEFT, theme->foreground_rgb565,
                   theme->background_rgb565)) {
        return false;
    }
    for (row = 0u; row < GP100_EXPRESSION_MENU_ITEMS; ++row) {
        index = row;
        if (index == (size_t)snapshot->selected_item) {
            (void)gp100_display_fill_rect(
                canvas, 2, (int32_t)(25u + row * 18u), 154u, 17u,
                theme->selected_rgb565);
        }
        if (!draw_text(canvas, theme->regular_font, 6,
                       (int32_t)(27u + row * 18u), menu_names[index],
                       GP100_TEXT_ALIGN_LEFT,
                       index == (size_t)snapshot->selected_item
                           ? theme->background_rgb565
                           : theme->foreground_rgb565,
                       index == (size_t)snapshot->selected_item
                           ? theme->selected_rgb565
                           : theme->background_rgb565)) {
            return false;
        }
    }
    (void)gp100_display_fill_rect(
        canvas, 2, 81, 156u, 47u, theme->panel_rgb565);
    for (index = 0u; index < 3u; ++index) {
        const gp100_expression_control_t *control =
            &snapshot->controls[index];
        if (!control->present) {
            continue;
        }
        (void)gp100_display_fill_rect(
            canvas, centers[index] - 22, 105, 44u, 19u,
            theme->value_rgb565);
        if (!draw_text(canvas, theme->small_font, centers[index], 87,
                       control->label, GP100_TEXT_ALIGN_CENTER,
                       theme->foreground_rgb565, theme->panel_rgb565) ||
            !draw_text(canvas, theme->small_font, centers[index], 108,
                       control->value, GP100_TEXT_ALIGN_CENTER,
                       theme->selected_rgb565, theme->value_rgb565)) {
            return false;
        }
    }
    return true;
}

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_expression_calibration_render(
    gp100_display_canvas_t *canvas,
    const gp100_expression_calibration_t *calibration,
    const gp100_expression_settings_theme_t *theme)
{
    static const char *const instruction[] = {
        "Lift pedal fully up", "Press pedal fully down", "Press strongly",
        "Calibration saved", "Calibration failed", "Save failed"
    };
    static const char *const action[] = {
        "Next", "Next", "Save", "OK", "Retry", "Retry"
    };

    if (canvas == NULL || calibration == NULL || theme == NULL ||
        theme->regular_font == NULL || theme->small_font == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT ||
        (unsigned)calibration->step >
            GP100_EXPRESSION_CALIBRATE_SAVE_FAILED) {
        return false;
    }
    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, theme->background_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 2, 83, 156u, 43u, theme->panel_rgb565);
    return draw_text(canvas, theme->regular_font, 5, 7, "< Calibrate",
                     GP100_TEXT_ALIGN_LEFT, theme->foreground_rgb565,
                     theme->background_rgb565) &&
           draw_text(canvas, theme->regular_font, 80, 49,
                     instruction[(unsigned)calibration->step],
                     GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                     theme->background_rgb565) &&
           draw_text(canvas, theme->regular_font, 80, 98,
                     action[(unsigned)calibration->step],
                     GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                     theme->panel_rgb565);
}
