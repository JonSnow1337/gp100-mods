#include "ui/screens/looper_screen.h"

#include <string.h>

static const gp100_bitmap_font_t empty_font = {
    NULL,
    0u,
    8u,
    5u,
};

int main(void)
{
    static int32_t storage[1000];
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    gp100_display_canvas_t canvas;
    gp100_looper_engine_t engine;
    gp100_looper_screen_theme_t theme;
    gp100_looper_screen_snapshot_t snapshot = {
        &engine,
        "It's GP-100",
        0u,
        false,
    };
    gp100_settings_record_t settings;
    uint16_t preset_index = 0u;
    bool changed;
    bool pair_release_guard = false;
    bool clear_feedback;
    bool drum_enabled = false;
    gp100_ui_mode_t next_mode = GP100_UI_MODE_LOOPER;
    gp100_event_t event = {0u, 0u};

    memset(pixels, 0xa5, sizeof(pixels));
    memset(&settings, 0, sizeof(settings));
    if (!gp100_looper_init(
            &engine, storage, sizeof(storage) / sizeof(storage[0]),
            GP100_LOOPER_PRE_EFFECTS, 90u, 80u) ||
        !gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_looper_screen_default_theme(
            &theme, &empty_font, NULL) ||
        !gp100_looper_screen_render(&canvas, &snapshot, &theme)) {
        return 1;
    }
    if (strcmp(gp100_looper_screen_status(&snapshot), "Empty") != 0 ||
        pixels[0] != theme.background_rgb565 ||
        pixels[35u * GP100_UI_LOGICAL_WIDTH + 3u] !=
            theme.track_rgb565 ||
        pixels[73u * GP100_UI_LOGICAL_WIDTH + 3u] !=
            theme.control_rgb565 ||
        pixels[98u * GP100_UI_LOGICAL_WIDTH] != theme.panel_rgb565 ||
        pixels[111u * GP100_UI_LOGICAL_WIDTH + 4u] !=
            theme.accent_rgb565) {
        return 2;
    }

    engine.state = GP100_LOOPER_RECORDING;
    engine.recorded_samples = 500u;
    if (!gp100_looper_screen_render(&canvas, &snapshot, &theme) ||
        strcmp(gp100_looper_screen_status(&snapshot), "Recording") !=
            0 ||
        pixels[24u * GP100_UI_LOGICAL_WIDTH + 8u] !=
            theme.record_rgb565 ||
        pixels[35u * GP100_UI_LOGICAL_WIDTH + 50u] !=
            theme.record_rgb565) {
        return 3;
    }
    snapshot.clear_feedback = true;
    if (strcmp(gp100_looper_screen_status(&snapshot), "Clean") != 0) {
        return 4;
    }
    snapshot.clear_feedback = false;
    engine.state = GP100_LOOPER_IDLE;
    engine.recorded_samples = 0u;
    if (!gp100_looper_screen_adjust(
            &engine, &settings, &preset_index,
            GP100_LOOPER_SCREEN_RECORD_LEVEL, true, 20u) ||
        engine.record_level_target != 99u ||
        !gp100_looper_screen_adjust(
            &engine, &settings, &preset_index,
            GP100_LOOPER_SCREEN_PLAYBACK_LEVEL, false, 90u) ||
        engine.playback_level_target != 0u ||
        !gp100_settings_validate(&settings) ||
        !gp100_looper_screen_adjust(
            &engine, &settings, &preset_index,
            GP100_LOOPER_SCREEN_PRESET, false, 7u) ||
        preset_index != 197u ||
        !gp100_looper_screen_adjust(
            &engine, &settings, &preset_index,
            GP100_LOOPER_SCREEN_PRESET, true, 7u) ||
        preset_index != 0u) {
        return 5;
    }
    if (!gp100_looper_screen_handle_footswitch(
            &engine, GP100_LOOPER_FOOTSWITCH_RECORD_PLAY,
            false, &changed, &clear_feedback) ||
        !changed || clear_feedback ||
        engine.state != GP100_LOOPER_RECORDING) {
        return 6;
    }
    engine.recorded_samples = 4u;
    if (!gp100_looper_screen_handle_footswitch(
            &engine, GP100_LOOPER_FOOTSWITCH_RECORD_PLAY,
            false, &changed, &clear_feedback) ||
        engine.state != GP100_LOOPER_PLAYING ||
        !gp100_looper_screen_handle_footswitch(
            &engine, GP100_LOOPER_FOOTSWITCH_RECORD_PLAY,
            false, &changed, &clear_feedback) ||
        engine.state != GP100_LOOPER_OVERDUBBING ||
        !gp100_looper_screen_handle_footswitch(
            &engine, GP100_LOOPER_FOOTSWITCH_STOP_CLEAR,
            false, &changed, &clear_feedback) ||
        !changed || engine.state != GP100_LOOPER_STOPPED ||
        !gp100_looper_screen_handle_footswitch(
            &engine, GP100_LOOPER_FOOTSWITCH_RECORD_PLAY,
            false, &changed, &clear_feedback) ||
        engine.state != GP100_LOOPER_PLAYING ||
        !gp100_looper_screen_handle_footswitch(
            &engine, GP100_LOOPER_FOOTSWITCH_STOP_CLEAR,
            true, &changed, &clear_feedback) ||
        !changed || !clear_feedback ||
        engine.state != GP100_LOOPER_IDLE ||
        engine.recorded_samples != 0u) {
        return 7;
    }
    engine.recorded_samples = 4u;
    engine.state = GP100_LOOPER_STOPPED;
    if (!gp100_looper_screen_adjust(
            &engine, &settings, &preset_index,
            GP100_LOOPER_SCREEN_POSITION, false, 1u) ||
        engine.position != GP100_LOOPER_POST_EFFECTS ||
        engine.recorded_samples != 0u ||
        engine.state != GP100_LOOPER_IDLE ||
        !gp100_settings_validate(&settings)) {
        return 8;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_3, true, 5u);
    if (gp100_looper_screen_handle_event(
            &engine, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_SETTINGS_CHANGED ||
        engine.playback_level_target != 5u) {
        return 9;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_PRESS);
    if (gp100_looper_screen_handle_event(
            &engine, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_ENGINE_ACTION ||
        engine.state != GP100_LOOPER_RECORDING) {
        return 10;
    }
    engine.recorded_samples = 4u;
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_PRESS);
    if (gp100_looper_screen_handle_event(
            &engine, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_ENGINE_ACTION ||
        engine.state != GP100_LOOPER_STOPPED) {
        return 11;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_HOLD);
    if (gp100_looper_screen_handle_event(
            &engine, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_ENGINE_ACTION ||
        !clear_feedback || engine.state != GP100_LOOPER_IDLE) {
        return 12;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_RELEASE);
    if (gp100_looper_screen_handle_event(
            &engine, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_DRUM_TOGGLED || !drum_enabled) {
        return 13;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_EXIT, GP100_BUTTON_PRESS);
    if (gp100_looper_screen_handle_event(
            &engine, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 14;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_GLOBAL, GP100_BUTTON_RELEASE);
    if (gp100_looper_screen_handle_event(
            &engine, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_GLOBAL) {
        return 14;
    }
    pair_release_guard = true;
    next_mode = GP100_UI_MODE_LOOPER;
    event.code = UINT32_C(0x40000000);
    if (gp100_looper_screen_handle_event(
            &engine, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_PAIR_RELEASE_GUARDED ||
        pair_release_guard || next_mode != GP100_UI_MODE_LOOPER ||
        gp100_looper_screen_handle_event(
            &engine, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 16;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_looper_screen_handle_event(
            &engine, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_UNHANDLED ||
        gp100_looper_screen_handle_event(
            NULL, &settings, &preset_index, &drum_enabled,
            &event, &pair_release_guard, &clear_feedback, &next_mode) !=
            GP100_LOOPER_EVENT_INVALID) {
        return 15;
    }
    snapshot.logical_preset_index = 198u;
    if (gp100_looper_screen_render(&canvas, &snapshot, &theme) ||
        gp100_looper_screen_default_theme(NULL, &empty_font, NULL) ||
        gp100_looper_screen_adjust(
            NULL, &settings, &preset_index,
            GP100_LOOPER_SCREEN_PRESET, true, 1u) ||
        gp100_looper_screen_handle_footswitch(
            NULL, GP100_LOOPER_FOOTSWITCH_RECORD_PLAY,
            false, NULL, NULL) ||
        gp100_looper_screen_handle_footswitch(
            &engine, GP100_LOOPER_FOOTSWITCH_RECORD_PLAY,
            true, NULL, NULL)) {
        return 16;
    }
    return 0;
}
