#include "play_screen.h"

#include "audio/effects/effect_catalog.h"
#include "ui/render/effect_art.h"
#include "ui/ui_palette.h"
#include "storage/patch_chain.h"

#include <stddef.h>
#include <string.h>

enum {
    kTopHeight = 56,
    kPresetNumberY = 12,
    kExpressionIconX = 5,
    kExpressionIconY = 6,
    kMiddleX = 2,
    kMiddleY = 58,
    kMiddleWidth = 156,
    kMiddleHeight = 36,
    kBottomY = 95,
    kBottomHeight = 33,
    kControlY = 112,
    kControlWidth = 46,
    kControlHeight = 4
};

static const char *const family_labels[GP100_EFFECT_FAMILY_COUNT] = {
    "FX", "DST", "AMP", "NR", "CAB", "EQ", "MOD", "DLY", "REV"
};

static bool name_is_terminated(const char name[GP100_PRESET_NAME_SIZE + 1u])
{
    size_t index;

    for (index = 0u; index <= GP100_PRESET_NAME_SIZE; ++index) {
        if (name[index] == '\0') {
            return true;
        }
    }
    return false;
}

static bool snapshot_is_valid(
    const gp100_play_screen_snapshot_t *snapshot)
{
    size_t switch_index;
    size_t assignment;

    if (snapshot == NULL ||
        snapshot->logical_preset >= GP100_TOTAL_LOGICAL_PRESETS ||
        !name_is_terminated(snapshot->preset_name) ||
        (snapshot->footswitch_mode != GP100_FOOTSWITCH_MODE_PATCH &&
         snapshot->footswitch_mode != GP100_FOOTSWITCH_MODE_STOMP) ||
        (snapshot->expression_active &&
         (unsigned)snapshot->expression_pedal.family >= GP100_EFFECT_FAMILY_COUNT &&
         snapshot->expression_pedal.algorithm != GP100_PATCH_UNKNOWN_ALGORITHM) ||
        snapshot->master_volume > 100u || snapshot->tempo_bpm < 40u ||
        snapshot->tempo_bpm > 250u || snapshot->preset_volume > 100u) {
        return false;
    }
    if (snapshot->native) {
        for (size_t i = 0; i < GP100_PLAY_STOMP_SWITCH_COUNT; ++i) {
            if (snapshot->stomp_count[i] > GP100_PATCH_MAX_BLOCKS) {
                return false;
            }
            for (size_t j = 0; j < snapshot->stomp_count[i]; ++j) {
                const gp100_play_effect_t *pedal = &snapshot->stomp_effects[i][j];
                if ((unsigned)pedal->family >= GP100_EFFECT_FAMILY_COUNT &&
                    pedal->algorithm != GP100_PATCH_UNKNOWN_ALGORITHM) {
                    return false;
                }
            }
        }
        return true;
    }
    for (switch_index = 0u;
         switch_index < GP100_PLAY_STOMP_SWITCH_COUNT;
         ++switch_index) {
        for (assignment = 0u;
             assignment < GP100_PLAY_STOMP_ASSIGNMENTS_PER_SWITCH;
             ++assignment) {
            uint16_t family =
                snapshot->stomp_family[switch_index][assignment];
            if (family != GP100_PLAY_UNASSIGNED_FAMILY &&
                family >= GP100_EFFECT_FAMILY_COUNT) {
                return false;
            }
        }
    }
    return true;
}

bool gp100_play_screen_snapshot_from_records(
    gp100_play_screen_snapshot_t *snapshot,
    uint16_t logical_preset,
    const gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings,
    bool preset_modified,
    bool drum_enabled)
{
    gp100_play_screen_snapshot_t next;
    gp100_expression_pedal_target_t pedal_target;
    int16_t setting;
    size_t index;
    size_t assignment;

    if (snapshot == NULL || preset == NULL || settings == NULL ||
        logical_preset >= GP100_TOTAL_LOGICAL_PRESETS) {
        return false;
    }
    memset(&next, 0, sizeof(next));
    next.logical_preset = logical_preset;
    gp100_preset_name(preset, next.preset_name);
    next.tempo_bpm = gp100_preset_tempo_bpm(preset);
    next.preset_volume = gp100_preset_level(preset);
    if (!gp100_settings_get_i16(
            settings, GP100_SETTINGS_FOOTSWITCH_MODE, &setting) ||
        (setting != GP100_FOOTSWITCH_MODE_PATCH &&
         setting != GP100_FOOTSWITCH_MODE_STOMP)) {
        return false;
    }
    next.footswitch_mode = (gp100_footswitch_mode_t)setting;
    if (!gp100_settings_get_i16(
            settings, GP100_SETTINGS_MASTER_VOLUME, &setting) ||
        setting < 0 || setting > 100) {
        return false;
    }
    next.master_volume = (uint16_t)setting;
    next.native = gp100_patch_is_native(preset);
    if (next.native) {
        for (size_t i = 0u; i < gp100_patch_count(preset); ++i) {
            gp100_patch_block_t block;
            if (!gp100_patch_block_at(preset, i, &block)) {
                return false;
            }
            const bool available = gp100_effect_available(block.family, block.algorithm);
            next.unavailable_effects |= !available;
            for (size_t sw = 0; sw < GP100_PLAY_STOMP_SWITCH_COUNT; ++sw) {
                if ((block.footswitches & (1u << sw)) != 0u) {
                    const uint8_t pos = next.stomp_count[sw]++;
                    next.stomp_effects[sw][pos] = (gp100_play_effect_t){
                        .family = block.family, .algorithm = block.algorithm,
                        .enabled = available && block.enabled
                    };
                }
            }
        }
    } else {
        for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
            if (!gp100_preset_effect_enabled(
                    preset, (gp100_effect_family_t)index,
                    &next.effect_enabled[index])) {
                return false;
            }
        }
        for (index = 0u; index < GP100_PLAY_STOMP_SWITCH_COUNT; ++index) {
            for (assignment = 0u;
                 assignment < GP100_PLAY_STOMP_ASSIGNMENTS_PER_SWITCH;
                 ++assignment) {
                if (!gp100_preset_control_assignment(
                        preset, index, assignment,
                        &next.stomp_family[index][assignment])) {
                    return false;
                }
            }
        }
    }
    if (!gp100_preset_expression_pedal_target(
            preset, &pedal_target)) {
        return false;
    }
    gp100_patch_block_t linked_pedal;
    if (pedal_target == GP100_EXPRESSION_PEDAL_EFFECT &&
        gp100_patch_expression_get(preset, NULL, &linked_pedal)) {
        next.expression_active = true;
        next.expression_pedal = (gp100_play_effect_t){
            .family = linked_pedal.family,
            .algorithm = linked_pedal.algorithm,
            .enabled = linked_pedal.enabled &&
                       gp100_effect_available(linked_pedal.family, linked_pedal.algorithm)
        };
    }
    next.preset_modified = preset_modified;
    next.drum_enabled = drum_enabled;
    if (!snapshot_is_valid(&next)) {
        return false;
    }
    *snapshot = next;
    return true;
}

static uint16_t stepped_value(uint16_t value,
                              uint16_t minimum,
                              uint16_t maximum,
                              bool increase,
                              uint8_t amount)
{
    unsigned adjusted;

    if (increase) {
        adjusted = (unsigned)value + amount;
        return (uint16_t)(adjusted > maximum ? maximum : adjusted);
    }
    return amount > value - minimum
        ? minimum
        : (uint16_t)(value - amount);
}

static uint16_t adjacent_preset(const gp100_preset_record_t *preset,
                                uint16_t logical_preset, bool increase)
{
    const uint16_t count = gp100_patch_is_native(preset)
        ? GP100_PRESETS_PER_BANK : GP100_TOTAL_LOGICAL_PRESETS;
    if (increase) {
        return logical_preset + 1u >= count ? 0u : (uint16_t)(logical_preset + 1u);
    }
    return logical_preset == 0u ? count - 1u : (uint16_t)(logical_preset - 1u);
}

gp100_play_action_result_t gp100_play_adjust(
    gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    uint16_t *logical_preset,
    gp100_play_control_t control,
    bool increase,
    uint8_t detent_count)
{
    uint16_t previous;
    uint16_t adjusted;
    int16_t setting;

    if (settings == NULL || preset == NULL || logical_preset == NULL ||
        *logical_preset >= GP100_TOTAL_LOGICAL_PRESETS ||
        (unsigned)control > (unsigned)GP100_PLAY_CONTROL_PRESET) {
        return GP100_PLAY_ACTION_INVALID;
    }
    switch (control) {
    case GP100_PLAY_CONTROL_MASTER_VOLUME:
        if (!gp100_settings_get_i16(
                settings, GP100_SETTINGS_MASTER_VOLUME, &setting) ||
            setting < 0 || setting > 99) {
            return GP100_PLAY_ACTION_INVALID;
        }
        previous = (uint16_t)setting;
        adjusted = stepped_value(
            previous, 0u, 99u, increase, detent_count);
        if (adjusted == previous) {
            return GP100_PLAY_ACTION_UNCHANGED;
        }
        if (!gp100_settings_set_i16(
                settings, GP100_SETTINGS_MASTER_VOLUME,
                (int16_t)adjusted)) {
            return GP100_PLAY_ACTION_INVALID;
        }
        gp100_settings_update_crc(settings);
        return GP100_PLAY_ACTION_SETTINGS_CHANGED;
    case GP100_PLAY_CONTROL_TEMPO:
        previous = gp100_preset_tempo_bpm(preset);
        if (previous < 40u || previous > 250u) {
            return GP100_PLAY_ACTION_INVALID;
        }
        adjusted = stepped_value(
            previous, 40u, 250u, increase, detent_count);
        if (adjusted == previous) {
            return GP100_PLAY_ACTION_UNCHANGED;
        }
        if (!gp100_preset_set_tempo_bpm(preset, adjusted)) {
            return GP100_PLAY_ACTION_INVALID;
        }
        gp100_preset_update_crc(preset);
        return GP100_PLAY_ACTION_PRESET_CHANGED;
    case GP100_PLAY_CONTROL_PRESET_VOLUME:
        previous = gp100_preset_level(preset);
        if (previous > 99u) {
            return GP100_PLAY_ACTION_INVALID;
        }
        adjusted = stepped_value(
            previous, 0u, 99u, increase, detent_count);
        if (adjusted == previous) {
            return GP100_PLAY_ACTION_UNCHANGED;
        }
        if (!gp100_preset_set_level(preset, adjusted)) {
            return GP100_PLAY_ACTION_INVALID;
        }
        gp100_preset_update_crc(preset);
        return GP100_PLAY_ACTION_PRESET_CHANGED;
    case GP100_PLAY_CONTROL_PRESET:
        *logical_preset = adjacent_preset(preset, *logical_preset, increase);
        return GP100_PLAY_ACTION_SELECT_PRESET;
    default:
        return GP100_PLAY_ACTION_INVALID;
    }
}

gp100_play_action_result_t gp100_play_footswitch(
    const gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    uint16_t *logical_preset,
    size_t footswitch)
{
    int16_t mode;
    size_t assignment;
    bool toggled = false;

    if (settings == NULL || preset == NULL || logical_preset == NULL ||
        *logical_preset >= GP100_TOTAL_LOGICAL_PRESETS ||
        footswitch >= GP100_PLAY_STOMP_SWITCH_COUNT ||
        !gp100_settings_get_i16(
            settings, GP100_SETTINGS_FOOTSWITCH_MODE, &mode)) {
        return GP100_PLAY_ACTION_INVALID;
    }
    if (mode == GP100_FOOTSWITCH_MODE_PATCH) {
        *logical_preset = adjacent_preset(
            preset, *logical_preset, footswitch != 0u);
        return GP100_PLAY_ACTION_SELECT_PRESET;
    }
    if (mode != GP100_FOOTSWITCH_MODE_STOMP) {
        return GP100_PLAY_ACTION_INVALID;
    }
    if (gp100_patch_is_native(preset)) {
        return gp100_patch_toggle_footswitch(preset, footswitch)
            ? GP100_PLAY_ACTION_PRESET_CHANGED : GP100_PLAY_ACTION_UNCHANGED;
    }
    for (assignment = 0u;
         assignment < GP100_PLAY_STOMP_ASSIGNMENTS_PER_SWITCH;
         ++assignment) {
        uint16_t family;
        bool enabled;

        if (!gp100_preset_control_assignment(
                preset, footswitch, assignment, &family)) {
            return GP100_PLAY_ACTION_INVALID;
        }
        if (family == GP100_PLAY_UNASSIGNED_FAMILY) {
            continue;
        }
        if (!gp100_preset_effect_enabled(
                preset, (gp100_effect_family_t)family, &enabled) ||
            !gp100_preset_set_effect_enabled(
                preset, (gp100_effect_family_t)family, !enabled)) {
            return GP100_PLAY_ACTION_INVALID;
        }
        toggled = true;
    }
    if (!toggled) {
        return GP100_PLAY_ACTION_UNCHANGED;
    }
    gp100_preset_update_crc(preset);
    return GP100_PLAY_ACTION_PRESET_CHANGED;
}

gp100_play_action_result_t gp100_play_footswitch_hold(
    gp100_settings_record_t *settings,
    bool *tap_tempo_enabled,
    uint32_t *tap_count,
    uint32_t *last_tap_tick,
    size_t footswitch,
    uint32_t event_tick)
{
    int16_t mode;

    if (settings == NULL || tap_tempo_enabled == NULL || tap_count == NULL ||
        last_tap_tick == NULL ||
        footswitch >= GP100_PLAY_STOMP_SWITCH_COUNT) {
        return GP100_PLAY_ACTION_INVALID;
    }
    if (footswitch == 0u) {
        *tap_tempo_enabled = !*tap_tempo_enabled;
        *tap_count = 0u;
        *last_tap_tick = event_tick;
        return GP100_PLAY_ACTION_TAP_TEMPO_TOGGLED;
    }
    if (!gp100_settings_get_i16(
            settings, GP100_SETTINGS_FOOTSWITCH_MODE, &mode) ||
        (mode != GP100_FOOTSWITCH_MODE_PATCH &&
         mode != GP100_FOOTSWITCH_MODE_STOMP) ||
        !gp100_settings_set_i16(
            settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            mode == GP100_FOOTSWITCH_MODE_PATCH
                ? GP100_FOOTSWITCH_MODE_STOMP
                : GP100_FOOTSWITCH_MODE_PATCH)) {
        return GP100_PLAY_ACTION_INVALID;
    }
    gp100_settings_update_crc(settings);
    return GP100_PLAY_ACTION_SETTINGS_CHANGED;
}

gp100_play_action_result_t gp100_play_tap_tempo(
    gp100_preset_record_t *preset,
    uint32_t *tap_count,
    uint32_t *last_tap_tick,
    uint32_t event_tick)
{
    uint32_t interval_ms;
    uint32_t bpm;

    if (preset == NULL || tap_count == NULL || last_tap_tick == NULL) {
        return GP100_PLAY_ACTION_INVALID;
    }
    if (*tap_count == 0u) {
        interval_ms = 500u;
        *tap_count = 1u;
    } else {
        interval_ms = event_tick - *last_tap_tick;
        if (interval_ms > 3000u) {
            *last_tap_tick = event_tick;
            return GP100_PLAY_ACTION_TAP_TEMPO_ARMED;
        }
        if (interval_ms < 240u) {
            interval_ms = 240u;
        } else if (interval_ms > 1500u) {
            interval_ms = 1500u;
        }
    }
    *last_tap_tick = event_tick;
    bpm = (60000u + interval_ms / 2u) / interval_ms;
    if (!gp100_preset_set_tempo_bpm(preset, (uint16_t)bpm)) {
        return GP100_PLAY_ACTION_INVALID;
    }
    gp100_preset_update_crc(preset);
    return GP100_PLAY_ACTION_TAP_TEMPO_APPLIED;
}

gp100_play_mode_result_t gp100_play_handle_mode_event(
    const gp100_event_t *event,
    bool *drum_enabled,
    gp100_ui_mode_t *next_mode)
{
    gp100_ui_control_event_t decoded;
    gp100_ui_mode_t target;

    if (event == NULL || drum_enabled == NULL || next_mode == NULL ||
        !gp100_decode_ui_control_event(event, &decoded)) {
        return GP100_PLAY_MODE_INVALID;
    }
    if (decoded.kind == GP100_UI_CONTROL_FOOTSWITCH_PAIR) {
        if (decoded.action == GP100_BUTTON_RELEASE) {
            *next_mode = GP100_UI_MODE_TUNER;
            return GP100_PLAY_MODE_CHANGED;
        }
        if (decoded.action == GP100_BUTTON_REPEAT) {
            *next_mode = GP100_UI_MODE_LOOPER;
            return GP100_PLAY_MODE_CHANGED;
        }
        return GP100_PLAY_MODE_UNHANDLED;
    }
    if (decoded.kind != GP100_UI_CONTROL_BUTTON) {
        return GP100_PLAY_MODE_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_DRUM) {
        if (decoded.action == GP100_BUTTON_RELEASE) {
            *drum_enabled = !*drum_enabled;
            return GP100_PLAY_MODE_DRUM_TOGGLED;
        }
        if (decoded.action == GP100_BUTTON_HOLD) {
            *next_mode = GP100_UI_MODE_DRUM;
            return GP100_PLAY_MODE_CHANGED;
        }
        return GP100_PLAY_MODE_UNHANDLED;
    }
    if (decoded.action != GP100_BUTTON_RELEASE) {
        return GP100_PLAY_MODE_UNHANDLED;
    }
    switch ((gp100_button_id_t)decoded.control) {
    case GP100_BUTTON_GLOBAL:
        target = GP100_UI_MODE_GLOBAL;
        break;
    case GP100_BUTTON_EDIT:
    case GP100_BUTTON_PARA:
        target = GP100_UI_MODE_EDIT;
        break;
    case GP100_BUTTON_SAVE:
        target = GP100_UI_MODE_SAVE;
        break;
    default:
        return GP100_PLAY_MODE_UNHANDLED;
    }
    *next_mode = target;
    return GP100_PLAY_MODE_CHANGED;
}

static gp100_play_event_result_t play_action_event_result(
    gp100_play_action_result_t result)
{
    switch (result) {
    case GP100_PLAY_ACTION_SETTINGS_CHANGED:
        return GP100_PLAY_EVENT_SETTINGS_CHANGED;
    case GP100_PLAY_ACTION_PRESET_CHANGED:
        return GP100_PLAY_EVENT_PRESET_CHANGED;
    case GP100_PLAY_ACTION_SELECT_PRESET:
        return GP100_PLAY_EVENT_PRESET_SELECTED;
    case GP100_PLAY_ACTION_TAP_TEMPO_TOGGLED:
        return GP100_PLAY_EVENT_TAP_TEMPO_TOGGLED;
    case GP100_PLAY_ACTION_TAP_TEMPO_ARMED:
        return GP100_PLAY_EVENT_TAP_TEMPO_ARMED;
    case GP100_PLAY_ACTION_TAP_TEMPO_APPLIED:
        return GP100_PLAY_EVENT_TAP_TEMPO_APPLIED;
    case GP100_PLAY_ACTION_UNCHANGED:
        return GP100_PLAY_EVENT_UNHANDLED;
    default:
        return GP100_PLAY_EVENT_INVALID;
    }
}

gp100_play_event_result_t gp100_play_handle_event(
    gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    uint16_t *logical_preset,
    bool *drum_enabled,
    bool *tap_tempo_enabled,
    bool *footswitch_release_pending,
    uint32_t *tap_count,
    uint32_t *last_tap_tick,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode)
{
    gp100_ui_control_event_t decoded;
    gp100_play_action_result_t action;
    gp100_play_mode_result_t mode;

    if (settings == NULL || preset == NULL || logical_preset == NULL ||
        drum_enabled == NULL || tap_tempo_enabled == NULL ||
        footswitch_release_pending == NULL || tap_count == NULL ||
        last_tap_tick == NULL ||
        event == NULL || next_mode == NULL ||
        !gp100_decode_ui_control_event(event, &decoded)) {
        return GP100_PLAY_EVENT_INVALID;
    }
    if (decoded.kind == GP100_UI_CONTROL_ENCODER) {
        if (decoded.control > GP100_ENCODER_PARA) {
            return GP100_PLAY_EVENT_INVALID;
        }
        action = gp100_play_adjust(
            settings, preset, logical_preset,
            (gp100_play_control_t)decoded.control,
            decoded.action == GP100_BUTTON_PRESS,
            decoded.amount);
        return play_action_event_result(action);
    }
    if (decoded.kind == GP100_UI_CONTROL_BUTTON &&
        decoded.action == GP100_BUTTON_RELEASE) {
        const bool suppress_footswitch = *footswitch_release_pending;
        *footswitch_release_pending = false;
        if (decoded.control <= GP100_BUTTON_FOOTSWITCH_PLUS) {
            if (suppress_footswitch) {
                return GP100_PLAY_EVENT_UNHANDLED;
            }
            action = *tap_tempo_enabled
                         ? gp100_play_tap_tempo(
                               preset, tap_count, last_tap_tick, event->value)
                         : gp100_play_footswitch(
                               settings, preset, logical_preset,
                               decoded.control);
            return play_action_event_result(action);
        }
    }
    if (decoded.kind == GP100_UI_CONTROL_BUTTON &&
        decoded.control <= GP100_BUTTON_FOOTSWITCH_PLUS &&
        decoded.action == GP100_BUTTON_HOLD) {
        action = gp100_play_footswitch_hold(
            settings, tap_tempo_enabled, tap_count, last_tap_tick,
            decoded.control, event->value);
        if (action == GP100_PLAY_ACTION_SETTINGS_CHANGED ||
            (action == GP100_PLAY_ACTION_TAP_TEMPO_TOGGLED &&
             *tap_tempo_enabled)) {
            *footswitch_release_pending = true;
        }
        return play_action_event_result(action);
    }
    mode = gp100_play_handle_mode_event(
        event, drum_enabled, next_mode);
    switch (mode) {
    case GP100_PLAY_MODE_DRUM_TOGGLED:
        return GP100_PLAY_EVENT_DRUM_TOGGLED;
    case GP100_PLAY_MODE_CHANGED:
        return GP100_PLAY_EVENT_MODE_CHANGED;
    case GP100_PLAY_MODE_UNHANDLED:
        return GP100_PLAY_EVENT_UNHANDLED;
    default:
        return GP100_PLAY_EVENT_INVALID;
    }
}

bool gp100_play_screen_patch_code(uint16_t logical_preset,
                                       char code[4])
{
    uint16_t number;

    if (code == NULL || logical_preset >= GP100_TOTAL_LOGICAL_PRESETS) {
        return false;
    }
    code[0] = logical_preset < GP100_PRESETS_PER_BANK ? 'P' : 'F';
    number = (uint16_t)(logical_preset % GP100_PRESETS_PER_BANK + 1u);
    code[1] = (char)('0' + number / 10u);
    code[2] = (char)('0' + number % 10u);
    code[3] = '\0';
    return true;
}

const char *gp100_play_screen_family_label(
    gp100_effect_family_t family)
{
    return (unsigned)family < GP100_EFFECT_FAMILY_COUNT
               ? family_labels[(unsigned)family]
               : NULL;
}

bool gp100_play_screen_default_theme(
    gp100_play_screen_theme_t *theme,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font)
{
    static const uint16_t effect_colors[GP100_EFFECT_FAMILY_COUNT] =
        GP100_UI_EFFECT_COLORS;
    size_t index;

    if (theme == NULL || regular_font == NULL) {
        return false;
    }
    theme->large_font = large_font != NULL ? large_font : regular_font;
    theme->regular_font = regular_font;
    theme->small_font = small_font != NULL ? small_font : regular_font;
    theme->background_rgb565 = GP100_UI_BACKGROUND;
    theme->foreground_rgb565 = GP100_UI_FOREGROUND;
    theme->top_rgb565 = GP100_UI_PANEL;
    theme->middle_rgb565 = GP100_UI_CONTROL;
    theme->bottom_rgb565 = GP100_UI_PANEL;
    theme->track_rgb565 = GP100_UI_TRACK;
    theme->disabled_rgb565 = GP100_UI_MUTED;
    theme->accent_rgb565 = GP100_UI_ACCENT;
    for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
        theme->effect_rgb565[index] =
            effect_colors[index];
    }
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

static void format_unsigned(char buffer[4], uint16_t value)
{
    size_t index = 0u;

    if (value >= 100u) {
        buffer[index++] = (char)('0' + value / 100u);
        value %= 100u;
        buffer[index++] = (char)('0' + value / 10u);
    } else if (value >= 10u) {
        buffer[index++] = (char)('0' + value / 10u);
    }
    buffer[index++] = (char)('0' + value % 10u);
    buffer[index] = '\0';
}

static void draw_expression(gp100_display_canvas_t *canvas,
                            const gp100_play_screen_snapshot_t *snapshot,
                            const gp100_play_screen_theme_t *theme)
{
    if (!snapshot->expression_active) {
        return;
    }
    const gp100_play_effect_t *pedal = &snapshot->expression_pedal;
    if (pedal->enabled) {
        gp100_effect_art_draw(canvas, kExpressionIconX, kExpressionIconY,
                              GP100_EFFECT_ART_WIDTH, GP100_EFFECT_ART_HEIGHT,
                              pedal->family, pedal->algorithm,
                              theme->effect_rgb565[pedal->family], theme->top_rgb565, true);
    } else {
        gp100_effect_art_draw_silhouette(canvas, kExpressionIconX, kExpressionIconY,
                                        pedal->family, pedal->algorithm, theme->disabled_rgb565);
    }
}

static void draw_drum_state(gp100_display_canvas_t *canvas,
                            const gp100_play_screen_snapshot_t *snapshot,
                            const gp100_play_screen_theme_t *theme)
{
    if (!snapshot->drum_enabled) {
        return;
    }
    /* GP100_UI_ASSET_REPLACEMENT: rhythm-enabled pictogram. */
    (void)gp100_display_fill_rect(canvas, 137, 35, 3u, 9u,
                                      theme->accent_rgb565);
    (void)gp100_display_fill_rect(canvas, 143, 28, 3u, 16u,
                                      theme->accent_rgb565);
    (void)gp100_display_fill_rect(canvas, 149, 32, 3u, 12u,
                                      theme->accent_rgb565);
}

static bool draw_patch_mode(gp100_display_canvas_t *canvas,
                            const gp100_play_screen_theme_t *theme)
{
    /* GP100_UI_ASSET_REPLACEMENT: patch-navigation indicators. */
    uint16_t accent = theme->accent_rgb565;

    (void)gp100_display_draw_line(canvas, 23, 75, 31, 67, accent);
    (void)gp100_display_draw_line(canvas, 23, 75, 31, 83, accent);
    (void)gp100_display_draw_line(canvas, 137, 75, 129, 67, accent);
    (void)gp100_display_draw_line(canvas, 137, 75, 129, 83, accent);
    (void)gp100_display_draw_line(canvas, 79, 62, 79, 89, theme->track_rgb565);
    return draw_text(canvas, theme->regular_font, 47, 72, "-",
                     GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                     theme->middle_rgb565) &&
           draw_text(canvas, theme->regular_font, 113, 72, "+",
                     GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                     theme->middle_rgb565);
}

static bool draw_stomp_effects(
    gp100_display_canvas_t *canvas,
    const gp100_play_screen_snapshot_t *snapshot,
    const gp100_play_screen_theme_t *theme)
{
    enum { kSwitchWidth = kMiddleWidth / 2, kLabelWidth = 10, kMaxColumns = 4 };
    static const char *const labels[] = {"-", "+"};

    (void)gp100_display_draw_line(canvas, kMiddleX + kSwitchWidth - 1,
                                  kMiddleY + 2, kMiddleX + kSwitchWidth - 1,
                                  kMiddleY + kMiddleHeight - 3, theme->track_rgb565);
    for (size_t sw = 0u; sw < GP100_PLAY_STOMP_SWITCH_COUNT; ++sw) {
        const int32_t side_x = kMiddleX + (int32_t)sw * kSwitchWidth;
        if (!draw_text(canvas, theme->small_font, side_x + 4, kMiddleY + 14,
                       labels[sw], GP100_TEXT_ALIGN_CENTER,
                       theme->accent_rgb565, theme->middle_rgb565)) {
            return false;
        }

        const int count = snapshot->stomp_count[sw];
        const int columns = count < kMaxColumns ? count : kMaxColumns;
        const int rows = count > kMaxColumns ? 2 : 1;
        const int width = columns < 3 ? (int)GP100_EFFECT_ART_WIDTH
                                      : (columns == 3 ? 20 : 16);
        const int height = (width * (int)GP100_EFFECT_ART_HEIGHT +
                            (int)GP100_EFFECT_ART_WIDTH / 2) /
                           (int)GP100_EFFECT_ART_WIDTH;
        const int y = kMiddleY + (kMiddleHeight - rows * height - (rows - 1) * 2) / 2;
        for (int first = 0; first < count; first += kMaxColumns) {
            const int remaining = count - first;
            const int in_row = remaining < kMaxColumns ? remaining : kMaxColumns;
            const int row_width = in_row * width + in_row - 1;
            const int32_t x = side_x + kLabelWidth +
                (kSwitchWidth - kLabelWidth - 1 - row_width) / 2;
            for (int column = 0; column < in_row; ++column) {
                const gp100_play_effect_t *pedal =
                    &snapshot->stomp_effects[sw][first + column];
                const int32_t pedal_x = x + column * (width + 1);
                const int32_t pedal_y = y + (first / kMaxColumns) * (height + 2);
                gp100_effect_art_draw(canvas, pedal_x, pedal_y, (uint16_t)width,
                                       (uint16_t)height, pedal->family, pedal->algorithm,
                                       pedal->algorithm == GP100_PATCH_UNKNOWN_ALGORITHM
                                           ? theme->disabled_rgb565 : theme->effect_rgb565[pedal->family],
                                       theme->middle_rgb565, pedal->enabled);
                if (pedal->enabled) {
                    (void)gp100_display_fill_rect(
                        canvas, pedal_x + width - 2, pedal_y + height - 2,
                        2u, 2u, theme->accent_rgb565);
                }
            }
        }
    }
    return true;
}

static bool draw_stomp_mode(
    gp100_display_canvas_t *canvas,
    const gp100_play_screen_snapshot_t *snapshot,
    const gp100_play_screen_theme_t *theme)
{
    static const int32_t first_x[GP100_PLAY_STOMP_SWITCH_COUNT] = {
        3, 82
    };
    size_t switch_index;
    size_t assignment;

    if (snapshot->native) {
        return draw_stomp_effects(canvas, snapshot, theme);
    }
    for (switch_index = 0u;
         switch_index < GP100_PLAY_STOMP_SWITCH_COUNT;
         ++switch_index) {
        for (assignment = 0u;
             assignment < GP100_PLAY_STOMP_ASSIGNMENTS_PER_SWITCH;
             ++assignment) {
            uint16_t family =
                snapshot->stomp_family[switch_index][assignment];
            int32_t x = first_x[switch_index] + (int32_t)assignment * 24;
            uint16_t color;

            if (family == GP100_PLAY_UNASSIGNED_FAMILY) {
                continue;
            }
            color = snapshot->effect_enabled[family]
                        ? theme->effect_rgb565[family]
                        : theme->disabled_rgb565;
            if (!draw_text(
                    canvas, theme->small_font, x + 10, 67,
                    gp100_play_screen_family_label(
                        (gp100_effect_family_t)family),
                    GP100_TEXT_ALIGN_CENTER, color,
                    theme->middle_rgb565)) {
                return false;
            }
        }
    }
    return true;
}

static void draw_control_bar(gp100_display_canvas_t *canvas,
                             int32_t x,
                             uint16_t numerator,
                             uint16_t denominator,
                             const gp100_play_screen_theme_t *theme)
{
    uint16_t filled = denominator == 0u ? 0u :
        (uint16_t)(((uint32_t)numerator * kControlWidth) / denominator);

    if (filled > kControlWidth) {
        filled = kControlWidth;
    }
    (void)gp100_display_fill_rect(
        canvas, x, kControlY, kControlWidth, kControlHeight,
        theme->track_rgb565);
    if (filled != 0u) {
        (void)gp100_display_fill_rect(
            canvas, x, kControlY, filled, kControlHeight,
            theme->accent_rgb565);
    }
}

static bool draw_controls(
    gp100_display_canvas_t *canvas,
    const gp100_play_screen_snapshot_t *snapshot,
    const gp100_play_screen_theme_t *theme)
{
    char master[4];
    char tempo[4];
    char volume[4];

    format_unsigned(master, snapshot->master_volume);
    format_unsigned(tempo, snapshot->tempo_bpm);
    format_unsigned(volume, snapshot->preset_volume);
    draw_control_bar(canvas, 3, snapshot->master_volume, 100u, theme);
    draw_control_bar(canvas, 57, (uint16_t)(snapshot->tempo_bpm - 40u),
                     210u, theme);
    draw_control_bar(canvas, 111, snapshot->preset_volume, 100u, theme);
    return draw_text(canvas, theme->small_font, 25, 97, "Master",
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->bottom_rgb565) &&
           draw_text(canvas, theme->small_font, 80, 97, "BPM",
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->bottom_rgb565) &&
           draw_text(canvas, theme->small_font, 133, 97, "P-VOL",
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->bottom_rgb565) &&
           draw_text(canvas, theme->regular_font, 25, 116, master,
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->bottom_rgb565) &&
           draw_text(canvas, theme->regular_font, 80, 116, tempo,
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->bottom_rgb565) &&
           draw_text(canvas, theme->regular_font, 133, 116, volume,
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->bottom_rgb565);
}

/* Shown after PARA was turned with unsaved edits: covers the footswitch area. */
static bool draw_unsaved_warning(gp100_display_canvas_t *canvas,
                                 const gp100_play_screen_theme_t *theme)
{
    const int32_t y = kMiddleY;
    (void)gp100_display_fill_rect(canvas, kMiddleX, y, kMiddleWidth, kMiddleHeight,
                                  theme->middle_rgb565);
    for (int32_t inset = 0; inset < 2; ++inset) {
        (void)gp100_display_draw_line(canvas, kMiddleX + inset, y + inset,
                                      kMiddleX + kMiddleWidth - 1 - inset, y + inset, GP100_UI_DANGER);
        (void)gp100_display_draw_line(canvas, kMiddleX + inset, y + kMiddleHeight - 1 - inset,
                                      kMiddleX + kMiddleWidth - 1 - inset,
                                      y + kMiddleHeight - 1 - inset, GP100_UI_DANGER);
        (void)gp100_display_draw_line(canvas, kMiddleX + inset, y + inset,
                                      kMiddleX + inset, y + kMiddleHeight - 1 - inset, GP100_UI_DANGER);
        (void)gp100_display_draw_line(canvas, kMiddleX + kMiddleWidth - 1 - inset, y + inset,
                                      kMiddleX + kMiddleWidth - 1 - inset,
                                      y + kMiddleHeight - 1 - inset, GP100_UI_DANGER);
    }
    return draw_text(canvas, theme->small_font, 80, y + 4, "UNSAVED CHANGES",
                     GP100_TEXT_ALIGN_CENTER, GP100_UI_DANGER, theme->middle_rgb565) &&
           draw_text(canvas, theme->small_font, 80, y + 15, "Turn PARA again to go on",
                     GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565, theme->middle_rgb565) &&
           draw_text(canvas, theme->small_font, 80, y + 25, "edits lost unless SAVE",
                     GP100_TEXT_ALIGN_CENTER, theme->disabled_rgb565, theme->middle_rgb565);
}

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_play_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_play_screen_snapshot_t *snapshot,
    const gp100_play_screen_theme_t *theme)
{
    char patch_code[4];

    if (canvas == NULL || theme == NULL || !snapshot_is_valid(snapshot) ||
        theme->large_font == NULL || theme->regular_font == NULL ||
        theme->small_font == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT ||
        !gp100_play_screen_patch_code(
            snapshot->logical_preset, patch_code)) {
        return false;
    }
    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, theme->background_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH, kTopHeight,
        theme->top_rgb565);
    if (snapshot->unavailable_effects) {
        if (!draw_text(canvas, theme->small_font, 80, 47, "Unavailable FX",
                       GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565, theme->top_rgb565)) {
            return false;
        }
    } else {
        (void)gp100_display_draw_line(canvas, 29, 50, 130, 50, theme->accent_rgb565);
    }
    (void)gp100_display_fill_rect(
        canvas, kMiddleX, kMiddleY, kMiddleWidth, kMiddleHeight,
        theme->middle_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 0, kBottomY, GP100_UI_LOGICAL_WIDTH, kBottomHeight,
        theme->bottom_rgb565);

    draw_expression(canvas, snapshot, theme);
    draw_drum_state(canvas, snapshot, theme);
    if (!draw_text(canvas, theme->large_font, 80, kPresetNumberY, patch_code,
                   GP100_TEXT_ALIGN_CENTER,
                   theme->accent_rgb565, theme->top_rgb565) ||
        !draw_text(canvas, theme->regular_font, 80, 36,
                   snapshot->preset_name, GP100_TEXT_ALIGN_CENTER,
                   theme->foreground_rgb565, theme->top_rgb565)) {
        return false;
    }
    if (snapshot->preset_modified &&
        !draw_text(canvas, theme->small_font, 113, 6, "*",
                   GP100_TEXT_ALIGN_LEFT,
                   theme->accent_rgb565, theme->top_rgb565)) {
        return false;
    }
    if (snapshot->footswitch_mode == GP100_FOOTSWITCH_MODE_PATCH) {
        if (!draw_patch_mode(canvas, theme)) {
            return false;
        }
    } else if (!draw_stomp_mode(canvas, snapshot, theme)) {
        return false;
    }
    if (snapshot->unsaved_warning && !draw_unsaved_warning(canvas, theme)) {
        return false;
    }
    return draw_controls(canvas, snapshot, theme);
}
