#include "looper_screen.h"

#include "ui/ui_palette.h"

#include "protocol/midi/midi_cc.h"
#include "protocol/midi/midi_message.h"
#include "storage/preset_record.h"

#include <stddef.h>

enum {
    kPanelMaxLevel = 99,
    kProgressX = 3,
    kProgressY = 35,
    kProgressWidth = 154,
    kProgressHeight = 5,
    kBottomY = 98,
};

static bool engine_is_valid(const gp100_looper_engine_t *engine)
{
    return engine != NULL && engine->sample_rate_hz != 0u &&
           engine->record_level <= GP100_LOOPER_MAX_LEVEL &&
           engine->playback_level <= GP100_LOOPER_MAX_LEVEL &&
           (engine->position == GP100_LOOPER_PRE_EFFECTS ||
            engine->position == GP100_LOOPER_POST_EFFECTS) &&
           (uint32_t)engine->state <=
               (uint32_t)GP100_LOOPER_STOPPED;
}

bool gp100_looper_screen_default_theme(
    gp100_looper_screen_theme_t *theme,
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
    theme->panel_rgb565 = GP100_UI_PANEL;
    theme->control_rgb565 = GP100_UI_CONTROL;
    theme->accent_rgb565 = GP100_UI_ACCENT;
    theme->record_rgb565 = GP100_UI_DANGER;
    theme->play_rgb565 = GP100_UI_PLAYBACK;
    theme->track_rgb565 = GP100_UI_TRACK;
    return true;
}

const char *gp100_looper_screen_status(
    const gp100_looper_screen_snapshot_t *snapshot)
{
    if (snapshot == NULL || !engine_is_valid(snapshot->engine)) {
        return NULL;
    }
    if (snapshot->clear_feedback) {
        return "Clean";
    }
    switch (snapshot->engine->state) {
    case GP100_LOOPER_RECORDING:
    case GP100_LOOPER_OVERDUBBING:
        return "Recording";
    case GP100_LOOPER_PLAYING:
        return "Playing";
    case GP100_LOOPER_STOPPED:
        return "Stop";
    case GP100_LOOPER_IDLE:
        return snapshot->engine->recorded_samples == 0u ? "Empty" : "Stop";
    default:
        return NULL;
    }
}

static uint8_t adjust_level(uint8_t value,
                            bool increase,
                            uint16_t detent_count)
{
    unsigned adjusted;

    if (increase) {
        adjusted = (unsigned)value + detent_count;
        return (uint8_t)(adjusted > kPanelMaxLevel
                             ? kPanelMaxLevel
                             : adjusted);
    }
    return detent_count > value ? 0u : (uint8_t)(value - detent_count);
}

static bool store_looper_setting(
    gp100_settings_record_t *settings,
    gp100_settings_field_t field,
    uint16_t value)
{
    if (!gp100_settings_set_i16(settings, field, (int16_t)value)) {
        return false;
    }
    gp100_settings_update_crc(settings);
    return true;
}

bool gp100_looper_screen_adjust(
    gp100_looper_engine_t *engine,
    gp100_settings_record_t *settings,
    uint16_t *logical_preset_index,
    gp100_looper_screen_control_t control,
    bool increase,
    uint16_t detent_count)
{
    uint8_t record_level;
    uint8_t playback_level;

    if (engine == NULL || settings == NULL ||
        logical_preset_index == NULL ||
        *logical_preset_index >= GP100_MIDI_PRESET_COUNT ||
        (unsigned)control >
            (unsigned)GP100_LOOPER_SCREEN_PRESET) {
        return false;
    }
    record_level = engine->record_level_target;
    playback_level = engine->playback_level_target;
    switch (control) {
    case GP100_LOOPER_SCREEN_RECORD_LEVEL:
        record_level = adjust_level(
            record_level, increase, detent_count);
        return gp100_looper_set_levels(
                   engine, record_level, playback_level) &&
               store_looper_setting(
                   settings, GP100_SETTINGS_LOOP_RECORD_VOLUME,
                   record_level);
    case GP100_LOOPER_SCREEN_POSITION: {
        gp100_looper_position_t position = increase
            ? GP100_LOOPER_PRE_EFFECTS
            : GP100_LOOPER_POST_EFFECTS;

        return gp100_looper_set_position(engine, position) &&
               store_looper_setting(
                   settings, GP100_SETTINGS_LOOPER_POSITION,
                   (uint16_t)position);
    }
    case GP100_LOOPER_SCREEN_PLAYBACK_LEVEL:
        playback_level = adjust_level(
            playback_level, increase, detent_count);
        return gp100_looper_set_levels(
                   engine, record_level, playback_level) &&
               store_looper_setting(
                   settings, GP100_SETTINGS_LOOP_PLAYBACK_VOLUME,
                   playback_level);
    case GP100_LOOPER_SCREEN_PRESET:
        *logical_preset_index = gp100_midi_step_preset(
            *logical_preset_index, increase ? 1 : -1);
        return true;
    default:
        return false;
    }
}

static bool stop_from_any_active_state(
    gp100_looper_engine_t *engine,
    bool *changed)
{
    bool first_changed = false;
    bool second_changed = false;

    if (engine->state == GP100_LOOPER_RECORDING ||
        engine->state == GP100_LOOPER_OVERDUBBING) {
        if (!gp100_looper_handle_event(
                engine, GP100_LOOPER_PLAY, &first_changed)) {
            return false;
        }
    }
    if (engine->state == GP100_LOOPER_PLAYING &&
        !gp100_looper_handle_event(
            engine, GP100_LOOPER_STOP, &second_changed)) {
        return false;
    }
    if (changed != NULL) {
        *changed = first_changed || second_changed;
    }
    return true;
}

bool gp100_looper_screen_handle_footswitch(
    gp100_looper_engine_t *engine,
    gp100_looper_footswitch_t footswitch,
    bool long_press,
    bool *changed,
    bool *clear_feedback)
{
    gp100_looper_event_t event;
    bool local_changed = false;

    if (engine == NULL ||
        (unsigned)footswitch >
            (unsigned)GP100_LOOPER_FOOTSWITCH_STOP_CLEAR) {
        return false;
    }
    if (clear_feedback != NULL) {
        *clear_feedback = false;
    }
    if (footswitch == GP100_LOOPER_FOOTSWITCH_STOP_CLEAR) {
        if (!long_press) {
            return stop_from_any_active_state(engine, changed);
        }
        if (!gp100_looper_handle_event(
                engine, GP100_LOOPER_CLEAR, &local_changed)) {
            return false;
        }
        if (clear_feedback != NULL) {
            *clear_feedback = local_changed;
        }
    } else {
        if (long_press) {
            return false;
        }
        event = (engine->state == GP100_LOOPER_IDLE ||
                 engine->state == GP100_LOOPER_PLAYING)
            ? GP100_LOOPER_RECORD_OR_OVERDUB
            : GP100_LOOPER_PLAY;
        if (!gp100_looper_handle_event(
                engine, event, &local_changed)) {
            return false;
        }
    }
    if (changed != NULL) {
        *changed = local_changed;
    }
    return true;
}

gp100_looper_event_result_t gp100_looper_screen_handle_event(
    gp100_looper_engine_t *engine,
    gp100_settings_record_t *settings,
    uint16_t *logical_preset_index,
    bool *drum_enabled,
    const gp100_event_t *event,
    bool *pair_release_guard,
    bool *clear_feedback,
    gp100_ui_mode_t *next_mode)
{
    gp100_ui_control_event_t decoded;
    bool changed;
    gp100_ui_mode_t target;

    if (engine == NULL || settings == NULL || logical_preset_index == NULL ||
        drum_enabled == NULL || event == NULL || pair_release_guard == NULL ||
        clear_feedback == NULL || next_mode == NULL ||
        !gp100_decode_ui_control_event(event, &decoded)) {
        return GP100_LOOPER_EVENT_INVALID;
    }
    *clear_feedback = false;
    if (decoded.kind == GP100_UI_CONTROL_ENCODER) {
        gp100_looper_screen_control_t control =
            (gp100_looper_screen_control_t)decoded.control;

        if (decoded.control > GP100_ENCODER_PARA ||
            !gp100_looper_screen_adjust(
                engine, settings, logical_preset_index, control,
                decoded.action == GP100_BUTTON_PRESS,
                decoded.amount)) {
            return GP100_LOOPER_EVENT_INVALID;
        }
        return control == GP100_LOOPER_SCREEN_PRESET
            ? GP100_LOOPER_EVENT_PRESET_SELECTED
            : GP100_LOOPER_EVENT_SETTINGS_CHANGED;
    }
    if (decoded.kind == GP100_UI_CONTROL_FOOTSWITCH_PAIR) {
        if (decoded.action != GP100_BUTTON_RELEASE) {
            return GP100_LOOPER_EVENT_UNHANDLED;
        }
        if (*pair_release_guard) {
            *pair_release_guard = false;
            return GP100_LOOPER_EVENT_PAIR_RELEASE_GUARDED;
        }
        *next_mode = GP100_UI_MODE_PLAY;
        return GP100_LOOPER_EVENT_MODE_CHANGED;
    }
    if (decoded.kind != GP100_UI_CONTROL_BUTTON) {
        return GP100_LOOPER_EVENT_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_FOOTSWITCH_MINUS &&
        decoded.action == GP100_BUTTON_PRESS) {
        if (!gp100_looper_screen_handle_footswitch(
                engine, GP100_LOOPER_FOOTSWITCH_RECORD_PLAY,
                false, &changed, clear_feedback)) {
            return GP100_LOOPER_EVENT_INVALID;
        }
        return GP100_LOOPER_EVENT_ENGINE_ACTION;
    }
    if (decoded.control == GP100_BUTTON_FOOTSWITCH_PLUS &&
        (decoded.action == GP100_BUTTON_PRESS ||
         decoded.action == GP100_BUTTON_HOLD)) {
        if (!gp100_looper_screen_handle_footswitch(
                engine, GP100_LOOPER_FOOTSWITCH_STOP_CLEAR,
                decoded.action == GP100_BUTTON_HOLD,
                &changed, clear_feedback)) {
            return GP100_LOOPER_EVENT_INVALID;
        }
        return GP100_LOOPER_EVENT_ENGINE_ACTION;
    }
    if (decoded.control == GP100_BUTTON_DRUM) {
        if (decoded.action == GP100_BUTTON_RELEASE) {
            *drum_enabled = !*drum_enabled;
            return GP100_LOOPER_EVENT_DRUM_TOGGLED;
        }
        if (decoded.action == GP100_BUTTON_HOLD) {
            *next_mode = GP100_UI_MODE_DRUM;
            return GP100_LOOPER_EVENT_MODE_CHANGED;
        }
        return GP100_LOOPER_EVENT_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_SAVE &&
        decoded.action == GP100_BUTTON_RELEASE) {
        target = GP100_UI_MODE_SAVE;
    } else if (decoded.control == GP100_BUTTON_GLOBAL) {
        target = GP100_UI_MODE_GLOBAL;
    } else if (decoded.control == GP100_BUTTON_EXIT) {
        target = GP100_UI_MODE_PLAY;
    } else if (decoded.control == GP100_BUTTON_EDIT) {
        target = GP100_UI_MODE_EDIT;
    } else {
        return GP100_LOOPER_EVENT_UNHANDLED;
    }
    *next_mode = target;
    return GP100_LOOPER_EVENT_MODE_CHANGED;
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

    return gp100_display_draw_text(
        canvas, font, x, y, text, &style);
}

static void append_text(char *buffer,
                        size_t capacity,
                        size_t *length,
                        const char *text)
{
    while (*text != '\0' && *length + 1u < capacity) {
        buffer[*length] = *text;
        ++*length;
        ++text;
    }
    buffer[*length] = '\0';
}

static void format_unsigned(char *buffer, size_t capacity, uint32_t value)
{
    char reversed[10];
    size_t count = 0u;
    size_t output = 0u;

    do {
        reversed[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u && count < sizeof(reversed));
    while (count != 0u && output + 1u < capacity) {
        buffer[output++] = reversed[--count];
    }
    buffer[output] = '\0';
}

static void format_time(char buffer[8], uint32_t current, uint32_t total)
{
    current = current > 99u ? 99u : current;
    total = total > 99u ? 99u : total;
    buffer[0] = (char)('0' + current / 10u);
    buffer[1] = (char)('0' + current % 10u);
    buffer[2] = '/';
    buffer[3] = (char)('0' + total / 10u);
    buffer[4] = (char)('0' + total % 10u);
    buffer[5] = ' ';
    buffer[6] = 'S';
    buffer[7] = '\0';
}

static size_t frame_count(const gp100_looper_engine_t *engine,
                          size_t samples)
{
    return engine->position == GP100_LOOPER_POST_EFFECTS
               ? samples / 2u
               : samples;
}

static uint32_t seconds_from_frames(const gp100_looper_engine_t *engine,
                                    size_t frames)
{
    return (uint32_t)(frames / engine->sample_rate_hz);
}

static uint16_t progress_width(const gp100_looper_engine_t *engine)
{
    size_t numerator;
    size_t denominator;

    if (engine->state == GP100_LOOPER_RECORDING) {
        numerator = frame_count(engine, engine->recorded_samples);
        denominator = frame_count(engine, engine->storage_capacity_samples);
    } else {
        numerator = frame_count(engine, engine->playback_position);
        denominator = frame_count(engine, engine->recorded_samples);
    }
    if (denominator == 0u) {
        return 0u;
    }
    if (numerator >= denominator) {
        return kProgressWidth;
    }
    return (uint16_t)((numerator * kProgressWidth) / denominator);
}

static bool format_preset(char *buffer,
                          size_t capacity,
                          uint16_t logical_index,
                          const char *name)
{
    size_t length = 0u;
    uint16_t number;

    if (logical_index >= GP100_TOTAL_LOGICAL_PRESETS ||
        capacity < 6u) {
        return false;
    }
    buffer[length++] = logical_index < GP100_PRESETS_PER_BANK ? 'P' : 'F';
    number = (uint16_t)(logical_index % GP100_PRESETS_PER_BANK + 1u);
    buffer[length++] = (char)('0' + number / 10u);
    buffer[length++] = (char)('0' + number % 10u);
    if (name != NULL && name[0] != '\0') {
        buffer[length++] = ' ';
        buffer[length] = '\0';
        append_text(buffer, capacity, &length, name);
    } else {
        buffer[length] = '\0';
    }
    return true;
}

static uint16_t status_color(const gp100_looper_engine_t *engine,
                             const gp100_looper_screen_theme_t *theme)
{
    if (engine->state == GP100_LOOPER_RECORDING ||
        engine->state == GP100_LOOPER_OVERDUBBING) {
        return theme->record_rgb565;
    }
    if (engine->state == GP100_LOOPER_PLAYING) {
        return theme->play_rgb565;
    }
    return theme->foreground_rgb565;
}

static bool draw_bottom(gp100_display_canvas_t *canvas,
                        const gp100_looper_engine_t *engine,
                        const gp100_looper_screen_theme_t *theme)
{
    char record_level[4];
    char playback_level[4];
    const char *mode = engine->position == GP100_LOOPER_PRE_EFFECTS
                           ? "PRE"
                           : "POST";

    format_unsigned(record_level, sizeof(record_level), engine->record_level);
    format_unsigned(playback_level, sizeof(playback_level),
                    engine->playback_level);
    (void)gp100_display_fill_rect(
        canvas, 0, kBottomY, 160u, 30u, theme->panel_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 3, 110, 45u, 5u, theme->track_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 3, 110,
        (uint16_t)((45u * engine->record_level) /
                   kPanelMaxLevel),
        5u, theme->accent_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 112, 110, 45u, 5u, theme->track_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 112, 110,
        (uint16_t)((45u * engine->playback_level) /
                   kPanelMaxLevel),
        5u, theme->accent_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 55, 109, 50u, 17u, theme->control_rgb565);

    return draw_text(canvas, theme->small_font, 25, 99, "Rec VOL",
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->panel_rgb565) &&
           draw_text(canvas, theme->small_font, 80, 99, "Mode",
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->panel_rgb565) &&
           draw_text(canvas, theme->small_font, 135, 99, "Play VOL",
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->panel_rgb565) &&
           draw_text(canvas, theme->regular_font, 25, 117, record_level,
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->panel_rgb565) &&
           draw_text(canvas, theme->regular_font, 80, 114, mode,
                     GP100_TEXT_ALIGN_CENTER,
                     theme->accent_rgb565, theme->control_rgb565) &&
           draw_text(canvas, theme->regular_font, 135, 117,
                     playback_level, GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->panel_rgb565);
}

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_looper_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_looper_screen_snapshot_t *snapshot,
    const gp100_looper_screen_theme_t *theme)
{
    char preset[32];
    char time[8];
    const char *status;
    uint16_t color;
    uint16_t filled;
    size_t recorded_frames;
    size_t current_frames;

    if (canvas == NULL || snapshot == NULL || theme == NULL ||
        !engine_is_valid(snapshot->engine) || theme->regular_font == NULL ||
        theme->small_font == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT ||
        !format_preset(preset, sizeof(preset),
                       snapshot->logical_preset_index,
                       snapshot->preset_name)) {
        return false;
    }
    status = gp100_looper_screen_status(snapshot);
    if (status == NULL) {
        return false;
    }
    color = snapshot->clear_feedback
                ? theme->foreground_rgb565
                : status_color(snapshot->engine, theme);
    recorded_frames = frame_count(snapshot->engine,
                                  snapshot->engine->recorded_samples);
    current_frames = snapshot->engine->state == GP100_LOOPER_RECORDING
                         ? recorded_frames
                         : frame_count(snapshot->engine,
                                       snapshot->engine->playback_position);
    format_time(time,
                seconds_from_frames(snapshot->engine, current_frames),
                seconds_from_frames(snapshot->engine, recorded_frames));

    (void)gp100_display_fill_rect(
        canvas, 0, 0, 160u, 128u, theme->background_rgb565);
    if (!draw_text(canvas, theme->regular_font, 4, 4, "LOOPER",
                   GP100_TEXT_ALIGN_LEFT,
                   theme->foreground_rgb565, theme->background_rgb565) ||
        !draw_text(canvas, theme->regular_font, 20, 20, status,
                   GP100_TEXT_ALIGN_LEFT, color,
                   theme->background_rgb565) ||
        !draw_text(canvas, theme->regular_font, 156, 20, time,
                   GP100_TEXT_ALIGN_RIGHT,
                   theme->foreground_rgb565, theme->background_rgb565)) {
        return false;
    }
    (void)gp100_display_fill_rect(canvas, 7, 22, 9u, 9u, color);
    (void)gp100_display_fill_rect(
        canvas, kProgressX, kProgressY, kProgressWidth,
        kProgressHeight, theme->track_rgb565);
    filled = progress_width(snapshot->engine);
    if (filled != 0u) {
        (void)gp100_display_fill_rect(
            canvas, kProgressX, kProgressY, filled,
            kProgressHeight, color);
    }

    (void)gp100_display_draw_line(
        canvas, 3, 43, 156, 43, theme->control_rgb565);
    (void)gp100_display_draw_line(
        canvas, 3, 68, 156, 68, theme->control_rgb565);
    if (!draw_text(canvas, theme->regular_font, 80, 50, preset,
                   GP100_TEXT_ALIGN_CENTER,
                   theme->foreground_rgb565, theme->background_rgb565)) {
        return false;
    }

    (void)gp100_display_fill_rect(
        canvas, 3, 72, 154u, 22u, theme->control_rgb565);
    (void)gp100_display_draw_line(
        canvas, 80, 74, 80, 92, theme->foreground_rgb565);
    if (!draw_text(canvas, theme->small_font, 22, 79, "REC",
                   GP100_TEXT_ALIGN_CENTER,
                   theme->record_rgb565, theme->control_rgb565) ||
        !draw_text(canvas, theme->small_font, 58, 79, "PLAY",
                   GP100_TEXT_ALIGN_CENTER,
                   theme->foreground_rgb565, theme->control_rgb565) ||
        !draw_text(canvas, theme->small_font, 103, 79, "STOP",
                   GP100_TEXT_ALIGN_CENTER,
                   theme->foreground_rgb565, theme->control_rgb565) ||
        !draw_text(canvas, theme->small_font, 137, 79, "CLEAR",
                   GP100_TEXT_ALIGN_CENTER,
                   theme->record_rgb565, theme->control_rgb565) ||
        !draw_bottom(canvas, snapshot->engine, theme)) {
        return false;
    }
    return true;
}
