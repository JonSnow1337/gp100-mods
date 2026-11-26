#include "protocol/midi/midi_cc_runtime.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct fixture {
    int32_t looper_storage[256];
    gp100_ui_runtime_t ui;
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    gp100_looper_engine_t looper;
    gp100_effect_runtime_t effects;
    gp100_rhythm_settings_t rhythm;
    gp100_midi_cc_runtime_t runtime;
} fixture_t;

static bool set_setting(fixture_t *fixture,
                        gp100_settings_field_t field,
                        int16_t value)
{
    return gp100_settings_set_i16(&fixture->settings, field, value);
}

static bool initialize_fixture(fixture_t *fixture)
{
    gp100_control_target_t unassigned = {false, 0, 0u, 0u};
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT];
    size_t family;
    size_t target;

    memset(fixture, 0, sizeof(*fixture));
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        if (!gp100_preset_select_algorithm(
                &fixture->preset,
                (gp100_effect_family_t)family, 0u) ||
            !gp100_preset_set_effect_enabled(
                &fixture->preset,
                (gp100_effect_family_t)family, true)) {
            return false;
        }
    }
    for (target = 0u; target < GP100_CONTROL_TARGET_COUNT; ++target) {
        if (!gp100_preset_set_control_target(
                &fixture->preset, target, &unassigned) ||
            !gp100_preset_set_control_range(
                &fixture->preset,
                (gp100_control_range_t)(
                    GP100_CONTROL_RANGE_EXPRESSION_TARGET_1 + target),
                0.0f, 99.0f)) {
            return false;
        }
    }
    if (!gp100_preset_set_control_range(
            &fixture->preset, GP100_CONTROL_RANGE_VOLUME,
            10.0f, 90.0f) ||
        !gp100_preset_set_expression_pedal_target(
            &fixture->preset, GP100_EXPRESSION_PEDAL_VOLUME) ||
        !gp100_preset_set_volume_pedal_position(
            &fixture->preset, GP100_VOLUME_PEDAL_POST) ||
        !gp100_preset_set_tempo_bpm(&fixture->preset, 120u) ||
        !gp100_preset_set_level(&fixture->preset, 50u) ||
        !set_setting(fixture, GP100_SETTINGS_FOOTSWITCH_MODE,
                     GP100_FOOTSWITCH_MODE_PATCH) ||
        !set_setting(fixture, GP100_SETTINGS_TUNER_MODE,
                     GP100_TUNER_OUTPUT_THRU) ||
        !set_setting(fixture, GP100_SETTINGS_TUNER_REFERENCE_HZ, 440) ||
        !set_setting(fixture, GP100_SETTINGS_DRUM_GENRE, 0) ||
        !set_setting(fixture, GP100_SETTINGS_DRUM_STYLE, 0) ||
        !set_setting(fixture, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0) ||
        !set_setting(fixture, GP100_SETTINGS_DRUM_TEMPO, 120) ||
        !set_setting(fixture, GP100_SETTINGS_DRUM_VOLUME, 80) ||
        !set_setting(fixture, GP100_SETTINGS_LOOP_RECORD_VOLUME, 80) ||
        !set_setting(fixture, GP100_SETTINGS_LOOP_PLAYBACK_VOLUME, 80)) {
        return false;
    }
    gp100_preset_update_crc(&fixture->preset);
    gp100_settings_update_crc(&fixture->settings);
    gp100_effect_runtime_reset(&fixture->effects);
    if (!gp100_effect_runtime_apply_preset(
            &fixture->effects, &fixture->preset, actions) ||
        !gp100_looper_init(
            &fixture->looper, fixture->looper_storage,
            sizeof(fixture->looper_storage) /
                sizeof(fixture->looper_storage[0]),
            GP100_LOOPER_PRE_EFFECTS, 80u, 80u) ||
        !gp100_ui_runtime_init(
            &fixture->ui, GP100_UI_MODE_PLAY, 0u, false, true,
            &fixture->preset, &fixture->settings) ||
        !gp100_rhythm_settings_from_record(
            &fixture->settings, false, &fixture->rhythm)) {
        return false;
    }
    fixture->runtime.ui = &fixture->ui;
    fixture->runtime.preset = &fixture->preset;
    fixture->runtime.settings = &fixture->settings;
    fixture->runtime.looper = &fixture->looper;
    fixture->runtime.effects = &fixture->effects;
    fixture->runtime.rhythm = &fixture->rhythm;
    return true;
}

static bool field_is(const fixture_t *fixture,
                     gp100_settings_field_t field,
                     int16_t expected)
{
    int16_t actual;
    return gp100_settings_get_i16(
               &fixture->settings, field, &actual) &&
           actual == expected;
}

static int test_preset_effect_and_expression(void)
{
    fixture_t fixture;
    gp100_midi_cc_apply_result_t result;
    gp100_control_target_t target;
    uint32_t algorithm_id;
    float expected;

    if (!initialize_fixture(&fixture) ||
        !gp100_midi_cc_decode_and_apply(
            7u, 75u, &fixture.runtime, &result) ||
        !field_is(&fixture, GP100_SETTINGS_MASTER_VOLUME, 75) ||
        !result.master_volume_changed ||
        result.master_volume_percent != 75u ||
        result.ui_updates != (GP100_UI_UPDATE_SETTINGS |
                              GP100_UI_UPDATE_REDRAW) ||
        !gp100_midi_cc_decode_and_apply(
            11u, 127u, &fixture.runtime, &result)) {
        return 1;
    }
    expected = (10.0f + 80.0f * 253.0f / 255.0f) * 100.0f / 99.0f;
    if (!result.volume_pedal_changed ||
        result.volume_pedal_position != GP100_VOLUME_PEDAL_POST ||
        fabsf(result.volume_pedal_percent - expected) > 0.001f) {
        return 2;
    }

    if (!gp100_effect_algorithm_id(
            GP100_EFFECT_PRE_FX, 0u, &algorithm_id)) {
        return 3;
    }
    target.assigned = true;
    target.family = GP100_EFFECT_PRE_FX;
    target.algorithm_id = algorithm_id;
    target.parameter_index = 0u;
    if (!gp100_preset_set_control_target(
            &fixture.preset, 0u, &target) ||
        !gp100_preset_set_control_range(
            &fixture.preset,
            GP100_CONTROL_RANGE_EXPRESSION_TARGET_1,
            0.0f, 99.0f) ||
        !gp100_midi_cc_decode_and_apply(
            13u, 64u, &fixture.runtime, &result) ||
        !gp100_midi_cc_decode_and_apply(
            11u, 64u, &fixture.runtime, &result) ||
        result.volume_pedal_changed ||
        result.effect_changed_slots[GP100_EFFECT_PRE_FX] != 1u ||
        fabsf(fixture.effects.slots[GP100_EFFECT_PRE_FX]
                  .parameters[0] - (99.0f * 127.0f / 255.0f)) > 0.001f) {
        return 4;
    }
    if (!gp100_midi_cc_decode_and_apply(
            48u, 127u, &fixture.runtime, &result) ||
        fixture.effects.slots[GP100_EFFECT_PRE_FX].enabled ||
        result.effect_actions[GP100_EFFECT_PRE_FX] !=
            GP100_EFFECT_APPLY_DISABLE ||
        !gp100_midi_cc_decode_and_apply(
            24u, 0u, &fixture.runtime, &result) ||
        fixture.ui.logical_preset != 1u ||
        (result.ui_updates & GP100_UI_UPDATE_PRESET_LOAD) == 0u) {
        return 5;
    }

    if (!gp100_effect_algorithm_id(
            GP100_EFFECT_DELAY, 0u, &algorithm_id)) {
        return 6;
    }
    target.family = GP100_EFFECT_DELAY;
    target.algorithm_id = algorithm_id;
    target.parameter_index = 1u;
    if (!gp100_preset_set_control_target(
            &fixture.preset, 1u, &target) ||
        !gp100_preset_set_control_range(
            &fixture.preset,
            GP100_CONTROL_RANGE_EXPRESSION_TARGET_2,
            25.0f, 75.0f) ||
        !gp100_midi_cc_decode_and_apply(
            11u, 64u, &fixture.runtime, &result)) {
        return 7;
    }
    expected = (20.0f + 25.0f * (4000.0f - 20.0f) / 99.0f) +
        (50.0f * (4000.0f - 20.0f) / 99.0f) * 127.0f / 255.0f;
    if (result.effect_changed_slots[GP100_EFFECT_DELAY] !=
            (UINT16_C(1) << 1) ||
        fabsf(fixture.effects.slots[GP100_EFFECT_DELAY]
                  .parameters[1] - expected) > 0.01f) {
        return 8;
    }
    return 0;
}

static int test_modes_rhythm_and_looper(void)
{
    fixture_t fixture;
    gp100_midi_cc_apply_result_t result;

    if (!initialize_fixture(&fixture) ||
        !gp100_midi_cc_decode_and_apply(
            57u, 0u, &fixture.runtime, &result) ||
        fixture.ui.mode != GP100_UI_MODE_TUNER ||
        !gp100_midi_cc_decode_and_apply(
            59u, 0u, &fixture.runtime, &result) ||
        !fixture.ui.drum_enabled || !result.rhythm_changed ||
        !fixture.rhythm.enabled ||
        !gp100_midi_cc_decode_and_apply(
            60u, 99u, &fixture.runtime, &result) ||
        !field_is(&fixture, GP100_SETTINGS_DRUM_GENRE, 7) ||
        !field_is(&fixture, GP100_SETTINGS_DRUM_STYLE, 99) ||
        !field_is(&fixture, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 9) ||
        !result.rhythm_changed || fixture.rhythm.pattern_index != 99u ||
        !gp100_midi_cc_decode_and_apply(
            61u, 91u, &fixture.runtime, &result) ||
        !field_is(&fixture, GP100_SETTINGS_DRUM_VOLUME, 91) ||
        !result.rhythm_changed || fixture.rhythm.target_gain_q8 != 91) {
        return 1;
    }
    if (gp100_midi_cc_decode_and_apply(60u, 100u, &fixture.runtime, &result) ||
        !field_is(&fixture, GP100_SETTINGS_DRUM_STYLE, 99) ||
        fixture.rhythm.pattern_index != 99u) {
        return 4;
    }
    if (!gp100_midi_cc_decode_and_apply(
            63u, 0u, &fixture.runtime, &result) ||
        fixture.looper.state != GP100_LOOPER_RECORDING ||
        !gp100_midi_cc_decode_and_apply(
            66u, 100u, &fixture.runtime, &result) ||
        fixture.looper.record_level_target != 100u ||
        !field_is(
            &fixture, GP100_SETTINGS_LOOP_RECORD_VOLUME, 100) ||
        (fixture.looper.recorded_samples = 2u) == 0u ||
        !gp100_midi_cc_decode_and_apply(
            64u, 127u, &fixture.runtime, &result) ||
        fixture.looper.state != GP100_LOOPER_PLAYING) {
        return 2;
    }
    return 0;
}

static int test_system_and_tempos(void)
{
    fixture_t fixture;
    gp100_midi_cc_apply_result_t result;
    gp100_preset_record_t before;
    gp100_volume_pedal_position_t position;
    int16_t usb_source;

    if (!initialize_fixture(&fixture) ||
        !gp100_midi_cc_decode_and_apply(
            68u, 0u, &fixture.runtime, &result) ||
        !field_is(&fixture, GP100_SETTINGS_FOOTSWITCH_MODE,
                  GP100_FOOTSWITCH_MODE_STOMP) ||
        !gp100_settings_get_i16(
            &fixture.settings, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT, &usb_source) ||
        !gp100_midi_cc_decode_and_apply(
            69u, 63u, &fixture.runtime, &result) ||
        (result.ui_updates & GP100_UI_UPDATE_PRESET) == 0u ||
        !gp100_preset_volume_pedal_position(&fixture.preset, &position) ||
        position != GP100_VOLUME_PEDAL_PRE || fixture.preset.bytes[0x284] != 1u ||
        !gp100_midi_cc_decode_and_apply(
            69u, 64u, &fixture.runtime, &result) ||
        !gp100_preset_volume_pedal_position(&fixture.preset, &position) ||
        position != GP100_VOLUME_PEDAL_POST || fixture.preset.bytes[0x284] != 0u ||
        !field_is(&fixture, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT, usb_source) ||
        !gp100_midi_cc_decode_and_apply(
            70u, 127u, &fixture.runtime, &result) ||
        gp100_preset_tempo_bpm(&fixture.preset) != 250u ||
        fixture.looper.tempo_bpm != 250u ||
        !gp100_midi_cc_decode_and_apply(
            72u, 20u, &fixture.runtime, &result) ||
        !field_is(&fixture, GP100_SETTINGS_INPUT_LEVEL_DB, 20) ||
        !gp100_midi_cc_decode_and_apply(
            74u, 0u, &fixture.runtime, &result) ||
        !field_is(&fixture, GP100_SETTINGS_DRUM_TEMPO, 10) ||
        !result.rhythm_changed || fixture.rhythm.tempo_bpm != 10u) {
        return 1;
    }
    before = fixture.preset;
    if (gp100_midi_cc_decode_and_apply(
            7u, 101u, &fixture.runtime, &result) ||
        memcmp(&before, &fixture.preset, sizeof(before)) != 0 ||
        gp100_midi_cc_apply_action(NULL, &fixture.runtime, &result) ||
        gp100_midi_cc_apply_action(
            &(gp100_midi_cc_action_t){
                GP100_MIDI_CC_MASTER_VOLUME, 50, 0u},
            NULL, &result)) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result = test_preset_effect_and_expression();
    if (result != 0) {
        fprintf(stderr, "preset/effect CC runtime test failed: %d\n", result);
        return 10 + result;
    }
    result = test_modes_rhythm_and_looper();
    if (result != 0) {
        fprintf(stderr, "mode/rhythm CC runtime test failed: %d\n", result);
        return 20 + result;
    }
    result = test_system_and_tempos();
    if (result != 0) {
        fprintf(stderr, "system/tempo CC runtime test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
