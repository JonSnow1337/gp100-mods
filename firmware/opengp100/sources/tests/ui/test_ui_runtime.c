#include "ui/ui_runtime.h"

#include <string.h>

static bool initialize_settings(gp100_settings_record_t *settings)
{
    memset(settings, 0, sizeof(*settings));
    return gp100_settings_set_i16(
               settings, GP100_SETTINGS_MASTER_VOLUME, 50) &&
           gp100_settings_set_i16(
               settings, GP100_SETTINGS_FOOTSWITCH_MODE,
               GP100_FOOTSWITCH_MODE_PATCH) &&
           gp100_settings_set_i16(
               settings, GP100_SETTINGS_TUNER_MODE,
               GP100_TUNER_OUTPUT_THRU) &&
           gp100_settings_set_i16(
               settings, GP100_SETTINGS_TUNER_REFERENCE_HZ, 440) &&
           gp100_settings_set_i16(
               settings, GP100_SETTINGS_DRUM_GENRE, 0) &&
           gp100_settings_set_i16(
               settings, GP100_SETTINGS_DRUM_STYLE, 0) &&
           gp100_settings_set_i16(
               settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0) &&
           gp100_settings_set_i16(
               settings, GP100_SETTINGS_DRUM_TEMPO, 120) &&
           gp100_settings_set_i16(
               settings, GP100_SETTINGS_DRUM_VOLUME, 80);
}

int main(void)
{
    static int32_t looper_storage[1024];
    gp100_ui_runtime_t runtime;
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    gp100_looper_engine_t looper;
    gp100_event_t event = {0u, 0u};
    uint32_t updates;
    int16_t setting;
    bool drum_before;

    memset(&preset, 0, sizeof(preset));
    if (!gp100_preset_set_name(&preset, "Runtime") ||
        !gp100_preset_set_tempo_bpm(&preset, 120u) ||
        !gp100_preset_set_level(&preset, 50u) ||
        !initialize_settings(&settings) ||
        !gp100_looper_init(
            &looper, looper_storage,
            sizeof(looper_storage) / sizeof(looper_storage[0]),
            GP100_LOOPER_PRE_EFFECTS, 90u, 80u) ||
        !gp100_ui_runtime_init(
            &runtime, GP100_UI_MODE_PLAY, 0u, false, true,
            &preset, &settings)) {
        return 1;
    }

    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_1, true, 2u);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        updates != (GP100_UI_UPDATE_REDRAW |
                    GP100_UI_UPDATE_SETTINGS) ||
        !gp100_settings_get_i16(
            &settings, GP100_SETTINGS_MASTER_VOLUME, &setting) ||
        setting != 52) {
        return 2;
    }

    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_HOLD);
    runtime.tap_count = 7u;
    runtime.last_tap_tick = 400u;
    event.value = 500u;
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        !runtime.tap_tempo_enabled || runtime.tap_count != 0u ||
        runtime.last_tap_tick != 500u ||
        updates != (GP100_UI_UPDATE_REDRAW |
                    GP100_UI_UPDATE_TAP_TEMPO)) {
        return 17;
    }
    event.value = 700u;
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.tap_count != 0u || runtime.last_tap_tick != 500u ||
        updates != GP100_UI_UPDATE_NONE) {
        return 19;
    }
    event.value = 1000u;
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.tap_count != 1u || runtime.last_tap_tick != 1000u ||
        gp100_preset_tempo_bpm(&preset) != 120u ||
        updates != (GP100_UI_UPDATE_REDRAW |
                    GP100_UI_UPDATE_PRESET)) {
        return 19;
    }
    event.value = 1600u;
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        gp100_preset_tempo_bpm(&preset) != 100u ||
        updates != (GP100_UI_UPDATE_REDRAW |
                    GP100_UI_UPDATE_PRESET)) {
        return 20;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_HOLD);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        updates != (GP100_UI_UPDATE_REDRAW |
                    GP100_UI_UPDATE_SETTINGS) ||
        !gp100_settings_get_i16(
            &settings, GP100_SETTINGS_FOOTSWITCH_MODE, &setting) ||
        setting != GP100_FOOTSWITCH_MODE_STOMP) {
        return 18;
    }

    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_HOLD);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_DRUM ||
        !runtime.drum_release_guard ||
        (updates & GP100_UI_UPDATE_MODE) == 0u) {
        return 3;
    }
    drum_before = runtime.drum_enabled;
    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_DRUM ||
        runtime.drum_release_guard ||
        runtime.drum_enabled != drum_before ||
        updates != GP100_UI_UPDATE_REDRAW) {
        return 3;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_2, true, 1u);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        (updates & GP100_UI_UPDATE_SETTINGS) == 0u ||
        !gp100_settings_get_i16(
            &settings, GP100_SETTINGS_DRUM_TEMPO, &setting) ||
        setting != 121) {
        return 4;
    }
    if (!gp100_settings_set_i16(
            &settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            GP100_FOOTSWITCH_MODE_PATCH)) {
        return 21;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.logical_preset != 1u ||
        updates != (GP100_UI_UPDATE_REDRAW |
                    GP100_UI_UPDATE_PRESET_LOAD)) {
        return 22;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_REPEAT);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.logical_preset != 2u ||
        updates != (GP100_UI_UPDATE_REDRAW |
                    GP100_UI_UPDATE_PRESET_LOAD)) {
        return 23;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_PLAY) {
        return 5;
    }

    event.code = UINT32_C(0x40000000);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_TUNER ||
        runtime.tuner.reference_hz != 440u) {
        return 6;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_3, true, 1u);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.tuner.reference_hz != 441u ||
        (updates & GP100_UI_UPDATE_SETTINGS) == 0u) {
        return 7;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_PLAY) {
        return 8;
    }

    event.code = gp100_button_event_code(
        GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_SAVE ||
        strcmp(runtime.save.name, "Runtime") != 0) {
        return 9;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_1, true, 1u);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        updates != GP100_UI_UPDATE_REDRAW) {
        return 10;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_PLAY ||
        (updates & GP100_UI_UPDATE_SAVE_COMMIT) == 0u) {
        return 11;
    }

    event.code = gp100_button_event_code(
        GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_EDIT) {
        return 12;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_PARA, true, 1u);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.edit.selected_position != 1u ||
        updates != GP100_UI_UPDATE_REDRAW) {
        return 13;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_PLAY) {
        return 14;
    }
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_PLAY ||
        updates != GP100_UI_UPDATE_NONE) {
        return 14;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_GLOBAL, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_GLOBAL) {
        return 24;
    }
    drum_before = runtime.drum_enabled;
    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.drum_enabled == drum_before ||
        updates != (GP100_UI_UPDATE_REDRAW |
                    GP100_UI_UPDATE_DRUM)) {
        return 25;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_PLAY) {
        return 26;
    }

    event.code = UINT32_C(0x40030000);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_LOOPER ||
        !runtime.looper_pair_release_guard) {
        return 15;
    }
    event.code = UINT32_C(0x40000000);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_LOOPER ||
        runtime.looper_pair_release_guard) {
        return 27;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_PRESS);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        looper.state != GP100_LOOPER_RECORDING ||
        (updates & GP100_UI_UPDATE_LOOPER) == 0u ||
        gp100_ui_runtime_handle_event(
            NULL, &preset, &settings, &looper, &event, &updates)) {
        return 16;
    }
    if (!gp100_ui_runtime_set_mode(
            &runtime, GP100_UI_MODE_GLOBAL,
            &preset, &settings)) {
        return 32;
    }
    runtime.global.view = GP100_GLOBAL_VIEW_EXPRESSION;
    runtime.global.menu_item = GP100_GLOBAL_EXPRESSION_SETTING;
    runtime.global.expression_item = GP100_EXPRESSION_VOLUME_RANGE;
    if (!gp100_ui_runtime_set_mode(
            &runtime, GP100_UI_MODE_GLOBAL,
            &preset, &settings) ||
        runtime.global.view != GP100_GLOBAL_VIEW_EXPRESSION ||
        runtime.global.menu_item != GP100_GLOBAL_EXPRESSION_SETTING ||
        runtime.global.expression_item !=
            GP100_EXPRESSION_VOLUME_RANGE) {
        return 32;
    }
    if (!gp100_ui_runtime_set_mode(
            &runtime, GP100_UI_MODE_HARDWARE_TEST,
            &preset, &settings)) {
        return 17;
    }
    runtime.hardware_test.exit_ready = true;
    event.code = gp100_button_event_code(
        GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    if (!gp100_ui_runtime_handle_event(
            &runtime, &preset, &settings, &looper, &event, &updates) ||
        runtime.mode != GP100_UI_MODE_PLAY ||
        (updates & GP100_UI_UPDATE_MODE) == 0u) {
        return 18;
    }
    return 0;
}
