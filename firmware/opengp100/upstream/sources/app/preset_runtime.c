#include "preset_runtime.h"

#include "storage/patch_chain.h"

#include <string.h>

bool gp100_preset_runtime_recall(
    uint16_t logical_preset,
    const gp100_effect_catalog_t *catalog,
    const gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    gp100_ui_runtime_t *ui,
    gp100_effect_chain_t *chain,
    gp100_effect_runtime_t *effects,
    gp100_audio_output_level_t *output,
    gp100_preset_recall_result_t *result)
{
    gp100_preset_record_t next_preset;
    gp100_ui_runtime_t next_ui;
    gp100_effect_chain_t next_chain;
    gp100_effect_runtime_t next_effects;
    gp100_audio_output_level_t next_output;
    gp100_preset_recall_result_t next_result;

    if (logical_preset >= GP100_TOTAL_LOGICAL_PRESETS ||
        preset == NULL || ui == NULL || chain == NULL || effects == NULL ||
        output == NULL || result == NULL) {
        return false;
    }
    next_preset = *preset;
    next_ui = *ui;
    next_chain = *chain;
    next_effects = *effects;
    next_output = *output;
    memset(&next_result, 0, sizeof(next_result));

    if (gp100_patch_is_native(&next_preset)) {
        if (logical_preset >= GP100_PRESETS_PER_BANK ||
            !gp100_patch_validate(&next_preset)) {
            return false;
        }
        next_ui.patch_editor = (gp100_patch_editor_t){0};
    } else {
        next_result.repairs = gp100_preset_normalize(&next_preset, catalog);
    }
    if ((next_ui.tap_tempo_enabled && settings != NULL &&
         !gp100_effect_runtime_apply_tap_sync(
             &next_preset, settings, NULL, next_result.tap_tempo_sync_slots)) ||
        !gp100_effect_chain_from_preset(
            &next_chain, &next_preset) ||
        !gp100_effect_runtime_apply_preset(
            &next_effects, &next_preset, next_result.effect_actions) ||
        !gp100_audio_output_level_set_target(
            &next_output, (uint8_t)gp100_preset_level(&next_preset))) {
        return false;
    }
    gp100_preset_update_crc(&next_preset);
    next_ui.logical_preset = logical_preset;
    next_ui.tap_tempo_enabled = false;
    next_result.ui_updates = GP100_UI_UPDATE_PRESET |
                             GP100_UI_UPDATE_REDRAW;

    *preset = next_preset;
    *ui = next_ui;
    *chain = next_chain;
    *effects = next_effects;
    *output = next_output;
    *result = next_result;
    return true;
}

bool gp100_preset_runtime_load_and_recall(
    uint16_t logical_preset,
    gp100_preset_loader_t loader,
    void *loader_context,
    const gp100_effect_catalog_t *catalog,
    const gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    gp100_ui_runtime_t *ui,
    gp100_effect_chain_t *chain,
    gp100_effect_runtime_t *effects,
    gp100_audio_output_level_t *output,
    gp100_preset_recall_result_t *result)
{
    gp100_preset_record_t candidate;

    if (logical_preset >= GP100_TOTAL_LOGICAL_PRESETS ||
        loader == NULL || preset == NULL || ui == NULL || chain == NULL ||
        effects == NULL || output == NULL || result == NULL ||
        !loader(loader_context, logical_preset, &candidate) ||
        !gp100_preset_runtime_recall(
            logical_preset, catalog, settings, &candidate, ui, chain, effects,
            output, result)) {
        return false;
    }
    *preset = candidate;
    return true;
}
