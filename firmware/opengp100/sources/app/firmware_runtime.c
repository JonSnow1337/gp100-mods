#include "firmware_runtime.h"

#include <string.h>

#define GP100_UNSAVED_WARNING_GUARD_MS 400u

typedef struct setting_default {
    gp100_settings_field_t field;
    int16_t value;
} setting_default_t;

static const setting_default_t common_setting_defaults[] = {
    {GP100_SETTINGS_FOOTSWITCH_MODE,
     GP100_FOOTSWITCH_MODE_PATCH},
    {GP100_SETTINGS_DRUM_GENRE, 0},
    {GP100_SETTINGS_DRUM_STYLE, 0},
    {GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0},
    {GP100_SETTINGS_DRUM_TEMPO, 120},
    {GP100_SETTINGS_TUNER_REFERENCE_HZ, 440},
    {GP100_SETTINGS_TUNER_MODE, GP100_TUNER_OUTPUT_MUTE},
    {GP100_SETTINGS_LANGUAGE_INDEX, 0},
    {GP100_SETTINGS_LOOPER_POSITION, GP100_LOOPER_POST_EFFECTS},
    {GP100_SETTINGS_MASTER_VOLUME, 50},
    {GP100_SETTINGS_INPUT_LEVEL_DB, 0},
    {GP100_SETTINGS_USB_MONITOR_LEVEL_DB, 0},
    {GP100_SETTINGS_USB_RECORD_LEVEL_DB, 0},
    {GP100_SETTINGS_DATA_REVISION,
     (int16_t)GP100_SETTINGS_V210_DATA_REVISION},
    {GP100_SETTINGS_TAP_SYNC_PRE, 0},
    {GP100_SETTINGS_TAP_SYNC_WAH, 0},
    {GP100_SETTINGS_TAP_SYNC_DISTORTION, 0},
    {GP100_SETTINGS_TAP_SYNC_AMPLIFIER, 0},
    {GP100_SETTINGS_TAP_SYNC_NOISE_GATE, 0},
    {GP100_SETTINGS_TAP_SYNC_CABINET, 0},
    {GP100_SETTINGS_TAP_SYNC_MODULATION, 0},
    {GP100_SETTINGS_TAP_SYNC_DELAY, 0},
    {GP100_SETTINGS_TAP_SYNC_REVERB, 0},
    {GP100_SETTINGS_NO_CAB_LEFT, 0},
    {GP100_SETTINGS_NO_CAB_RIGHT, 0}
};

static const setting_default_t development_setting_defaults[] = {
    {GP100_SETTINGS_DRUM_VOLUME, 80},
    {GP100_SETTINGS_LOOP_RECORD_VOLUME, 80},
    {GP100_SETTINGS_LOOP_PLAYBACK_VOLUME, 80},
    {GP100_SETTINGS_USB_RECORD_SOURCE_LEFT, 0},
    {GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT, 0}
};

static const setting_default_t factory_setting_defaults[] = {
    {GP100_SETTINGS_DRUM_VOLUME, 50},
    {GP100_SETTINGS_LOOP_RECORD_VOLUME, 99},
    {GP100_SETTINGS_LOOP_PLAYBACK_VOLUME, 99},
    {GP100_SETTINGS_USB_RECORD_SOURCE_LEFT, 1},
    {GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT, 1}
};

bool gp100_make_development_preset(
    gp100_preset_record_t *preset,
    const char *name)
{
    gp100_preset_record_t next;
    gp100_control_target_t target;
    gp100_effect_catalog_t catalog;
    uint16_t positions[GP100_EFFECT_FAMILY_COUNT];
    size_t assignment;
    size_t family;
    size_t target_index;
    size_t range;

    if (preset == NULL || name == NULL) {
        return false;
    }
    memset(&next, 0, sizeof(next));
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        const gp100_effect_family_t kind = (gp100_effect_family_t)family;
        const int algorithm = gp100_effect_algorithm_index(kind, gp100_effect_default_algorithm_id(kind));
        positions[family] = (uint16_t)family;
        if ((algorithm >= 0 && !gp100_preset_select_algorithm(&next, kind, (size_t)algorithm)) ||
            !gp100_preset_set_effect_enabled(
                &next, kind,
                algorithm >= 0 && (kind == GP100_EFFECT_AMPLIFIER || kind == GP100_EFFECT_CABINET))) {
            return false;
        }
    }
    if (!gp100_preset_set_effect_positions(&next, positions) ||
        !gp100_preset_set_name(&next, name) ||
        !gp100_preset_set_tempo_bpm(&next, 120u) ||
        !gp100_preset_set_level(&next, 50u) ||
        !gp100_preset_set_expression_pedal_target(
            &next, GP100_EXPRESSION_PEDAL_VOLUME) ||
        !gp100_preset_set_volume_pedal_position(
            &next, GP100_VOLUME_PEDAL_POST) ||
        !gp100_preset_set_genre_index(&next, 0u)) {
        return false;
    }
    for (assignment = 0u;
         assignment < GP100_STOMP_FOOTSWITCH_COUNT *
                          GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH;
         ++assignment) {
        static const uint16_t assignments[] = {
            GP100_EFFECT_PRE_FX,
            GP100_EFFECT_DISTORTION,
            GP100_EFFECT_AMPLIFIER,
            GP100_EFFECT_MODULATION,
            GP100_EFFECT_DELAY,
            GP100_EFFECT_REVERB
        };

        if (!gp100_preset_set_control_assignment(
                &next,
                assignment / GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH,
                assignment % GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH,
                assignments[assignment])) {
            return false;
        }
    }
    memset(&target, 0, sizeof(target));
    for (target_index = 0u;
         target_index < GP100_CONTROL_TARGET_COUNT;
         ++target_index) {
        if (!gp100_preset_set_control_target(
                &next, target_index, &target)) {
            return false;
        }
    }
    for (range = 0u; range < GP100_CONTROL_RANGE_COUNT; ++range) {
        if (!gp100_preset_set_control_range(
                &next, (gp100_control_range_t)range,
                0.0f, 99.0f)) {
            return false;
        }
    }
    /* Install schema/cache/tail fields through the same repair path used by
     * recall, then checksum the final semantic record. */
    catalog = gp100_effect_catalog_core();
    (void)gp100_preset_normalize(&next, &catalog);
    gp100_preset_update_crc(&next);
    *preset = next;
    return true;
}

static bool make_settings(
    gp100_settings_record_t *settings,
    const setting_default_t *specific_defaults,
    size_t specific_default_count)
{
    gp100_settings_record_t next;
    size_t index;

    if (settings == NULL) {
        return false;
    }
    memset(&next, 0, sizeof(next));
    for (index = 0u;
         index < sizeof(common_setting_defaults) /
                     sizeof(common_setting_defaults[0]);
         ++index) {
        if (!gp100_settings_set_i16(
                &next, common_setting_defaults[index].field,
                common_setting_defaults[index].value)) {
            return false;
        }
    }
    for (index = 0u; index < specific_default_count; ++index) {
        if (!gp100_settings_set_i16(
                &next, specific_defaults[index].field,
                specific_defaults[index].value)) {
            return false;
        }
    }
    if (!gp100_settings_set_factory_global_eq(&next)) {
        return false;
    }
    gp100_settings_update_crc(&next);
    *settings = next;
    return true;
}

bool gp100_make_factory_settings(
    gp100_settings_record_t *settings)
{
    return make_settings(
        settings, factory_setting_defaults,
        sizeof(factory_setting_defaults) / sizeof(factory_setting_defaults[0]));
}

bool gp100_make_development_settings(
    gp100_settings_record_t *settings)
{
    return make_settings(
        settings, development_setting_defaults,
        sizeof(development_setting_defaults) /
            sizeof(development_setting_defaults[0]));
}

static bool read_looper_settings(
    const gp100_settings_record_t *settings,
    gp100_looper_position_t *position,
    uint8_t *record_level,
    uint8_t *playback_level)
{
    int16_t position_value;
    int16_t record_value;
    int16_t playback_value;

    if (!gp100_settings_get_i16(
            settings, GP100_SETTINGS_LOOPER_POSITION,
            &position_value) ||
        !gp100_settings_get_i16(
            settings, GP100_SETTINGS_LOOP_RECORD_VOLUME,
            &record_value) ||
        !gp100_settings_get_i16(
            settings, GP100_SETTINGS_LOOP_PLAYBACK_VOLUME,
            &playback_value) ||
        (position_value != GP100_LOOPER_POST_EFFECTS &&
         position_value != GP100_LOOPER_PRE_EFFECTS) ||
        record_value < 0 ||
        record_value > (int16_t)GP100_LOOPER_MAX_LEVEL ||
        playback_value < 0 ||
        playback_value > (int16_t)GP100_LOOPER_MAX_LEVEL) {
        return false;
    }
    *position = (gp100_looper_position_t)position_value;
    *record_level = (uint8_t)record_value;
    *playback_level = (uint8_t)playback_value;
    return true;
}

bool gp100_firmware_runtime_init(
    gp100_firmware_runtime_t *runtime,
    uint16_t logical_preset,
    const gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings,
    int32_t *looper_storage,
    size_t looper_storage_samples,
    uint16_t calibration_heel,
    uint16_t calibration_toe)
{
    gp100_firmware_runtime_t next;
    gp100_preset_recall_result_t recall;
    gp100_looper_position_t looper_position;
    uint8_t record_level;
    uint8_t playback_level;
    gp100_volume_pedal_position_t volume_position;

    if (runtime == NULL || preset == NULL || settings == NULL ||
        looper_storage == NULL || looper_storage_samples == 0u ||
        logical_preset >= GP100_TOTAL_LOGICAL_PRESETS ||
        calibration_heel >= calibration_toe ||
        gp100_preset_validate(preset) != GP100_PRESET_VALID ||
        !gp100_settings_validate(settings) ||
        !read_looper_settings(
            settings, &looper_position, &record_level, &playback_level) ||
        !gp100_preset_volume_pedal_position(
            preset, &volume_position)) {
        return false;
    }
    memset(&next, 0, sizeof(next));
    next.preset = *preset;
    next.settings = *settings;
    next.catalog = gp100_effect_catalog_core();
    next.calibration_heel = calibration_heel;
    next.calibration_toe = calibration_toe;
    next.volume_pedal_percent = 100u;
    next.volume_pedal_position = volume_position;
    gp100_effect_runtime_reset(&next.effects);
    gp100_audio_output_level_reset(&next.output);
    if (!gp100_looper_init(
            &next.looper, looper_storage, looper_storage_samples,
            looper_position, record_level, playback_level) ||
        !gp100_rhythm_settings_from_record(
            &next.settings, false, &next.rhythm) ||
        !gp100_ui_runtime_init(
            &next.ui, GP100_UI_MODE_PLAY, logical_preset,
            false, true, &next.preset, &next.settings) ||
        !gp100_preset_runtime_recall(
            logical_preset, &next.catalog, &next.settings, &next.preset, &next.ui,
            &next.chain, &next.effects, &next.output, &recall)) {
        return false;
    }
    *runtime = next;
    return true;
}

bool gp100_firmware_runtime_handle_event(
    gp100_firmware_runtime_t *runtime,
    const gp100_event_t *event,
    gp100_preset_loader_t loader,
    void *loader_context,
    gp100_firmware_runtime_result_t *result)
{
    gp100_firmware_runtime_result_t next_result;
    bool was_preset_modified;
    bool was_settings_dirty;

    if (runtime == NULL || event == NULL || loader == NULL ||
        result == NULL) {
        return false;
    }
    memset(&next_result, 0, sizeof(next_result));
    /* Unsaved-changes warning (Play mode only, PARA only; the footswitches never warn so a
     * preset change on stage is always instant). First PARA turn with unsaved edits shows the
     * warning, the next one goes on and discards them. Turns within the guard time of the warning
     * are part of the same spin and are swallowed. Any other input dismisses the warning. */
    const bool warning_was_shown = runtime->ui.unsaved_warning;
    {
        gp100_ui_control_event_t decoded;
        if (gp100_decode_ui_control_event(event, &decoded) &&
            gp100_ui_control_dismisses_warning(&decoded)) {
            const bool para_turn = decoded.kind == GP100_UI_CONTROL_ENCODER &&
                                   decoded.control == GP100_ENCODER_PARA;
            int16_t warning_off = 0;
            (void)gp100_settings_get_i16(&runtime->settings, GP100_SETTINGS_UNSAVED_WARNING_OFF,
                                         &warning_off);
            if (runtime->ui.mode == GP100_UI_MODE_PLAY && runtime->preset_modified && para_turn &&
                warning_off != 1) {
                if (!warning_was_shown) {
                    runtime->ui.unsaved_warning = true;
                    runtime->ui.unsaved_warning_ms = runtime->clock_ms;
                    next_result.live.ui_updates = GP100_UI_UPDATE_REDRAW;
                    *result = next_result;
                    return true;
                }
                if ((uint32_t)(runtime->clock_ms - runtime->ui.unsaved_warning_ms) <
                    GP100_UNSAVED_WARNING_GUARD_MS) {
                    *result = next_result;
                    return true;
                }
            }
            runtime->ui.unsaved_warning = false;
        }
    }
    was_preset_modified = runtime->preset_modified;
    was_settings_dirty = runtime->settings_dirty;
    /* Remember whether the preset was modified before a picker/removal starts, so that cancelling
     * or undoing it can bring the "modified" marker back to that state. */
    if (runtime->ui.patch_editor.picker == GP100_PATCH_PICK_NONE &&
        !runtime->ui.patch_editor.undo_available) {
        runtime->ui.patch_editor.modified_snapshot = runtime->preset_modified;
    }
    if (!gp100_ui_live_handle_event_and_recall(
            &runtime->ui, &runtime->preset, &runtime->settings,
            &runtime->looper, &runtime->chain, &runtime->effects,
            &runtime->output, &runtime->rhythm,
            runtime->calibration_heel, runtime->calibration_toe,
            event, loader, loader_context, &runtime->catalog,
            &next_result.live)) {
        return false;
    }
    if (next_result.live.expression.apply.volume_changed &&
        !gp100_firmware_runtime_set_volume_pedal(
            runtime, next_result.live.expression.apply.volume_percent,
            next_result.live.expression.apply.volume_position)) {
        return false;
    }
    if ((next_result.live.ui_updates & GP100_UI_UPDATE_PRESET) != 0u) {
        runtime->preset_modified = !next_result.live.preset_loaded;
    }
    if (runtime->ui.patch_editor.restore_clean) {
        runtime->ui.patch_editor.restore_clean = false;
        runtime->preset_modified = runtime->ui.patch_editor.modified_snapshot;
    }
    if (next_result.live.preset_loaded) {
        runtime->preset_modified = false;
        if (!gp100_settings_set_i16(
                &runtime->settings, GP100_SETTINGS_CURRENT_PRESET,
                (int16_t)runtime->ui.logical_preset)) {
            return false;
        }
        runtime->settings_dirty = true;
    }
    if ((next_result.live.ui_updates & GP100_UI_UPDATE_PRESET) != 0u &&
        !gp100_preset_volume_pedal_position(
            &runtime->preset, &runtime->volume_pedal_position)) {
        return false;
    }
    if ((next_result.live.ui_updates & GP100_UI_UPDATE_SETTINGS) != 0u) {
        runtime->settings_dirty = true;
    }
    if (warning_was_shown && !runtime->ui.unsaved_warning) {
        next_result.live.ui_updates |= GP100_UI_UPDATE_REDRAW;   /* take the warning off the screen */
    }
    next_result.settings_changed =
        next_result.live.preset_loaded ||
        (next_result.live.ui_updates & GP100_UI_UPDATE_SETTINGS) != 0u;
    next_result.preset_became_modified =
        !was_preset_modified && runtime->preset_modified;
    next_result.settings_became_dirty =
        !was_settings_dirty && runtime->settings_dirty;
    *result = next_result;
    return true;
}

bool gp100_firmware_runtime_set_volume_pedal(
    gp100_firmware_runtime_t *runtime,
    float percent,
    gp100_volume_pedal_position_t position)
{
    if (runtime == NULL || percent < 0.0f || percent > 100.0f ||
        percent != percent ||
        (position != GP100_VOLUME_PEDAL_PRE &&
         position != GP100_VOLUME_PEDAL_POST)) {
        return false;
    }
    runtime->volume_pedal_percent = (uint8_t)percent;
    runtime->volume_pedal_position = position;
    return true;
}

void gp100_firmware_runtime_mark_preset_saved(
    gp100_firmware_runtime_t *runtime,
    uint32_t flash_address,
    const gp100_preset_record_t *saved_preset)
{
    uint32_t active_address;

    if (runtime != NULL && saved_preset != NULL &&
        gp100_preset_flash_address(runtime->ui.logical_preset, &active_address) &&
        active_address == flash_address) {
        runtime->preset = *saved_preset;
        runtime->preset_modified = false;
    }
}

void gp100_firmware_runtime_mark_settings_saved(
    gp100_firmware_runtime_t *runtime)
{
    if (runtime != NULL) {
        runtime->settings_dirty = false;
    }
}
