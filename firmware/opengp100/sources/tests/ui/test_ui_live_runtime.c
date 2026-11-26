#include "ui/ui_live_runtime.h"

#include <string.h>

typedef struct loader_fixture {
    gp100_preset_record_t source;
    uint16_t requested;
    unsigned calls;
    bool succeed;
} loader_fixture_t;

static bool load_preset(
    void *opaque,
    uint16_t logical_preset,
    gp100_preset_record_t *preset)
{
    loader_fixture_t *loader = (loader_fixture_t *)opaque;

    ++loader->calls;
    loader->requested = logical_preset;
    if (!loader->succeed) {
        return false;
    }
    *preset = loader->source;
    return true;
}

int main(void)
{
    static int32_t looper_storage[64];
    gp100_ui_runtime_t ui;
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    gp100_looper_engine_t looper;
    gp100_effect_chain_t chain;
    gp100_effect_runtime_t effects;
    gp100_audio_output_level_t output;
    gp100_rhythm_settings_t rhythm;
    gp100_ui_live_result_t result;
    gp100_effect_catalog_t catalog;
    loader_fixture_t loader;
    gp100_event_t event;
    char saved_name[GP100_PRESET_NAME_SIZE + 1u];
    uint16_t positions[GP100_EFFECT_FAMILY_COUNT];
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT];
    float delay_time;
    size_t family;

    memset(&preset, 0, sizeof(preset));
    memset(&settings, 0, sizeof(settings));
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        positions[family] = (uint16_t)family;
        if (!gp100_preset_select_algorithm(
                &preset, (gp100_effect_family_t)family, 0u) ||
            !gp100_preset_set_effect_enabled(
                &preset, (gp100_effect_family_t)family, true)) {
            return 1;
        }
    }
    if (!gp100_preset_set_effect_positions(&preset, positions) ||
        !gp100_preset_set_name(&preset, "Live") ||
        !gp100_preset_set_level(&preset, 50u) ||
        !gp100_preset_set_tempo_bpm(&preset, 120u) ||
        !gp100_preset_set_control_range(
            &preset, GP100_CONTROL_RANGE_VOLUME, 0.0f, 99.0f) ||
        !gp100_preset_set_expression_pedal_target(
            &preset, GP100_EXPRESSION_PEDAL_VOLUME) ||
        !gp100_preset_set_volume_pedal_position(
            &preset, GP100_VOLUME_PEDAL_POST) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_MASTER_VOLUME, 50) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            GP100_FOOTSWITCH_MODE_PATCH) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_STYLE, 12) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_TEMPO, 120) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_VOLUME, 80) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_TAP_SYNC_MODULATION, 1) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_TAP_SYNC_DELAY, 1) ||
        !gp100_looper_init(
            &looper, looper_storage,
            sizeof(looper_storage) / sizeof(looper_storage[0]),
            GP100_LOOPER_PRE_EFFECTS, 80u, 80u)) {
        return 2;
    }
    gp100_effect_runtime_reset(&effects);
    gp100_audio_output_level_reset(&output);
    catalog = gp100_effect_catalog_core();
    if (!gp100_effect_chain_from_preset(&chain, &preset) ||
        !gp100_effect_runtime_apply_preset(
            &effects, &preset, actions) ||
        !gp100_audio_output_level_set_target(&output, 50u) ||
        !gp100_rhythm_settings_from_record(
            &settings, false, &rhythm) ||
        !gp100_ui_runtime_init(
            &ui, GP100_UI_MODE_PLAY, 0u, false, true,
            &preset, &settings)) {
        return 3;
    }

    event.value = 0u;
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_3, true, 1u);
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        gp100_preset_level(&preset) != 51u ||
        !result.output_level_changed || output.target_percent != 51u ||
        (result.ui_updates & GP100_UI_UPDATE_PRESET) == 0u) {
        return 4;
    }

    event.code = GP100_EXPRESSION_POSITION_EVENT_PREFIX | 111u;
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        result.expression_status != GP100_EXPRESSION_EVENT_POSITION ||
        !result.expression.apply.volume_changed ||
        result.ui_updates != GP100_UI_UPDATE_NONE) {
        return 5;
    }
    event.value = 1u;
    event.code = GP100_EXPRESSION_ACTIVATE_EVENT_PREFIX | 220u;
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        result.expression_status !=
            GP100_EXPRESSION_EVENT_TARGET_TOGGLED ||
        result.ui_updates != (GP100_UI_UPDATE_PRESET |
                              GP100_UI_UPDATE_REDRAW)) {
        return 6;
    }

    event.value = 0u;
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_HOLD);
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        !ui.tap_tempo_enabled || !result.tap_tempo_changed ||
        result.tap_tempo_sync_slots[GP100_EFFECT_PRE_FX] != 0u ||
        result.tap_tempo_sync_slots[GP100_EFFECT_MODULATION] !=
            UINT16_C(1) << 3 ||
        result.tap_tempo_sync_slots[GP100_EFFECT_DELAY] !=
            UINT16_C(1) << 3 ||
        (result.ui_updates & GP100_UI_UPDATE_PRESET) == 0u ||
        gp100_preset_validate(&preset) != GP100_PRESET_VALID ||
        !gp100_preset_parameter(&preset, GP100_EFFECT_DELAY, 1u, &delay_time) ||
        delay_time != 4.0f ||
        effects.slots[GP100_EFFECT_DELAY].parameters[1] != 4.0f ||
        effects.slots[GP100_EFFECT_DELAY].parameters[3] != 1.0f) {
        return 19;
    }
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        ui.tap_tempo_enabled || !result.tap_tempo_changed ||
        result.tap_tempo_sync_slots[GP100_EFFECT_MODULATION] != 0u ||
        result.tap_tempo_sync_slots[GP100_EFFECT_DELAY] != 0u ||
        effects.slots[GP100_EFFECT_DELAY].parameters[3] != 1.0f) {
        return 20;
    }

    if (!gp100_ui_runtime_set_mode(
            &ui, GP100_UI_MODE_EDIT, &preset, &settings)) {
        return 7;
    }
    ui.tap_tempo_enabled = true;
    event.value = 0u;
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_1, false, 1u);
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        effects.slots[GP100_EFFECT_PRE_FX].enabled || !ui.tap_tempo_enabled ||
        result.effect_actions[GP100_EFFECT_PRE_FX] !=
            GP100_EFFECT_APPLY_DISABLE) {
        return 8;
    }
    const uint16_t edit_algorithm = effects.slots[GP100_EFFECT_PRE_FX].algorithm_index;
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_2, true, 1u);
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        !ui.tap_tempo_enabled || result.tap_tempo_changed ||
        effects.slots[GP100_EFFECT_PRE_FX].algorithm_index != edit_algorithm) {
        return 25;
    }
    event.code = gp100_button_event_code(GP100_BUTTON_PARA, GP100_BUTTON_HOLD);
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        !ui.edit.manage_signal_chain) {
        return 25;
    }
    event.code = gp100_button_event_code(GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result)) {
        return 25;
    }
    event.code = gp100_encoder_event_code(GP100_ENCODER_QUICK_2, true, 1u);
    uint32_t changed_algorithm_id;
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        ui.tap_tempo_enabled || !result.tap_tempo_changed ||
        !gp100_preset_algorithm_id(&preset, GP100_EFFECT_PRE_FX, &changed_algorithm_id) ||
        gp100_effect_algorithm_index(GP100_EFFECT_PRE_FX, changed_algorithm_id) !=
            (int)edit_algorithm + 1) {
        return 25;
    }

    if (!gp100_ui_runtime_set_mode(
            &ui, GP100_UI_MODE_DRUM, &preset, &settings)) {
        return 9;
    }
    event.code = GP100_EXPRESSION_POSITION_EVENT_PREFIX | 111u;
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        result.expression_status != GP100_EXPRESSION_EVENT_POSITION) {
        return 21;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_2, true, 1u);
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        !result.rhythm_changed || rhythm.tempo_bpm != 121u ||
        rhythm.enabled ||
        (result.ui_updates & GP100_UI_UPDATE_SETTINGS) == 0u) {
        return 10;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        !result.rhythm_changed || !rhythm.enabled ||
        (result.ui_updates & GP100_UI_UPDATE_DRUM) == 0u) {
        return 11;
    }

    if (!gp100_ui_runtime_set_mode(
            &ui, GP100_UI_MODE_GLOBAL, &preset, &settings)) {
        return 22;
    }
    ui.global.view = GP100_GLOBAL_VIEW_EXPRESSION_CALIBRATION;
    gp100_expression_calibration_init(
        &ui.global.expression_calibration);
    event.code = GP100_EXPRESSION_POSITION_EVENT_PREFIX | 123u;
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        result.expression_status != GP100_EXPRESSION_EVENT_UNHANDLED ||
        ui.global.calibration_raw != 123u ||
        (result.ui_updates & GP100_UI_UPDATE_REDRAW) == 0u) {
        return 23;
    }
    ui.global.view = GP100_GLOBAL_VIEW_MENU;
    event.code = GP100_EXPRESSION_POSITION_EVENT_PREFIX | 111u;
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        result.expression_status != GP100_EXPRESSION_EVENT_POSITION) {
        return 24;
    }

    if (!gp100_ui_runtime_set_mode(
            &ui, GP100_UI_MODE_SAVE, &preset, &settings)) {
        return 12;
    }
    memcpy(ui.save.name, "Live Copy  ", 11u);
    ui.save.name[11] = '\0';
    ui.save.destination = 4u;
    event.code = gp100_button_event_code(
        GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    if (!gp100_ui_live_handle_event(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, &result) ||
        !result.save_record_ready ||
        result.save_flash_address !=
            GP100_USER_PRESET_BASE +
                4u * GP100_PRESET_RECORD_SIZE ||
        gp100_preset_validate(&result.save_record) !=
            GP100_PRESET_VALID) {
        return 13;
    }
    gp100_preset_name(&result.save_record, saved_name);
    if (strcmp(saved_name, "Live Copy") != 0 ||
        ui.mode != GP100_UI_MODE_PLAY ||
        (result.ui_updates & GP100_UI_UPDATE_SAVE_COMMIT) == 0u) {
        return 14;
    }
    loader.source = preset;
    loader.calls = 0u;
    loader.requested = 0u;
    loader.succeed = true;
    if (!gp100_preset_set_name(&loader.source, "Panel") ||
        !gp100_preset_set_level(&loader.source, 73u)) {
        return 15;
    }
    ui.logical_preset = 0u;
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_RELEASE);
    if (!gp100_ui_live_handle_event_and_recall(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, load_preset,
            &loader, &catalog, &result) ||
        loader.calls != 1u || loader.requested != 1u ||
        ui.logical_preset != 1u || !result.preset_loaded ||
        (result.ui_updates & GP100_UI_UPDATE_PRESET_LOAD) != 0u ||
        (result.ui_updates & GP100_UI_UPDATE_PRESET) == 0u ||
        !result.output_level_changed || output.target_percent != 73u) {
        return 16;
    }
    gp100_preset_name(&preset, saved_name);
    if (strcmp(saved_name, "Panel") != 0) {
        return 17;
    }
    loader.succeed = false;
    loader.source = preset;
    if (gp100_ui_live_handle_event_and_recall(
            &ui, &preset, &settings, &looper, &chain, &effects,
            &output, &rhythm, 10u, 210u, &event, load_preset,
            &loader, &catalog, &result) ||
        loader.calls != 2u || loader.requested != 2u ||
        ui.logical_preset != 1u ||
        memcmp(&preset, &loader.source, sizeof(preset)) != 0) {
        return 18;
    }
    return 0;
}
