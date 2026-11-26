#include "tuner_screen.h"

#include "ui/ui_palette.h"

#include <stddef.h>

bool gp100_tuner_screen_load_settings(
    gp100_tuner_model_t *model,
    const gp100_settings_record_t *settings)
{
    int16_t output_mode;
    int16_t reference_hz;

    if (model == NULL || settings == NULL ||
        !gp100_settings_get_i16(
            settings, GP100_SETTINGS_TUNER_MODE, &output_mode) ||
        !gp100_settings_get_i16(
            settings, GP100_SETTINGS_TUNER_REFERENCE_HZ,
            &reference_hz) ||
        output_mode < GP100_TUNER_OUTPUT_MUTE ||
        output_mode > GP100_TUNER_OUTPUT_BYPASS ||
        reference_hz < (int16_t)GP100_TUNER_REFERENCE_MIN_HZ ||
        reference_hz > (int16_t)GP100_TUNER_REFERENCE_MAX_HZ) {
        return false;
    }
    return gp100_tuner_set_output_mode(
               model, (gp100_tuner_output_mode_t)output_mode) &&
           gp100_tuner_set_reference(model, (uint16_t)reference_hz);
}

bool gp100_tuner_screen_adjust(
    gp100_tuner_model_t *model,
    gp100_settings_record_t *settings,
    gp100_tuner_screen_control_t control,
    bool increase)
{
    int value;
    gp100_settings_field_t field;
    bool changed;

    if (model == NULL || settings == NULL) {
        return false;
    }
    switch (control) {
    case GP100_TUNER_SCREEN_OUTPUT_MODE:
        value = model->output_mode;
        value += increase ? 1 : -1;
        if (value < GP100_TUNER_OUTPUT_MUTE) {
            value = GP100_TUNER_OUTPUT_MUTE;
        } else if (value > GP100_TUNER_OUTPUT_BYPASS) {
            value = GP100_TUNER_OUTPUT_BYPASS;
        }
        changed = gp100_tuner_set_output_mode(
            model, (gp100_tuner_output_mode_t)value);
        field = GP100_SETTINGS_TUNER_MODE;
        break;
    case GP100_TUNER_SCREEN_REFERENCE_HZ:
        value = model->reference_hz;
        value += increase ? 1 : -1;
        if (value < (int)GP100_TUNER_REFERENCE_MIN_HZ) {
            value = GP100_TUNER_REFERENCE_MIN_HZ;
        } else if (value > (int)GP100_TUNER_REFERENCE_MAX_HZ) {
            value = GP100_TUNER_REFERENCE_MAX_HZ;
        }
        changed = gp100_tuner_set_reference(model, (uint16_t)value);
        field = GP100_SETTINGS_TUNER_REFERENCE_HZ;
        break;
    default:
        return false;
    }
    if (!changed ||
        !gp100_settings_set_i16(settings, field, (int16_t)value)) {
        return false;
    }
    gp100_settings_update_crc(settings);
    return true;
}

gp100_tuner_screen_mode_result_t
gp100_tuner_screen_handle_mode_event(
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode)
{
    gp100_ui_control_event_t decoded;

    if (event == NULL || next_mode == NULL ||
        !gp100_decode_ui_control_event(event, &decoded)) {
        return GP100_TUNER_SCREEN_MODE_INVALID;
    }
    if ((decoded.kind == GP100_UI_CONTROL_FOOTSWITCH_PAIR &&
         decoded.action == GP100_BUTTON_RELEASE) ||
        (decoded.kind == GP100_UI_CONTROL_BUTTON &&
         (decoded.action == GP100_BUTTON_RELEASE ||
          (decoded.control == GP100_BUTTON_DRUM &&
           decoded.action == GP100_BUTTON_HOLD)))) {
        *next_mode = GP100_UI_MODE_PLAY;
        return GP100_TUNER_SCREEN_MODE_CHANGED;
    }
    return GP100_TUNER_SCREEN_MODE_UNHANDLED;
}

gp100_tuner_screen_event_result_t
gp100_tuner_screen_handle_event(
    gp100_tuner_model_t *model,
    gp100_settings_record_t *settings,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode)
{
    gp100_ui_control_event_t decoded;
    gp100_tuner_screen_mode_result_t mode;
    gp100_tuner_screen_control_t control;

    if (model == NULL || settings == NULL || event == NULL ||
        next_mode == NULL ||
        !gp100_decode_ui_control_event(event, &decoded)) {
        return GP100_TUNER_SCREEN_EVENT_INVALID;
    }
    if (decoded.kind == GP100_UI_CONTROL_ENCODER) {
        if (decoded.control == GP100_ENCODER_QUICK_1) {
            control = GP100_TUNER_SCREEN_OUTPUT_MODE;
        } else if (decoded.control == GP100_ENCODER_QUICK_3) {
            control = GP100_TUNER_SCREEN_REFERENCE_HZ;
        } else {
            return GP100_TUNER_SCREEN_EVENT_UNHANDLED;
        }
        return gp100_tuner_screen_adjust(
                   model, settings, control,
                   decoded.action == GP100_BUTTON_PRESS)
            ? GP100_TUNER_SCREEN_EVENT_SETTINGS_CHANGED
            : GP100_TUNER_SCREEN_EVENT_INVALID;
    }
    mode = gp100_tuner_screen_handle_mode_event(event, next_mode);
    if (mode == GP100_TUNER_SCREEN_MODE_CHANGED) {
        return GP100_TUNER_SCREEN_EVENT_MODE_CHANGED;
    }
    if (mode == GP100_TUNER_SCREEN_MODE_UNHANDLED) {
        return GP100_TUNER_SCREEN_EVENT_UNHANDLED;
    }
    return GP100_TUNER_SCREEN_EVENT_INVALID;
}

typedef struct tuner_point {
    int32_t x;
    int32_t y;
} tuner_point_t;

static const char *output_mode_name(gp100_tuner_output_mode_t mode)
{
    static const char *const names[] = {"MUTE", "THRU", "BYPASS"};
    uint32_t index = (uint32_t)mode;

    return index <= (uint32_t)GP100_TUNER_OUTPUT_BYPASS
               ? names[index]
               : NULL;
}

bool gp100_tuner_screen_default_theme(
    gp100_tuner_screen_theme_t *theme,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *large_font)
{
    if (theme == NULL || regular_font == NULL) {
        return false;
    }
    theme->regular_font = regular_font;
    theme->large_font = large_font != NULL ? large_font : regular_font;
    theme->background_rgb565 = GP100_UI_BACKGROUND;
    theme->foreground_rgb565 = GP100_UI_FOREGROUND;
    theme->status_background_rgb565 = GP100_UI_PANEL;
    theme->control_background_rgb565 = GP100_UI_CONTROL;
    theme->control_foreground_rgb565 = GP100_UI_ACCENT;
    theme->near_marker_rgb565 = GP100_UI_ACCENT;
    theme->far_marker_rgb565 = GP100_UI_DANGER;
    theme->in_tune_rgb565 = GP100_UI_SUCCESS;
    return true;
}

static void draw_quad(gp100_display_canvas_t *canvas,
                      const tuner_point_t points[4],
                      uint16_t color,
                      bool filled)
{
    if (filled) {
        (void)gp100_display_fill_triangle(
            canvas, points[0].x, points[0].y, points[1].x, points[1].y,
            points[2].x, points[2].y, color);
        (void)gp100_display_fill_triangle(
            canvas, points[0].x, points[0].y, points[2].x, points[2].y,
            points[3].x, points[3].y, color);
    } else {
        for (size_t index = 0u; index < 4u; ++index) {
            size_t next = (index + 1u) % 4u;
            (void)gp100_display_draw_line(
                canvas, points[index].x, points[index].y,
                points[next].x, points[next].y, color);
        }
    }
}

static void marker_points(size_t index,
                          bool right,
                          tuner_point_t points[4])
{
    int32_t inner_x = right ? 99 + (int32_t)index * 12
                            : 61 - (int32_t)index * 12;
    int32_t outer_x = right ? 109 + (int32_t)index * 22
                            : 51 - (int32_t)index * 22;
    int32_t top_inner = 42 + (int32_t)index * 4;
    int32_t top_outer = 45 + (int32_t)index * 7;

    points[0].x = inner_x;
    points[0].y = top_inner;
    points[1].x = outer_x;
    points[1].y = top_outer;
    points[2].x = outer_x;
    points[2].y = 76;
    points[3].x = inner_x;
    points[3].y = 76;
}

/* GP100_UI_ASSET_REPLACEMENT: tuning indicators. */
static void draw_markers(gp100_display_canvas_t *canvas,
                         const gp100_tuner_reading_t *reading,
                         const gp100_tuner_screen_theme_t *theme)
{
    int32_t magnitude = reading->needle_step < 0
                            ? -(int32_t)reading->needle_step
                            : (int32_t)reading->needle_step;

    for (size_t index = 0u; index < 5u; ++index) {
        tuner_point_t left[4];
        tuner_point_t right[4];
        uint16_t active = index < 3u ? theme->near_marker_rgb565
                                     : theme->far_marker_rgb565;
        bool left_active = reading->valid && reading->needle_step < 0 &&
                           (int32_t)index < magnitude;
        bool right_active = reading->valid && reading->needle_step > 0 &&
                            (int32_t)index < magnitude;
        bool centered = reading->valid && reading->needle_step == 0;

        marker_points(index, false, left);
        marker_points(index, true, right);
        draw_quad(canvas, left,
                  centered ? theme->in_tune_rgb565
                           : (left_active ? active
                                          : theme->foreground_rgb565),
                  centered || left_active);
        draw_quad(canvas, right,
                  centered ? theme->in_tune_rgb565
                           : (right_active ? active
                                           : theme->foreground_rgb565),
                  centered || right_active);
    }
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
        foreground,
        background,
        alignment,
        true,
    };

    return gp100_display_draw_text(canvas, font, x, y, text, &style);
}

static void format_reference(uint16_t reference_hz, char buffer[4])
{
    buffer[0] = (char)('0' + (reference_hz / 100u) % 10u);
    buffer[1] = (char)('0' + (reference_hz / 10u) % 10u);
    buffer[2] = (char)('0' + reference_hz % 10u);
    buffer[3] = '\0';
}

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_tuner_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_tuner_model_t *model,
    const gp100_tuner_screen_theme_t *theme)
{
    char reference[4];
    char note[2];
    const char *mode_name;

    if (canvas == NULL || model == NULL || theme == NULL ||
        theme->regular_font == NULL || theme->large_font == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT ||
        model->reference_hz < GP100_TUNER_REFERENCE_MIN_HZ ||
        model->reference_hz > GP100_TUNER_REFERENCE_MAX_HZ) {
        return false;
    }
    mode_name = output_mode_name(model->output_mode);
    if (mode_name == NULL) {
        return false;
    }

    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, theme->background_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 0, 92, GP100_UI_LOGICAL_WIDTH, 36u,
        theme->status_background_rgb565);
    if (!draw_text(canvas, theme->large_font, 80, 12, "TUNER",
                   GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                   theme->background_rgb565)) {
        return false;
    }

    draw_markers(canvas, &model->reading, theme);
    (void)gp100_display_draw_line(
        canvas, 80, 38, 80, 44, theme->foreground_rgb565);
    (void)gp100_display_draw_line(
        canvas, 80, 73, 80, 82, theme->foreground_rgb565);

    if (model->reading.valid) {
        note[0] = model->reading.note_letter;
        note[1] = '\0';
        if (!draw_text(canvas, theme->large_font, 80, 56, note,
                       GP100_TEXT_ALIGN_CENTER,
                       theme->foreground_rgb565,
                       theme->background_rgb565) ||
            (model->reading.sharp &&
             !draw_text(canvas, theme->regular_font, 89, 49, "#",
                        GP100_TEXT_ALIGN_LEFT,
                        theme->foreground_rgb565,
                        theme->background_rgb565))) {
            return false;
        }
    }

    (void)gp100_display_fill_rect(
        canvas, 3, 110, 50u, 14u, theme->control_background_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 110, 110, 50u, 14u, theme->control_background_rgb565);
    format_reference(model->reference_hz, reference);
    if (!draw_text(canvas, theme->regular_font, 28, 95, "OUTPUT",
                   GP100_TEXT_ALIGN_CENTER,
                   theme->foreground_rgb565,
                   theme->status_background_rgb565) ||
        !draw_text(canvas, theme->regular_font, 135, 95, "REFERENCE",
                   GP100_TEXT_ALIGN_CENTER,
                   theme->foreground_rgb565,
                   theme->status_background_rgb565) ||
        !draw_text(canvas, theme->regular_font, 28, 113, mode_name,
                   GP100_TEXT_ALIGN_CENTER,
                   theme->control_foreground_rgb565,
                   theme->control_background_rgb565) ||
        !draw_text(canvas, theme->regular_font, 136, 113, reference,
                   GP100_TEXT_ALIGN_RIGHT,
                   theme->control_foreground_rgb565,
                   theme->control_background_rgb565) ||
        !draw_text(canvas, theme->regular_font, 139, 113, "Hz",
                   GP100_TEXT_ALIGN_LEFT,
                   theme->control_foreground_rgb565,
                   theme->control_background_rgb565)) {
        return false;
    }
    return true;
}
