#include "ui_live_runtime.h"

#include "storage/patch_chain.h"

#include <string.h>

static bool effect_state_changed(
    const gp100_preset_record_t *before,
    const gp100_preset_record_t *after)
{
    size_t family;

    if (gp100_patch_is_native(after)) {
        return memcmp(before, after, sizeof(*after)) != 0;
    }
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        uint32_t before_algorithm;
        uint32_t after_algorithm;
        bool before_enabled;
        bool after_enabled;
        size_t slot;

        if (!gp100_preset_algorithm_id(
                before, (gp100_effect_family_t)family,
                &before_algorithm) ||
            !gp100_preset_algorithm_id(
                after, (gp100_effect_family_t)family,
                &after_algorithm) ||
            !gp100_preset_effect_enabled(
                before, (gp100_effect_family_t)family,
                &before_enabled) ||
            !gp100_preset_effect_enabled(
                after, (gp100_effect_family_t)family,
                &after_enabled)) {
            return true;
        }
        if (before_algorithm != after_algorithm ||
            before_enabled != after_enabled) {
            return true;
        }
        for (slot = 0u; slot < GP100_PARAMETERS_PER_FAMILY; ++slot) {
            float before_value;
            float after_value;

            if (!gp100_preset_parameter(
                    before, (gp100_effect_family_t)family,
                    slot, &before_value) ||
                !gp100_preset_parameter(
                    after, (gp100_effect_family_t)family,
                    slot, &after_value) ||
                memcmp(&before_value, &after_value,
                       sizeof(before_value)) != 0) {
                return true;
            }
        }
    }
    return gp100_preset_tempo_bpm(before) !=
           gp100_preset_tempo_bpm(after);
}

bool gp100_ui_live_handle_event(
    gp100_ui_runtime_t *ui,
    gp100_preset_record_t *preset,
    gp100_settings_record_t *settings,
    gp100_looper_engine_t *looper,
    gp100_effect_chain_t *chain,
    gp100_effect_runtime_t *effects,
    gp100_audio_output_level_t *output,
    gp100_rhythm_settings_t *rhythm,
    uint16_t calibration_heel,
    uint16_t calibration_toe,
    const gp100_event_t *event,
    gp100_ui_live_result_t *result)
{
    gp100_ui_live_result_t next;
    gp100_preset_record_t before_preset;
    gp100_effect_chain_t next_chain;
    gp100_effect_runtime_t next_effects;
    gp100_audio_output_level_t next_output;
    gp100_rhythm_settings_t next_rhythm;
    uint8_t before_level;
    uint8_t after_level;

    if (ui == NULL || preset == NULL || settings == NULL || looper == NULL ||
        chain == NULL || effects == NULL || output == NULL || rhythm == NULL ||
        event == NULL || result == NULL) {
        return false;
    }
    memset(&next, 0, sizeof(next));
    next.expression_status =
        ui->mode == GP100_UI_MODE_GLOBAL &&
        ui->global.view == GP100_GLOBAL_VIEW_EXPRESSION_CALIBRATION
            ? GP100_EXPRESSION_EVENT_UNHANDLED
            : gp100_expression_event_handle(
                  ui->mode, preset, effects,
                  calibration_heel, calibration_toe,
                  event, &next.expression);
    if (next.expression_status == GP100_EXPRESSION_EVENT_INVALID) {
        return false;
    }
    if (next.expression_status ==
            GP100_EXPRESSION_EVENT_TARGET_TOGGLED) {
        next.ui_updates = GP100_UI_UPDATE_PRESET |
                          GP100_UI_UPDATE_REDRAW;
        *result = next;
        return true;
    }
    if (next.expression_status == GP100_EXPRESSION_EVENT_POSITION) {
        *result = next;
        return true;
    }

    before_preset = *preset;
    next_chain = *chain;
    next_effects = *effects;
    next_output = *output;
    next_rhythm = *rhythm;
    if (!gp100_ui_runtime_handle_event(
            ui, preset, settings, looper, event, &next.ui_updates)) {
        return false;
    }
    if ((next.ui_updates & GP100_UI_UPDATE_PRESET) != 0u) {
        gp100_effect_chain_t rebuilt_chain;

        if (!gp100_effect_chain_from_preset(&rebuilt_chain, preset)) {
            return false;
        }
        next.chain_changed =
            memcmp(chain, &rebuilt_chain, sizeof(*chain)) != 0;
        next_chain = rebuilt_chain;
        if (effect_state_changed(&before_preset, preset) &&
            !gp100_effect_runtime_sync_preset(
                &next_effects, preset, next.effect_actions)) {
            return false;
        }
        before_level = gp100_preset_level(&before_preset);
        after_level = gp100_preset_level(preset);
        if (before_level != after_level) {
            if (!gp100_audio_output_level_set_target(
                    &next_output, after_level)) {
                return false;
            }
            next.output_level_changed = true;
        }
        *chain = next_chain;
        *effects = next_effects;
        *output = next_output;
        if (ui->tap_tempo_enabled && !gp100_patch_is_native(preset) &&
            gp100_effect_runtime_tap_sync_algorithm_changed(&before_preset, preset)) {
            ui->tap_tempo_enabled = false;
            next.ui_updates |= GP100_UI_UPDATE_TAP_TEMPO;
        }
    }
    if ((next.ui_updates & (GP100_UI_UPDATE_SETTINGS |
                            GP100_UI_UPDATE_DRUM)) != 0u) {
        gp100_rhythm_settings_t rebuilt_rhythm;

        if (!gp100_rhythm_settings_from_record(
                settings, ui->drum_enabled, &rebuilt_rhythm)) {
            return false;
        }
        next.rhythm_changed =
            memcmp(rhythm, &rebuilt_rhythm, sizeof(*rhythm)) != 0;
        next_rhythm = rebuilt_rhythm;
        *rhythm = next_rhythm;
    }
    if ((next.ui_updates & GP100_UI_UPDATE_TAP_TEMPO) != 0u) {
        if (ui->tap_tempo_enabled &&
            !gp100_effect_runtime_apply_tap_sync(
                preset, settings, effects, next.tap_tempo_sync_slots)) {
            return false;
        }
        if (memcmp(preset, &before_preset, sizeof(*preset)) != 0) {
            gp100_preset_update_crc(preset);
            next.ui_updates |= GP100_UI_UPDATE_PRESET;
        }
        next.tap_tempo_changed = true;
    }
    if ((next.ui_updates & GP100_UI_UPDATE_SAVE_COMMIT) != 0u) {
        if (!gp100_save_screen_prepare_record(
                &ui->save, preset, &next.save_record,
                &next.save_flash_address)) {
            return false;
        }
        next.save_record_ready = true;
    }
    *result = next;
    return true;
}

bool gp100_ui_live_handle_event_and_recall(
    gp100_ui_runtime_t *ui,
    gp100_preset_record_t *preset,
    gp100_settings_record_t *settings,
    gp100_looper_engine_t *looper,
    gp100_effect_chain_t *chain,
    gp100_effect_runtime_t *effects,
    gp100_audio_output_level_t *output,
    gp100_rhythm_settings_t *rhythm,
    uint16_t calibration_heel,
    uint16_t calibration_toe,
    const gp100_event_t *event,
    gp100_preset_loader_t loader,
    void *loader_context,
    const gp100_effect_catalog_t *catalog,
    gp100_ui_live_result_t *result)
{
    gp100_ui_runtime_t next_ui;
    gp100_preset_record_t next_preset;
    gp100_settings_record_t next_settings;
    gp100_looper_engine_t next_looper;
    gp100_effect_chain_t next_chain;
    gp100_effect_runtime_t next_effects;
    gp100_audio_output_level_t next_output;
    gp100_rhythm_settings_t next_rhythm;
    gp100_ui_live_result_t next_result;

    if (ui == NULL || preset == NULL || settings == NULL || looper == NULL ||
        chain == NULL || effects == NULL || output == NULL || rhythm == NULL ||
        event == NULL || loader == NULL || result == NULL) {
        return false;
    }
    next_ui = *ui;
    next_preset = *preset;
    next_settings = *settings;
    next_looper = *looper;
    next_chain = *chain;
    next_effects = *effects;
    next_output = *output;
    next_rhythm = *rhythm;
    if (!gp100_ui_live_handle_event(
            &next_ui, &next_preset, &next_settings, &next_looper,
            &next_chain, &next_effects, &next_output, &next_rhythm,
            calibration_heel, calibration_toe, event, &next_result)) {
        return false;
    }
    if ((next_result.ui_updates & GP100_UI_UPDATE_PRESET_LOAD) != 0u) {
        gp100_preset_recall_result_t recall;

        if (!gp100_preset_runtime_load_and_recall(
                next_ui.logical_preset, loader, loader_context, catalog, &next_settings,
                &next_preset, &next_ui, &next_chain, &next_effects,
                &next_output, &recall)) {
            return false;
        }
        next_result.ui_updates &= ~GP100_UI_UPDATE_PRESET_LOAD;
        next_result.ui_updates |= recall.ui_updates;
        memcpy(next_result.effect_actions, recall.effect_actions,
               sizeof(next_result.effect_actions));
        memcpy(next_result.tap_tempo_sync_slots,
               recall.tap_tempo_sync_slots,
               sizeof(next_result.tap_tempo_sync_slots));
        next_result.chain_changed =
            memcmp(chain, &next_chain, sizeof(*chain)) != 0;
        next_result.output_level_changed =
            memcmp(output, &next_output, sizeof(*output)) != 0;
        next_result.preset_loaded = true;
        next_result.preset_repairs = recall.repairs;
        next_result.tap_tempo_changed = ui->tap_tempo_enabled != next_ui.tap_tempo_enabled;
    }
    *ui = next_ui;
    *preset = next_preset;
    *settings = next_settings;
    *looper = next_looper;
    *chain = next_chain;
    *effects = next_effects;
    *output = next_output;
    *rhythm = next_rhythm;
    *result = next_result;
    return true;
}
