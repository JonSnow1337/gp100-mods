#include "audio/effects/effect_catalog.h"
#include "app/preset_runtime.h"

#include <string.h>

static bool initialize_preset(gp100_preset_record_t *preset)
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
           gp100_preset_set_name(preset, "Recall") &&
           gp100_preset_set_tempo_bpm(preset, 120u) &&
           gp100_preset_set_level(preset, 73u);
}

typedef struct loader_fixture {
    gp100_preset_record_t source;
    uint16_t requested;
    unsigned calls;
    bool succeed;
} loader_fixture_t;

static bool load_preset(void *opaque,
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
    gp100_preset_record_t preset;
    gp100_preset_record_t invalid;
    gp100_settings_record_t settings = {0};
    gp100_ui_runtime_t ui = {0};
    gp100_effect_chain_t chain = {0};
    gp100_effect_runtime_t effects;
    gp100_audio_output_level_t output;
    gp100_preset_recall_result_t result;
    gp100_effect_catalog_t catalog = gp100_effect_catalog_core();
    loader_fixture_t loader;
    size_t family;

    if (!initialize_preset(&preset) ||
        !gp100_settings_set_i16(&settings, GP100_SETTINGS_TAP_SYNC_DELAY, 1)) {
        return 1;
    }
    gp100_effect_runtime_reset(&effects);
    gp100_audio_output_level_reset(&output);
    ui.mode = GP100_UI_MODE_PLAY;
    ui.tap_tempo_enabled = true;
    if (!gp100_preset_runtime_recall(
            123u, &catalog, &settings, &preset, &ui, &chain, &effects,
            &output, &result) ||
        ui.logical_preset != 123u || output.target_percent != 73u ||
        ui.tap_tempo_enabled ||
        effects.slots[GP100_EFFECT_DELAY].parameters[1] != 4.0f ||
        effects.slots[GP100_EFFECT_DELAY].parameters[3] != 1.0f ||
        effects.slots[GP100_EFFECT_MODULATION].parameters[3] != 0.0f ||
        gp100_preset_validate(&preset) != GP100_PRESET_VALID ||
        (result.repairs & GP100_PRESET_REPAIRED_SCHEMA) == 0u ||
        result.tap_tempo_sync_slots[GP100_EFFECT_PRE_FX] != 0u ||
        result.tap_tempo_sync_slots[GP100_EFFECT_MODULATION] != 0u ||
        result.tap_tempo_sync_slots[GP100_EFFECT_DELAY] !=
            UINT16_C(1) << 3 ||
        result.ui_updates != (GP100_UI_UPDATE_PRESET |
                              GP100_UI_UPDATE_REDRAW)) {
        return 2;
    }
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        if (chain.family_at_position[family] !=
                (gp100_effect_family_t)family ||
            !effects.slots[family].enabled ||
            !effects.slots[family].algorithm_loaded ||
            (result.effect_actions[family] &
             GP100_EFFECT_APPLY_LOAD_ALGORITHM) == 0u) {
            return 3;
        }
    }

    invalid = preset;
    invalid.bytes[0x20u] = 0xffu;
    invalid.bytes[0x21u] = 0xffu;
    invalid.bytes[0x22u] = 0xffu;
    invalid.bytes[0x23u] = 0xffu;
    if (!gp100_preset_runtime_recall(
            1u, &catalog, &settings, &invalid, &ui, &chain, &effects,
            &output, &result) ||
        (result.repairs & GP100_PRESET_REPAIRED_ALGORITHM) == 0u ||
        ui.logical_preset != 1u ||
        gp100_preset_runtime_recall(
            GP100_TOTAL_LOGICAL_PRESETS, &catalog, &settings, &invalid,
            &ui, &chain, &effects, &output, &result)) {
        return 4;
    }
    loader.source = preset;
    loader.calls = 0u;
    loader.succeed = true;
    if (!gp100_preset_set_name(&loader.source, "Loaded") ||
        !gp100_preset_runtime_load_and_recall(
            42u, load_preset, &loader, &catalog, &settings, &preset, &ui, &chain,
            &effects, &output, &result) ||
        loader.calls != 1u || loader.requested != 42u ||
        ui.logical_preset != 42u) {
        return 5;
    }
    loader.succeed = false;
    invalid = preset;
    if (gp100_preset_runtime_load_and_recall(
            43u, load_preset, &loader, &catalog, &settings, &preset, &ui, &chain,
            &effects, &output, &result) ||
        memcmp(&invalid, &preset, sizeof(preset)) != 0 ||
        ui.logical_preset != 42u) {
        return 6;
    }
    return 0;
}
