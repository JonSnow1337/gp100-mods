#include "app/firmware_runtime.h"

#include <string.h>

typedef struct loader_state {
    gp100_preset_record_t preset;
    uint16_t requested;
    unsigned calls;
} loader_state_t;

static const uint8_t expected_factory_settings_payload[
    GP100_SETTINGS_PAYLOAD_SIZE] = {
    [0x0a] = 0x78,
    [0x0c] = 0x32,
    [0x0e] = 0xb8, [0x0f] = 0x01,
    [0x1a] = 0x63,
    [0x1c] = 0x63,
    [0x1e] = 0x32,
    [0x24] = 0x01,
    [0x26] = 0x01,
    [0x2c] = 0x12,
    [0x4a] = 0x98, [0x4b] = 0x41,
    [0x4d] = 0x42, [0x4e] = 0x9c, [0x4f] = 0x46,
    [0x52] = 0xc8, [0x53] = 0x42,
    [0x54] = 0x8f, [0x55] = 0xc2, [0x56] = 0x35, [0x57] = 0x3f,
    [0x5e] = 0xfa, [0x5f] = 0x43,
    [0x60] = 0x8f, [0x61] = 0xc2, [0x62] = 0x35, [0x63] = 0x3f,
    [0x6a] = 0x7a, [0x6b] = 0x44,
    [0x6c] = 0x8f, [0x6d] = 0xc2, [0x6e] = 0x35, [0x6f] = 0x3f,
    [0x75] = 0x40, [0x76] = 0x9c, [0x77] = 0x45,
    [0x78] = 0x8f, [0x79] = 0xc2, [0x7a] = 0x35, [0x7b] = 0x3f
};

static bool load_preset(void *context,
                        uint16_t logical_preset,
                        gp100_preset_record_t *preset)
{
    loader_state_t *state = context;

    ++state->calls;
    state->requested = logical_preset;
    *preset = state->preset;
    return true;
}

int main(void)
{
    static int32_t looper_storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    gp100_settings_record_t factory_settings;
    gp100_firmware_runtime_result_t result;
    gp100_event_t event;
    loader_state_t loader;
    gp100_play_screen_snapshot_t play;
    char name[GP100_PRESET_NAME_SIZE + 1u];
    uint32_t other_address;
    int16_t tuner_mode;
    int16_t current_preset;

    memset(&loader, 0, sizeof(loader));
    if (gp100_make_factory_settings(NULL) ||
        !gp100_make_factory_settings(&factory_settings) ||
        !gp100_settings_validate(&factory_settings) ||
        memcmp(factory_settings.bytes, expected_factory_settings_payload,
               GP100_SETTINGS_PAYLOAD_SIZE) != 0) {
        return 10;
    }
    if (!gp100_make_development_preset(&preset, "Development") ||
        !gp100_make_development_settings(&settings) ||
        gp100_preset_validate(&preset) != GP100_PRESET_VALID ||
        !gp100_settings_validate(&settings) ||
        !gp100_firmware_runtime_init(
            &runtime, 0u, &preset, &settings, looper_storage,
            sizeof(looper_storage) / sizeof(looper_storage[0]),
            10u, 4090u)) {
        return 1;
    }
    if (settings.bytes[0x0cu] != 80u ||
        settings.bytes[0x1au] != 80u ||
        settings.bytes[0x1cu] != 80u ||
        settings.bytes[0x24u] != 0u ||
        settings.bytes[0x26u] != 0u ||
        settings.bytes[0x2cu] !=
            (uint8_t)GP100_SETTINGS_V210_DATA_REVISION) {
        return 11;
    }
    gp100_preset_name(&runtime.preset, name);
    if (strcmp(name, "Development") != 0 ||
        runtime.ui.mode != GP100_UI_MODE_PLAY ||
        runtime.ui.logical_preset != 0u ||
        runtime.output.target_percent != 50u ||
        runtime.volume_pedal_percent != 100u ||
        runtime.volume_pedal_position != GP100_VOLUME_PEDAL_POST ||
        runtime.looper.storage != looper_storage ||
        !gp100_settings_get_i16(
            &runtime.settings, GP100_SETTINGS_TUNER_MODE,
            &tuner_mode) ||
        tuner_mode != GP100_TUNER_OUTPUT_MUTE ||
        gp100_preset_validate(&runtime.preset) !=
            GP100_PRESET_VALID ||
        runtime.preset_modified || runtime.settings_dirty) {
        return 2;
    }

    loader.preset = preset;
    event.value = 0u;
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_3, true, 1u);
    if (!gp100_firmware_runtime_handle_event(
            &runtime, &event, load_preset, &loader, &result) ||
        !runtime.preset_modified || !result.preset_became_modified ||
        gp100_preset_level(&runtime.preset) != 51u ||
        runtime.output.target_percent != 51u) {
        return 3;
    }
    event.code = gp100_button_event_code(GP100_BUTTON_SAVE,
                                        GP100_BUTTON_RELEASE);
    if (!gp100_firmware_runtime_handle_event(
            &runtime, &event, load_preset, &loader, &result) ||
        runtime.ui.mode != GP100_UI_MODE_SAVE) {
        return 4;
    }
    memcpy(runtime.ui.save.name, "BASS", sizeof("BASS"));
    if (!gp100_firmware_runtime_handle_event(
            &runtime, &event, load_preset, &loader, &result) ||
        !result.live.save_record_ready || runtime.ui.mode != GP100_UI_MODE_PLAY ||
        !gp100_preset_flash_address(1u, &other_address)) {
        return 12;
    }
    gp100_firmware_runtime_mark_preset_saved(
        &runtime, other_address, &result.live.save_record);
    gp100_preset_name(&runtime.preset, name);
    if (strcmp(name, "Development") != 0 || !runtime.preset_modified) {
        return 13;
    }
    gp100_firmware_runtime_mark_preset_saved(
        &runtime, result.live.save_flash_address, &result.live.save_record);
    if (!gp100_play_screen_snapshot_from_records(
            &play, runtime.ui.logical_preset, &runtime.preset,
            &runtime.settings, runtime.preset_modified, runtime.ui.drum_enabled) ||
        strcmp(play.preset_name, "BASS") != 0 || runtime.preset_modified ||
        gp100_preset_validate(&runtime.preset) != GP100_PRESET_VALID ||
        gp100_preset_level(&runtime.preset) != 51u || loader.calls != 0u) {
        return 14;
    }
    if (!gp100_firmware_runtime_handle_event(
            &runtime, &event, load_preset, &loader, &result) ||
        runtime.ui.mode != GP100_UI_MODE_SAVE ||
        strcmp(runtime.ui.save.name, "BASS") != 0) {
        return 15;
    }
    event.code = gp100_button_event_code(GP100_BUTTON_EXIT,
                                        GP100_BUTTON_RELEASE);
    if (!gp100_firmware_runtime_handle_event(
            &runtime, &event, load_preset, &loader, &result) ||
        runtime.ui.mode != GP100_UI_MODE_PLAY) {
        return 16;
    }

    event.value = 0u;
    event.code = GP100_EXPRESSION_POSITION_EVENT_PREFIX | 2050u;
    if (!gp100_firmware_runtime_handle_event(
            &runtime, &event, load_preset, &loader, &result) ||
        result.live.expression_status !=
            GP100_EXPRESSION_EVENT_POSITION ||
        runtime.volume_pedal_percent != 49u ||
        runtime.volume_pedal_position != GP100_VOLUME_PEDAL_POST) {
        return 8;
    }

    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_1, true, 1u);
    if (!gp100_firmware_runtime_handle_event(
            &runtime, &event, load_preset, &loader, &result) ||
        !runtime.settings_dirty || !result.settings_changed ||
        !result.settings_became_dirty) {
        return 5;
    }
    if (!gp100_firmware_runtime_handle_event(
            &runtime, &event, load_preset, &loader, &result) ||
        !runtime.settings_dirty || !result.settings_changed ||
        result.settings_became_dirty) {
        return 9;
    }
    gp100_firmware_runtime_mark_settings_saved(&runtime);
    if (runtime.settings_dirty) {
        return 6;
    }

    event.code = gp100_encoder_event_code(
        GP100_ENCODER_PARA, true, 1u);
    if (!gp100_firmware_runtime_handle_event(
            &runtime, &event, load_preset, &loader, &result) ||
        loader.calls != 1u || loader.requested != 1u ||
        !result.live.preset_loaded || runtime.ui.logical_preset != 1u ||
        !result.settings_changed || !result.settings_became_dirty ||
        !runtime.settings_dirty ||
        !gp100_settings_get_i16(
            &runtime.settings, GP100_SETTINGS_CURRENT_PRESET,
            &current_preset) ||
        current_preset != 1 || runtime.preset_modified) {
        return 7;
    }
    return 0;
}
