#include "protocol/midi/midi_runtime.h"

#include <string.h>

typedef struct fixture {
    int32_t looper_storage[64];
    gp100_ui_runtime_t ui;
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    gp100_looper_engine_t looper;
    gp100_effect_runtime_t effects;
    gp100_effect_chain_t chain;
    gp100_audio_output_level_t output;
    gp100_rhythm_settings_t rhythm;
    gp100_effect_catalog_t catalog;
    gp100_midi_runtime_t runtime;
    unsigned load_calls;
    unsigned sysex_calls;
    uint16_t loaded_index;
    bool sysex_data_path;
    bool load_failed;
} fixture_t;

static bool make_preset(
    gp100_preset_record_t *preset,
    const char *name,
    uint8_t level)
{
    uint16_t positions[GP100_EFFECT_FAMILY_COUNT];
    size_t family;

    memset(preset, 0, sizeof(*preset));
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        positions[family] = (uint16_t)family;
        if (!gp100_preset_select_algorithm(
                preset, (gp100_effect_family_t)family, 0u) ||
            !gp100_preset_set_effect_enabled(
                preset, (gp100_effect_family_t)family, true)) {
            return false;
        }
    }
    return gp100_preset_set_effect_positions(preset, positions) &&
           gp100_preset_set_name(preset, name) &&
           gp100_preset_set_tempo_bpm(preset, 120u) &&
           gp100_preset_set_level(preset, level);
}

static bool load_preset(
    void *opaque,
    uint16_t logical_preset,
    gp100_preset_record_t *preset)
{
    fixture_t *fixture = (fixture_t *)opaque;
    ++fixture->load_calls;
    fixture->loaded_index = logical_preset;
    return !fixture->load_failed && make_preset(preset, "MIDI recall", 73u);
}

static bool handle_sysex(
    void *opaque,
    bool data_path,
    const uint8_t *message,
    size_t message_length)
{
    fixture_t *fixture = (fixture_t *)opaque;
    (void)message;
    (void)message_length;
    ++fixture->sysex_calls;
    fixture->sysex_data_path = data_path;
    return true;
}

static bool initialize_fixture(fixture_t *fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    if (!make_preset(&fixture->preset, "Initial", 50u) ||
        !gp100_settings_set_i16(
            &fixture->settings,
            GP100_SETTINGS_MASTER_VOLUME, 50) ||
        !gp100_settings_set_i16(
            &fixture->settings, GP100_SETTINGS_DRUM_STYLE, 0) ||
        !gp100_settings_set_i16(
            &fixture->settings, GP100_SETTINGS_DRUM_TEMPO, 120) ||
        !gp100_settings_set_i16(
            &fixture->settings, GP100_SETTINGS_DRUM_VOLUME, 80) ||
        !gp100_settings_set_i16(
            &fixture->settings, GP100_SETTINGS_TAP_SYNC_MODULATION, 1) ||
        !gp100_settings_set_i16(
            &fixture->settings, GP100_SETTINGS_TAP_SYNC_DELAY, 1) ||
        !gp100_looper_init(
            &fixture->looper, fixture->looper_storage,
            sizeof(fixture->looper_storage) /
                sizeof(fixture->looper_storage[0]),
            GP100_LOOPER_PRE_EFFECTS, 80u, 80u)) {
        return false;
    }
    fixture->ui.mode = GP100_UI_MODE_PLAY;
    gp100_effect_runtime_reset(&fixture->effects);
    gp100_audio_output_level_reset(&fixture->output);
    fixture->catalog = gp100_effect_catalog_core();
    fixture->runtime.controls.ui = &fixture->ui;
    fixture->runtime.controls.preset = &fixture->preset;
    fixture->runtime.controls.settings = &fixture->settings;
    fixture->runtime.controls.looper = &fixture->looper;
    fixture->runtime.controls.effects = &fixture->effects;
    if (!gp100_rhythm_settings_from_record(
            &fixture->settings, false, &fixture->rhythm)) {
        return false;
    }
    fixture->runtime.controls.rhythm = &fixture->rhythm;
    fixture->runtime.catalog = &fixture->catalog;
    fixture->runtime.chain = &fixture->chain;
    fixture->runtime.output = &fixture->output;
    fixture->runtime.load_preset = load_preset;
    fixture->runtime.handle_sysex = handle_sysex;
    fixture->runtime.context = fixture;
    fixture->ui.tap_tempo_enabled = true;
    gp100_midi_runtime_reset(&fixture->runtime);
    return true;
}

int main(void)
{
    fixture_t fixture;
    gp100_midi_stream_t stream;
    gp100_midi_runtime_result_t result;
    gp100_midi_poll_result_t poll_result;
    uint8_t workspace[8];
    int16_t master;
    int16_t current_preset;
    char name[GP100_PRESET_NAME_SIZE + 1u];
    const uint8_t bank[] = {0xb0u, 0u, 1u};
    const uint8_t program[] = {0xc0u, 1u};
    const uint8_t control[] = {0xb0u, 7u, 75u};
    const uint8_t preset_step[] = {0xb0u, 24u, 0u};
    const uint8_t sysex[] = {
        0xf0u, 0x21u, 0x25u, 0u,
        (uint8_t)'G', (uint8_t)'P', (uint8_t)'-', (uint8_t)'d',
        0x11u, 0xf7u
    };

    if (!initialize_fixture(&fixture) ||
        !gp100_midi_runtime_handle_message(
            &fixture.runtime, bank, sizeof(bank), &result) ||
        result.action != GP100_MIDI_ACTION_BANK_SELECT ||
        !result.midi_activity_observed ||
        fixture.runtime.change_notifications_suppressed ||
        !gp100_midi_runtime_handle_message(
            &fixture.runtime, program, sizeof(program), &result) ||
        result.action != GP100_MIDI_ACTION_PRESET_RECALL ||
        !result.settings_changed ||
        fixture.load_calls != 1u || fixture.loaded_index != 100u ||
        fixture.ui.logical_preset != 100u ||
        !gp100_settings_get_i16(
            &fixture.settings, GP100_SETTINGS_CURRENT_PRESET,
            &current_preset) ||
        current_preset != 100 ||
        fixture.output.target_percent != 73u || fixture.ui.tap_tempo_enabled ||
        result.preset_recall
                .tap_tempo_sync_slots[GP100_EFFECT_MODULATION] !=
            UINT16_C(1) << 3 ||
        result.preset_recall
                .tap_tempo_sync_slots[GP100_EFFECT_DELAY] !=
            UINT16_C(1) << 3) {
        return 1;
    }
    gp100_preset_name(&fixture.preset, name);
    if (strcmp(name, "MIDI recall") != 0) return 1;
    if (!gp100_midi_runtime_handle_message(
            &fixture.runtime, control, sizeof(control), &result) ||
        result.action != GP100_MIDI_ACTION_CONTROL_CHANGE ||
        !result.settings_changed ||
        !result.control_change.master_volume_changed ||
        result.control_change.master_volume_percent != 75u ||
        !gp100_settings_get_i16(
            &fixture.settings, GP100_SETTINGS_MASTER_VOLUME,
            &master) || master != 75) {
        return 2;
    }
    if (!gp100_midi_runtime_handle_message(
            &fixture.runtime, preset_step, sizeof(preset_step), &result) ||
        result.action != GP100_MIDI_ACTION_CONTROL_CHANGE ||
        (result.control_change.ui_updates &
         GP100_UI_UPDATE_PRESET_LOAD) == 0u ||
        !result.settings_changed ||
        fixture.load_calls != 2u || fixture.loaded_index != 101u ||
        fixture.ui.logical_preset != 101u ||
        !gp100_settings_get_i16(
            &fixture.settings, GP100_SETTINGS_CURRENT_PRESET,
            &current_preset) ||
        current_preset != 101 ||
        fixture.output.target_percent != 73u) {
        return 3;
    }

    fixture.load_failed = true;
    if (gp100_midi_runtime_handle_message(
            &fixture.runtime, preset_step, sizeof(preset_step), &result) ||
        fixture.loaded_index != 102u || fixture.ui.logical_preset != 101u ||
        !gp100_settings_get_i16(
            &fixture.settings, GP100_SETTINGS_CURRENT_PRESET,
            &current_preset) || current_preset != 101) {
        return 7;
    }
    fixture.load_failed = false;
    fixture.runtime.input_blocked = true;
    if (!gp100_midi_runtime_handle_message(
            &fixture.runtime, control, sizeof(control), &result) ||
        result.action != GP100_MIDI_ACTION_BLOCKED ||
        result.midi_activity_observed ||
        fixture.runtime.change_notifications_suppressed) {
        return 4;
    }
    fixture.runtime.input_blocked = false;
    if (!gp100_midi_runtime_handle_message(
            &fixture.runtime, sysex, sizeof(sysex), &result) ||
        result.action != GP100_MIDI_ACTION_SYSEX_DATA ||
        fixture.sysex_calls != 1u || !fixture.sysex_data_path ||
        gp100_midi_runtime_handle_message(
            NULL, control, sizeof(control), &result)) {
        return 5;
    }
    gp100_midi_stream_reset(&stream);
    if (!gp100_midi_stream_push(&stream, bank, sizeof(bank)) ||
        !gp100_midi_runtime_poll_stream(
            &fixture.runtime, &stream, workspace, sizeof(workspace),
            &poll_result, &result) ||
        poll_result != GP100_MIDI_POLL_MESSAGE ||
        result.action != GP100_MIDI_ACTION_BANK_SELECT ||
        fixture.runtime.message.bank_select != 1u ||
        gp100_midi_stream_count(&stream) != 0u ||
        !gp100_midi_runtime_poll_stream(
            &fixture.runtime, &stream, workspace, sizeof(workspace),
            &poll_result, &result) ||
        poll_result != GP100_MIDI_POLL_IDLE ||
        result.action != GP100_MIDI_ACTION_IGNORED) {
        return 6;
    }
    return 0;
}
