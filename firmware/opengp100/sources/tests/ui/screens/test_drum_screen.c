#include "ui/screens/drum_screen.h"
#include "ui/screens/play_screen.h"

#include <string.h>

static const gp100_bitmap_font_t empty_font = {
    NULL, 0u, 8u, 5u
};

static bool read_setting(const gp100_settings_record_t *settings,
                         gp100_settings_field_t field,
                         uint16_t expected)
{
    int16_t value;

    return gp100_settings_get_i16(settings, field, &value) &&
           value >= 0 && (uint16_t)value == expected;
}

int main(void)
{
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    gp100_display_canvas_t canvas;
    gp100_drum_screen_theme_t theme;
    gp100_drum_screen_snapshot_t snapshot = {
        true, 3u, 48u, 0u, 120u, 90u
    };
    gp100_settings_record_t settings;
    gp100_preset_record_t preset;
    uint16_t logical_preset = 0u;
    uint16_t first;
    uint16_t selected;
    gp100_event_t event = {0u, 0u};
    gp100_ui_mode_t next_mode = GP100_UI_MODE_DRUM;
    bool drum_release_guard = false;

    memset(pixels, 0xa5, sizeof(pixels));
    if (!gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_drum_screen_default_theme(&theme, &empty_font, NULL) ||
        !gp100_drum_screen_render(&canvas, &snapshot, &theme)) {
        return 1;
    }
    if (strcmp(gp100_drum_genre_name(0u), "Electronic") != 0 ||
        strcmp(gp100_drum_genre_name(7u), "Metro") != 0 ||
        strcmp(gp100_drum_style_name(48u), "Pub") != 0 ||
        strcmp(gp100_drum_style_name(99u), "9/8") != 0 ||
        gp100_drum_genre_name(8u) != NULL ||
        gp100_drum_style_name(100u) != NULL) {
        return 2;
    }
    if (!gp100_drum_genre_window(3u, &first, &selected) ||
        first != 3u || selected != 0u ||
        !gp100_drum_genre_window(6u, &first, &selected) ||
        first != 4u || selected != 2u) {
        return 3;
    }
    memset(&settings, 0, sizeof(settings));
    memset(&preset, 0, sizeof(preset));
    if (!gp100_settings_set_i16(
            &settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            GP100_FOOTSWITCH_MODE_PATCH) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_GENRE, 3) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_STYLE, 48) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_TEMPO, 120) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_VOLUME, 90) ||
        !gp100_drum_screen_snapshot_from_settings(
            &snapshot, &settings, true) ||
        !snapshot.enabled || snapshot.genre != 3u ||
        snapshot.absolute_style != 48u || snapshot.style_in_genre != 0u ||
        snapshot.tempo_bpm != 120u || snapshot.volume != 90u) {
        return 4;
    }
    if (pixels[0] != theme.background_rgb565 ||
        pixels[41u * GP100_UI_LOGICAL_WIDTH + 23u] !=
            theme.accent_rgb565 ||
        pixels[30u * GP100_UI_LOGICAL_WIDTH + 90u] !=
            theme.accent_rgb565 ||
        pixels[94u * GP100_UI_LOGICAL_WIDTH] != theme.panel_rgb565 ||
        pixels[112u * GP100_UI_LOGICAL_WIDTH + 2u] !=
            theme.control_rgb565 ||
        pixels[111u * GP100_UI_LOGICAL_WIDTH + 70u] !=
            theme.accent_rgb565 ||
        pixels[111u * GP100_UI_LOGICAL_WIDTH + 150u] !=
            theme.accent_rgb565) {
        return 5;
    }
    if (!gp100_drum_adjust_setting(
            &settings, GP100_DRUM_CONTROL_STYLE, true) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_STYLE, 49u) ||
        !read_setting(
            &settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 1u) ||
        !gp100_settings_validate(&settings) ||
        !gp100_drum_adjust_setting(
            &settings, GP100_DRUM_CONTROL_STYLE, false) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_STYLE, 48u) ||
        !gp100_drum_adjust_setting(
            &settings, GP100_DRUM_CONTROL_GENRE, true) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_GENRE, 4u) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_STYLE, 52u) ||
        !read_setting(
            &settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0u)) {
        return 6;
    }
    if (!gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 6) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_STYLE, 58) ||
        !gp100_drum_adjust_setting(
            &settings, GP100_DRUM_CONTROL_STYLE, true) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_STYLE, 58u) ||
        !gp100_drum_adjust_setting(
            &settings, GP100_DRUM_CONTROL_TEMPO, true) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_TEMPO, 121u) ||
        !gp100_drum_adjust_setting(
            &settings, GP100_DRUM_CONTROL_VOLUME, true) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_VOLUME, 91u)) {
        return 7;
    }
    if (!gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_TEMPO, 250) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_VOLUME, 0) ||
        !gp100_drum_adjust_setting(
            &settings, GP100_DRUM_CONTROL_TEMPO, true) ||
        !gp100_drum_adjust_setting(
            &settings, GP100_DRUM_CONTROL_VOLUME, false) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_TEMPO, 250u) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_VOLUME, 0u) ||
        !gp100_drum_toggle_playback(&snapshot.enabled) ||
        snapshot.enabled ||
        !gp100_drum_toggle_playback(&snapshot.enabled) ||
        !snapshot.enabled) {
        return 8;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_2, false, 1u);
    if (gp100_drum_handle_event(
            &settings, &preset, &logical_preset,
            &snapshot.enabled, &drum_release_guard, &event, &next_mode) !=
            GP100_DRUM_EVENT_SETTINGS_CHANGED ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_TEMPO, 249u)) {
        return 9;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_drum_handle_event(
            &settings, &preset, &logical_preset,
            &snapshot.enabled, &drum_release_guard, &event, &next_mode) !=
            GP100_DRUM_EVENT_PLAYBACK_TOGGLED || snapshot.enabled) {
        return 10;
    }
    drum_release_guard = true;
    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_RELEASE);
    if (gp100_drum_handle_event(
            &settings, &preset, &logical_preset,
            &snapshot.enabled, &drum_release_guard, &event, &next_mode) !=
            GP100_DRUM_EVENT_RELEASE_GUARDED ||
        drum_release_guard || snapshot.enabled ||
        gp100_drum_handle_event(
            &settings, &preset, &logical_preset,
            &snapshot.enabled, &drum_release_guard, &event, &next_mode) !=
            GP100_DRUM_EVENT_PLAYBACK_TOGGLED ||
        !snapshot.enabled) {
        return 10;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_GLOBAL, GP100_BUTTON_RELEASE);
    if (gp100_drum_handle_event(
            &settings, &preset, &logical_preset,
            &snapshot.enabled, &drum_release_guard, &event, &next_mode) !=
            GP100_DRUM_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_GLOBAL) {
        return 11;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_RELEASE);
    if (gp100_drum_handle_event(
            &settings, &preset, &logical_preset,
            &snapshot.enabled, &drum_release_guard, &event, &next_mode) !=
            GP100_DRUM_EVENT_PRESET_SELECTED ||
        logical_preset != GP100_TOTAL_LOGICAL_PRESETS - 1u) {
        return 12;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_REPEAT);
    if (gp100_drum_handle_event(
            &settings, &preset, &logical_preset,
            &snapshot.enabled, &drum_release_guard, &event, &next_mode) !=
            GP100_DRUM_EVENT_PRESET_SELECTED ||
        logical_preset != 0u ||
        gp100_drum_handle_event(
            NULL, &preset, &logical_preset,
            &snapshot.enabled, &drum_release_guard, &event, &next_mode) !=
            GP100_DRUM_EVENT_INVALID) {
        return 14;
    }
    if (!gp100_settings_set_i16(&settings, GP100_SETTINGS_DRUM_TEMPO, 11) ||
        !gp100_drum_adjust_setting(&settings, GP100_DRUM_CONTROL_TEMPO, false) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_TEMPO, 10u) ||
        !gp100_drum_adjust_setting(&settings, GP100_DRUM_CONTROL_TEMPO, false) ||
        !read_setting(&settings, GP100_SETTINGS_DRUM_TEMPO, 10u)) {
        return 15;
    }
    snapshot.tempo_bpm = 10u;
    if (!gp100_drum_screen_render(&canvas, &snapshot, &theme)) {
        return 16;
    }
    snapshot.genre = 8u;
    if (gp100_drum_screen_render(&canvas, &snapshot, &theme) ||
        gp100_drum_genre_window(8u, &first, &selected) ||
        gp100_drum_screen_default_theme(NULL, &empty_font, NULL) ||
        gp100_drum_adjust_setting(
            NULL, GP100_DRUM_CONTROL_STYLE, true) ||
        gp100_drum_adjust_setting(
            &settings, (gp100_drum_control_t)4, true) ||
        gp100_drum_toggle_playback(NULL)) {
        return 17;
    }
    return 0;
}
