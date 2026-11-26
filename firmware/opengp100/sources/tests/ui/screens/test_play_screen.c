#include "assets/ui/development_font.h"
#include "ui/render/effect_art.h"
#include "ui/screens/play_screen.h"

#include <string.h>

static const gp100_bitmap_font_t empty_font = {
    NULL, 0u, 8u, 5u
};

static void write_u16(uint8_t *bytes, size_t offset, uint16_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1u] = (uint8_t)(value >> 8);
}

static bool test_expression_icon(gp100_display_canvas_t *canvas,
                                 const gp100_play_screen_theme_t *theme,
                                 const gp100_preset_record_t *source,
                                 const gp100_settings_record_t *settings)
{
    gp100_preset_record_t preset = *source;
    gp100_play_screen_snapshot_t snapshot;
    gp100_patch_block_t block;
    bool silhouette[GP100_EFFECT_ART_WIDTH * GP100_EFFECT_ART_HEIGHT];
    size_t painted = 0u;
    size_t colored = 0u;

    gp100_patch_init(&preset);
    if (gp100_patch_insert(&preset, 0u, GP100_EFFECT_PRE_FX, 0u) != GP100_PATCH_OK ||
        !gp100_patch_block_at(&preset, 0u, &block) ||
        !gp100_play_screen_snapshot_from_records(&snapshot, 0u, &preset, settings, false, false) ||
        snapshot.expression_active) {
        return false;
    }
    const gp100_patch_expression_t assignment = {
        .block_id = block.id, .parameter = 0u, .heel = 0.0f, .toe = 99.0f
    };
    if (!gp100_patch_expression_set(&preset, &assignment) ||
        !gp100_play_screen_snapshot_from_records(&snapshot, 0u, &preset, settings, false, false) ||
        snapshot.expression_active || !gp100_play_screen_render(canvas, &snapshot, theme)) {
        return false;
    }
    for (size_t y = 6u; y < 6u + GP100_EFFECT_ART_HEIGHT; ++y) {
        for (size_t x = 5u; x < 5u + GP100_EFFECT_ART_WIDTH; ++x) {
            if (canvas->pixels[y * canvas->stride_pixels + x] != theme->top_rgb565) {
                return false;
            }
        }
    }

    if (!gp100_patch_expression_set_mode(&preset, GP100_EXPRESSION_PEDAL_EFFECT) ||
        !gp100_play_screen_snapshot_from_records(&snapshot, 0u, &preset, settings, false, false) ||
        !snapshot.expression_active || !snapshot.expression_pedal.enabled ||
        snapshot.expression_pedal.family != block.family ||
        snapshot.expression_pedal.algorithm != block.algorithm ||
        !gp100_play_screen_render(canvas, &snapshot, theme)) {
        return false;
    }
    for (size_t y = 0u; y < GP100_EFFECT_ART_HEIGHT; ++y) {
        for (size_t x = 0u; x < GP100_EFFECT_ART_WIDTH; ++x) {
            const uint16_t pixel = canvas->pixels[(y + 6u) * canvas->stride_pixels + x + 5u];
            const bool ink = pixel != theme->top_rgb565;
            silhouette[y * GP100_EFFECT_ART_WIDTH + x] = ink;
            painted += ink;
            colored += ink && pixel != theme->disabled_rgb565;
        }
    }
    block.enabled = false;
    if (painted == 0u || colored == 0u || !gp100_patch_update(&preset, &block) ||
        !gp100_play_screen_snapshot_from_records(&snapshot, 0u, &preset, settings, false, false) ||
        !snapshot.expression_active || snapshot.expression_pedal.enabled ||
        !gp100_play_screen_render(canvas, &snapshot, theme)) {
        return false;
    }
    for (size_t y = 0u; y < GP100_EFFECT_ART_HEIGHT; ++y) {
        for (size_t x = 0u; x < GP100_EFFECT_ART_WIDTH; ++x) {
            const uint16_t expected = silhouette[y * GP100_EFFECT_ART_WIDTH + x]
                ? theme->disabled_rgb565 : theme->top_rgb565;
            if (canvas->pixels[(y + 6u) * canvas->stride_pixels + x + 5u] != expected) {
                return false;
            }
        }
    }
    return true;
}

int main(void)
{
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    gp100_display_canvas_t canvas;
    gp100_play_screen_theme_t theme;
    gp100_play_screen_snapshot_t snapshot;
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    char code[4];
    size_t assignment;
    size_t family;
    int16_t footswitch_mode;
    uint16_t logical_preset = 0u;
    bool enabled;
    bool drum_enabled = false;
    bool tap_tempo_enabled = false;
    bool footswitch_release_pending = false;
    uint32_t tap_count = 0u;
    uint32_t last_tap_tick = 0u;
    gp100_ui_mode_t next_mode = GP100_UI_MODE_PLAY;
    gp100_event_t event = {0u, 0u};

    memset(&preset, 0, sizeof(preset));
    memset(&settings, 0, sizeof(settings));
    memcpy(&preset.bytes[2], "It's GP-100", 11u);
    if (!gp100_preset_set_tempo_bpm(&preset, 120u) ||
        !gp100_preset_set_level(&preset, 50u) ||
        !gp100_preset_set_expression_pedal_target(
            &preset, GP100_EXPRESSION_PEDAL_EFFECT) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            GP100_FOOTSWITCH_MODE_PATCH) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_MASTER_VOLUME, 50)) {
        return 1;
    }
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        write_u16(preset.bytes, 0x260u + family * 2u,
                  (uint16_t)(family == GP100_EFFECT_AMPLIFIER));
    }
    for (assignment = 0u;
         assignment < GP100_STOMP_FOOTSWITCH_COUNT *
                          GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH;
         ++assignment) {
        if (!gp100_preset_set_control_assignment(
                &preset,
                assignment / GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH,
                assignment % GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH,
                (uint16_t)assignment)) {
            return 2;
        }
    }
    if (!gp100_play_screen_snapshot_from_records(
            &snapshot, 0u, &preset, &settings, true, true) ||
        snapshot.logical_preset != 0u ||
        strcmp(snapshot.preset_name, "It's GP-100") != 0 ||
        snapshot.master_volume != 50u || snapshot.tempo_bpm != 120u ||
        snapshot.preset_volume != 50u ||
        snapshot.footswitch_mode != GP100_FOOTSWITCH_MODE_PATCH ||
        snapshot.expression_active || !snapshot.preset_modified ||
        !snapshot.drum_enabled ||
        snapshot.stomp_family[1][2] != 5u ||
        !snapshot.effect_enabled[GP100_EFFECT_AMPLIFIER]) {
        return 3;
    }
    if (!gp100_play_screen_patch_code(0u, code) ||
        strcmp(code, "P01") != 0 ||
        !gp100_play_screen_patch_code(98u, code) ||
        strcmp(code, "P99") != 0 ||
        !gp100_play_screen_patch_code(99u, code) ||
        strcmp(code, "F01") != 0 ||
        !gp100_play_screen_patch_code(197u, code) ||
        strcmp(code, "F99") != 0 ||
        gp100_play_screen_patch_code(198u, code) ||
        strcmp(gp100_play_screen_family_label(
                   GP100_EFFECT_DELAY), "DLY") != 0 ||
        gp100_play_screen_family_label(
            (gp100_effect_family_t)GP100_EFFECT_FAMILY_COUNT) !=
            NULL) {
        return 4;
    }
    memset(pixels, 0xa5, sizeof(pixels));
    if (!gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_play_screen_default_theme(
            &theme, gp100_development_font_10x14(), &empty_font,
            gp100_development_font_5x7()) ||
        !gp100_play_screen_render(&canvas, &snapshot, &theme)) {
        return 5;
    }
    if (pixels[0] != theme.top_rgb565 ||
        pixels[58u * GP100_UI_LOGICAL_WIDTH] !=
            theme.background_rgb565 ||
        pixels[58u * GP100_UI_LOGICAL_WIDTH + 2u] !=
            theme.middle_rgb565 ||
        pixels[95u * GP100_UI_LOGICAL_WIDTH] != theme.bottom_rgb565 ||
        pixels[12u * GP100_UI_LOGICAL_WIDTH + 62u] != theme.accent_rgb565 ||
        pixels[25u * GP100_UI_LOGICAL_WIDTH + 62u] != theme.accent_rgb565 ||
        pixels[14u * GP100_UI_LOGICAL_WIDTH + 13u] != theme.top_rgb565 ||
        pixels[24u * GP100_UI_LOGICAL_WIDTH + 6u] != theme.top_rgb565 ||
        pixels[6u * GP100_UI_LOGICAL_WIDTH + 6u] != theme.top_rgb565 ||
        pixels[45u * GP100_UI_LOGICAL_WIDTH + 8u] != theme.top_rgb565 ||
        pixels[62u * GP100_UI_LOGICAL_WIDTH + 79u] !=
            theme.track_rgb565 ||
        pixels[50u * GP100_UI_LOGICAL_WIDTH + 80u] !=
            theme.accent_rgb565 ||
        pixels[112u * GP100_UI_LOGICAL_WIDTH + 3u] !=
            theme.accent_rgb565 ||
        pixels[112u * GP100_UI_LOGICAL_WIDTH + 40u] !=
            theme.track_rgb565 ||
        pixels[112u * GP100_UI_LOGICAL_WIDTH + 57u] !=
            theme.accent_rgb565 ||
        pixels[112u * GP100_UI_LOGICAL_WIDTH + 90u] !=
            theme.track_rgb565) {
        return 6;
    }
    snapshot.footswitch_mode = GP100_FOOTSWITCH_MODE_STOMP;
    if (!gp100_play_screen_render(&canvas, &snapshot, &theme) ||
        !test_expression_icon(&canvas, &theme, &preset, &settings)) {
        return 7;
    }
    if (gp100_play_adjust(
            &settings, &preset, &logical_preset,
            GP100_PLAY_CONTROL_MASTER_VOLUME, true, 7u) !=
            GP100_PLAY_ACTION_SETTINGS_CHANGED ||
        gp100_play_adjust(
            &settings, &preset, &logical_preset,
            GP100_PLAY_CONTROL_TEMPO, false, 30u) !=
            GP100_PLAY_ACTION_PRESET_CHANGED ||
        gp100_preset_tempo_bpm(&preset) != 90u ||
        gp100_play_adjust(
            &settings, &preset, &logical_preset,
            GP100_PLAY_CONTROL_PRESET_VOLUME, true, 60u) !=
            GP100_PLAY_ACTION_PRESET_CHANGED ||
        gp100_preset_level(&preset) != 99u ||
        !gp100_settings_validate(&settings) ||
        gp100_preset_validate(&preset) != GP100_PRESET_VALID ||
        gp100_play_adjust(
            &settings, &preset, &logical_preset,
            GP100_PLAY_CONTROL_PRESET, false, 99u) !=
            GP100_PLAY_ACTION_SELECT_PRESET ||
        logical_preset != 197u) {
        return 8;
    }
    if (gp100_play_footswitch(
            &settings, &preset, &logical_preset, 1u) !=
            GP100_PLAY_ACTION_SELECT_PRESET ||
        logical_preset != 0u ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            GP100_FOOTSWITCH_MODE_STOMP) ||
        gp100_play_footswitch(
            &settings, &preset, &logical_preset, 0u) !=
            GP100_PLAY_ACTION_PRESET_CHANGED ||
        !gp100_preset_effect_enabled(
            &preset, GP100_EFFECT_PRE_FX, &enabled) || !enabled ||
        !gp100_preset_effect_enabled(
            &preset, GP100_EFFECT_DISTORTION, &enabled) || !enabled ||
        !gp100_preset_effect_enabled(
            &preset, GP100_EFFECT_AMPLIFIER, &enabled) || enabled ||
        gp100_preset_validate(&preset) != GP100_PRESET_VALID) {
        return 9;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_RELEASE);
    if (gp100_play_handle_mode_event(
            &event, &drum_enabled, &next_mode) !=
            GP100_PLAY_MODE_DRUM_TOGGLED || !drum_enabled ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 10;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_HOLD);
    if (gp100_play_handle_mode_event(
            &event, &drum_enabled, &next_mode) !=
            GP100_PLAY_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_DRUM) {
        return 11;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_GLOBAL, GP100_BUTTON_RELEASE);
    if (gp100_play_handle_mode_event(
            &event, &drum_enabled, &next_mode) !=
            GP100_PLAY_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_GLOBAL) {
        return 12;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_play_handle_mode_event(
            &event, &drum_enabled, &next_mode) !=
            GP100_PLAY_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_EDIT) {
        return 13;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    next_mode = GP100_UI_MODE_PLAY;
    if (gp100_play_handle_mode_event(
            &event, &drum_enabled, &next_mode) !=
            GP100_PLAY_MODE_UNHANDLED ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 13;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    if (gp100_play_handle_mode_event(
            &event, &drum_enabled, &next_mode) !=
            GP100_PLAY_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_SAVE) {
        return 14;
    }
    event.code = UINT32_C(0x40000000);
    if (gp100_play_handle_mode_event(
            &event, &drum_enabled, &next_mode) !=
            GP100_PLAY_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_TUNER) {
        return 15;
    }
    event.code = UINT32_C(0x40030000);
    if (gp100_play_handle_mode_event(
            &event, &drum_enabled, &next_mode) !=
            GP100_PLAY_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_LOOPER) {
        return 16;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_1, true, 1u);
    if (gp100_play_handle_event(
            &settings, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) !=
            GP100_PLAY_EVENT_SETTINGS_CHANGED) {
        return 17;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_RELEASE);
    if (gp100_play_handle_event(
            &settings, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) !=
            GP100_PLAY_EVENT_PRESET_CHANGED) {
        return 18;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_HOLD);
    tap_count = 4u;
    last_tap_tick = 123u;
    event.value = 900u;
    if (gp100_play_handle_event(
            &settings, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) !=
            GP100_PLAY_EVENT_TAP_TEMPO_TOGGLED ||
        !tap_tempo_enabled || tap_count != 0u || last_tap_tick != 900u) {
        return 23;
    }
    event.value = 1000u;
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_RELEASE);
    if (!footswitch_release_pending ||
        gp100_play_handle_event(
            &settings, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) != GP100_PLAY_EVENT_UNHANDLED ||
        footswitch_release_pending || tap_count != 0u || last_tap_tick != 900u) {
        return 25;
    }
    event.value = 1100u;
    if (gp100_play_handle_event(
            &settings, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) != GP100_PLAY_EVENT_TAP_TEMPO_APPLIED ||
        tap_count != 1u || last_tap_tick != 1100u ||
        gp100_preset_tempo_bpm(&preset) != 120u) {
        return 25;
    }
    event.value = 5001u;
    if (gp100_play_handle_event(
            &settings, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) != GP100_PLAY_EVENT_TAP_TEMPO_ARMED ||
        last_tap_tick != 5001u) {
        return 26;
    }
    event.value = 5601u;
    if (gp100_play_handle_event(
            &settings, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) != GP100_PLAY_EVENT_TAP_TEMPO_APPLIED ||
        gp100_preset_tempo_bpm(&preset) != 100u) {
        return 27;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_HOLD);
    if (gp100_play_handle_event(
            &settings, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) !=
            GP100_PLAY_EVENT_SETTINGS_CHANGED ||
        !gp100_settings_get_i16(
            &settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            &footswitch_mode) ||
        footswitch_mode != GP100_FOOTSWITCH_MODE_PATCH ||
        !gp100_settings_validate(&settings)) {
        return 24;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_RELEASE);
    if (!footswitch_release_pending ||
        gp100_play_handle_event(
            &settings, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) != GP100_PLAY_EVENT_UNHANDLED ||
        footswitch_release_pending || last_tap_tick != 5601u) {
        return 24;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    if (gp100_play_handle_event(
            &settings, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) !=
            GP100_PLAY_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_SAVE) {
        return 19;
    }
    snapshot.master_volume = 101u;
    if (gp100_play_screen_render(&canvas, &snapshot, &theme) ||
        gp100_play_screen_default_theme(NULL, NULL, NULL, NULL) ||
        gp100_play_adjust(
            NULL, &preset, &logical_preset,
            GP100_PLAY_CONTROL_TEMPO, true, 1u) !=
            GP100_PLAY_ACTION_INVALID ||
        gp100_play_footswitch(
            &settings, &preset, &logical_preset, 2u) !=
            GP100_PLAY_ACTION_INVALID ||
        gp100_preset_set_effect_enabled(
            NULL, GP100_EFFECT_PRE_FX, true)) {
        return 20;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_1, true, 1u);
    if (gp100_play_handle_mode_event(
            &event, &drum_enabled, &next_mode) !=
            GP100_PLAY_MODE_UNHANDLED ||
        gp100_play_handle_mode_event(
            NULL, &drum_enabled, &next_mode) !=
            GP100_PLAY_MODE_INVALID) {
        return 21;
    }
    if (gp100_play_handle_event(
            NULL, &preset, &logical_preset, &drum_enabled,
            &tap_tempo_enabled, &footswitch_release_pending,
            &tap_count, &last_tap_tick, &event, &next_mode) !=
            GP100_PLAY_EVENT_INVALID ||
        gp100_play_footswitch_hold(
            &settings, &tap_tempo_enabled, &tap_count, &last_tap_tick,
            2u, 0u) !=
            GP100_PLAY_ACTION_INVALID ||
        gp100_play_tap_tempo(
            NULL, &tap_count, &last_tap_tick, 0u) !=
            GP100_PLAY_ACTION_INVALID) {
        return 22;
    }
    return 0;
}
