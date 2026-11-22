#include "midi_cc_runtime.h"

#include "storage/patch_chain.h"

#include "protocol/midi/sysex/vendor_sysex_performance.h"

#include <stddef.h>
#include <string.h>

static bool runtime_is_valid(const gp100_midi_cc_runtime_t *runtime)
{
    return runtime != NULL && runtime->ui != NULL &&
           runtime->preset != NULL && runtime->settings != NULL &&
           runtime->looper != NULL && runtime->effects != NULL &&
           runtime->rhythm != NULL;
}

static bool is_rhythm_action(gp100_midi_cc_target_t target)
{
    return target == GP100_MIDI_CC_RHYTHM_ENABLE ||
           target == GP100_MIDI_CC_RHYTHM_STYLE ||
           target == GP100_MIDI_CC_RHYTHM_VOLUME ||
           target == GP100_MIDI_CC_RHYTHM_TEMPO_BPM;
}

static bool set_setting(gp100_settings_record_t *settings,
                        gp100_settings_field_t field,
                        int16_t value)
{
    if (!gp100_settings_set_i16(settings, field, value)) {
        return false;
    }
    gp100_settings_update_crc(settings);
    return true;
}

static bool apply_performance_setting(
    gp100_settings_record_t *settings,
    gp100_sysex_performance_kind_t kind,
    uint16_t value)
{
    gp100_sysex_performance_action_t action;

    action.kind = kind;
    action.value = value;
    if (!gp100_sysex_performance_apply(settings, &action)) {
        return false;
    }
    gp100_settings_update_crc(settings);
    return true;
}

static bool apply_expression(
    const gp100_midi_cc_action_t *action,
    gp100_preset_record_t *preset,
    gp100_effect_runtime_t *effects,
    gp100_midi_cc_apply_result_t *result)
{
    gp100_expression_apply_result_t expression;
    size_t family;

    if (action->value < 0 || action->value > 255 ||
        !gp100_expression_runtime_apply(
            preset, effects, (uint16_t)action->value, 255u,
            &expression)) {
        return false;
    }
    result->volume_pedal_changed = expression.volume_changed;
    result->volume_pedal_percent = expression.volume_percent;
    result->volume_pedal_position = expression.volume_position;
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        result->effect_changed_slots[family] =
            expression.effect_changed_slots[family];
    }
    return true;
}

static bool apply_action_to_staged_state(
    const gp100_midi_cc_action_t *action,
    gp100_ui_runtime_t *ui,
    gp100_preset_record_t *preset,
    gp100_settings_record_t *settings,
    gp100_looper_engine_t *looper,
    gp100_effect_runtime_t *effects,
    gp100_midi_cc_apply_result_t *result)
{
    switch (action->target) {
    case GP100_MIDI_CC_MASTER_VOLUME:
        if (action->value < 0 || action->value > 100 ||
            !set_setting(settings, GP100_SETTINGS_MASTER_VOLUME,
                         action->value)) {
            return false;
        }
        result->master_volume_changed = true;
        result->master_volume_percent = (uint8_t)action->value;
        result->ui_updates |= GP100_UI_UPDATE_SETTINGS |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    case GP100_MIDI_CC_EXPRESSION_VALUE:
        return apply_expression(action, preset, effects, result);
    case GP100_MIDI_CC_EXPRESSION_TARGET: {
        gp100_patch_block_t block;
        if (action->value < GP100_EXPRESSION_PEDAL_VOLUME ||
            action->value > GP100_EXPRESSION_PEDAL_EFFECT ||
            !gp100_patch_expression_set_mode(
                preset,
                (gp100_expression_pedal_target_t)action->value)) {
            return false;
        }
        if (gp100_patch_expression_get(preset, NULL, &block)) {
            effects->slots[block.id].enabled = block.enabled;
        }
        gp100_preset_update_crc(preset);
        result->ui_updates |= GP100_UI_UPDATE_PRESET |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    }
    case GP100_MIDI_CC_VOLUME_PEDAL_POSITION:
        if (action->value < GP100_VOLUME_PEDAL_PRE ||
            action->value > GP100_VOLUME_PEDAL_POST ||
            !gp100_preset_set_volume_pedal_position(
                preset, (gp100_volume_pedal_position_t)action->value)) {
            return false;
        }
        gp100_preset_update_crc(preset);
        result->ui_updates |= GP100_UI_UPDATE_PRESET |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    case GP100_MIDI_CC_PRESET_STEP:
        if (gp100_patch_is_native(preset)) {
            if (gp100_play_adjust(settings, preset, &ui->logical_preset,
                                  GP100_PLAY_CONTROL_PRESET, action->value > 0,
                                  1u) != GP100_PLAY_ACTION_SELECT_PRESET) {
                return false;
            }
        } else {
            ui->logical_preset = gp100_midi_step_preset(
                ui->logical_preset, action->value);
        }
        result->ui_updates |= GP100_UI_UPDATE_PRESET_LOAD |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    case GP100_MIDI_CC_EFFECT_ENABLE: {
        gp100_effect_family_t family =
            (gp100_effect_family_t)action->index;

        if (gp100_patch_is_native(preset)) {
            if ((unsigned)family >= GP100_EFFECT_FAMILY_COUNT ||
                (action->value != 0 && action->value != 1)) {
                return false;
            }
            bool changed = false;
            for (size_t i = 0u; i < gp100_patch_count(preset); ++i) {
                gp100_patch_block_t block;
                if (!gp100_patch_block_at(preset, i, &block)) {
                    return false;
                }
                if (block.family == family && block.enabled != (action->value != 0)) {
                    block.enabled = action->value != 0;
                    if (!gp100_patch_update(preset, &block)) {
                        return false;
                    }
                    changed = true;
                }
            }
            if (changed) {
                if (!gp100_effect_runtime_sync_preset(effects, preset, result->effect_actions)) {
                    return false;
                }
                result->ui_updates |= GP100_UI_UPDATE_PRESET | GP100_UI_UPDATE_REDRAW;
            }
            return true;
        }
        if (action->index >= GP100_EFFECT_FAMILY_COUNT ||
            (action->value != 0 && action->value != 1) ||
            !gp100_preset_set_effect_enabled(
                preset, family, action->value != 0) ||
            !gp100_effect_runtime_set_enabled(
                effects, family, action->value != 0,
                &result->effect_actions[action->index])) {
            return false;
        }
        gp100_preset_update_crc(preset);
        result->ui_updates |= GP100_UI_UPDATE_PRESET |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    }
    case GP100_MIDI_CC_UI_MODE:
        if (!gp100_ui_runtime_set_mode(
                ui, (gp100_ui_mode_t)action->value,
                preset, settings)) {
            return false;
        }
        result->ui_updates |= GP100_UI_UPDATE_MODE |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    case GP100_MIDI_CC_RHYTHM_ENABLE:
        if (action->value != 0 && action->value != 1) {
            return false;
        }
        ui->drum_enabled = action->value != 0;
        result->ui_updates |= GP100_UI_UPDATE_DRUM |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    case GP100_MIDI_CC_RHYTHM_STYLE:
        if (action->value < 0 ||
            !apply_performance_setting(
                settings, GP100_SYSEX_PERFORMANCE_RHYTHM_STYLE,
                (uint16_t)action->value)) {
            return false;
        }
        result->ui_updates |= GP100_UI_UPDATE_SETTINGS |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    case GP100_MIDI_CC_RHYTHM_VOLUME:
        if (action->value < 0 ||
            !apply_performance_setting(
                settings, GP100_SYSEX_PERFORMANCE_RHYTHM_VOLUME,
                (uint16_t)action->value)) {
            return false;
        }
        result->ui_updates |= GP100_UI_UPDATE_SETTINGS |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    case GP100_MIDI_CC_LOOPER_EVENT: {
        bool changed;

        if (action->value < GP100_LOOPER_RECORD_OR_OVERDUB ||
            action->value > GP100_LOOPER_CLEAR ||
            !gp100_looper_handle_event(
                looper, (gp100_looper_event_t)action->value,
                &changed)) {
            return false;
        }
        if (changed) {
            result->ui_updates |= GP100_UI_UPDATE_LOOPER |
                                  GP100_UI_UPDATE_REDRAW;
        }
        return true;
    }
    case GP100_MIDI_CC_LOOP_RECORD_VOLUME:
    case GP100_MIDI_CC_LOOP_VOLUME: {
        uint8_t record_level = looper->record_level_target;
        uint8_t playback_level = looper->playback_level_target;
        gp100_sysex_performance_kind_t kind;

        if (action->value < 0 || action->value > (int)GP100_LOOPER_MAX_LEVEL) {
            return false;
        }
        if (action->target == GP100_MIDI_CC_LOOP_RECORD_VOLUME) {
            record_level = (uint8_t)action->value;
            kind = GP100_SYSEX_PERFORMANCE_LOOP_RECORD_VOLUME;
        } else {
            playback_level = (uint8_t)action->value;
            kind = GP100_SYSEX_PERFORMANCE_LOOP_PLAYBACK_VOLUME;
        }
        if (!gp100_looper_set_levels(
                looper, record_level, playback_level) ||
            !apply_performance_setting(
                settings, kind, (uint16_t)action->value)) {
            return false;
        }
        result->ui_updates |= GP100_UI_UPDATE_SETTINGS |
                              GP100_UI_UPDATE_LOOPER |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    }
    case GP100_MIDI_CC_FOOTSWITCH_MODE:
        if (action->value < GP100_FOOTSWITCH_MODE_PATCH ||
            action->value > GP100_FOOTSWITCH_MODE_STOMP ||
            !set_setting(settings, GP100_SETTINGS_FOOTSWITCH_MODE,
                         action->value)) {
            return false;
        }
        result->ui_updates |= GP100_UI_UPDATE_SETTINGS |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    case GP100_MIDI_CC_USB_RECORD_SOURCE_LEFT:
        if (action->value < GP100_USB_RECORD_SOURCE_DRY ||
            action->value > GP100_USB_RECORD_SOURCE_EFFECT ||
            !set_setting(
                settings, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT,
                action->value)) {
            return false;
        }
        result->ui_updates |= GP100_UI_UPDATE_SETTINGS;
        return true;
    case GP100_MIDI_CC_TEMPO_BPM:
        if (action->value < 0 ||
            !gp100_preset_set_tempo_bpm(
                preset, (uint16_t)action->value) ||
            !gp100_looper_set_tempo(
                looper, looper->sample_rate_hz,
                (uint16_t)action->value)) {
            return false;
        }
        gp100_preset_update_crc(preset);
        result->ui_updates |= GP100_UI_UPDATE_PRESET |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    case GP100_MIDI_CC_INPUT_LEVEL:
        if (action->value < 0 || action->value > 20 ||
            !set_setting(settings, GP100_SETTINGS_INPUT_LEVEL_DB,
                         action->value)) {
            return false;
        }
        result->ui_updates |= GP100_UI_UPDATE_SETTINGS;
        return true;
    case GP100_MIDI_CC_RHYTHM_TEMPO_BPM:
        if (action->value < 0 ||
            !apply_performance_setting(
                settings, GP100_SYSEX_PERFORMANCE_RHYTHM_TEMPO,
                (uint16_t)action->value)) {
            return false;
        }
        result->ui_updates |= GP100_UI_UPDATE_SETTINGS |
                              GP100_UI_UPDATE_REDRAW;
        return true;
    default:
        return false;
    }
}

bool gp100_midi_cc_apply_action(
    const gp100_midi_cc_action_t *action,
    gp100_midi_cc_runtime_t *runtime,
    gp100_midi_cc_apply_result_t *result)
{
    gp100_ui_runtime_t ui;
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    gp100_looper_engine_t looper;
    gp100_effect_runtime_t effects;
    gp100_rhythm_settings_t rhythm;
    gp100_midi_cc_apply_result_t next_result;

    if (action == NULL || result == NULL || !runtime_is_valid(runtime)) {
        return false;
    }
    ui = *runtime->ui;
    preset = *runtime->preset;
    settings = *runtime->settings;
    looper = *runtime->looper;
    effects = *runtime->effects;
    rhythm = *runtime->rhythm;
    memset(&next_result, 0, sizeof(next_result));

    if (!apply_action_to_staged_state(
            action, &ui, &preset, &settings, &looper, &effects,
            &next_result)) {
        return false;
    }
    if (is_rhythm_action(action->target)) {
        gp100_rhythm_settings_t next_rhythm;

        if (!gp100_rhythm_settings_from_record(
                &settings, ui.drum_enabled, &next_rhythm)) {
            return false;
        }
        next_result.rhythm_changed =
            memcmp(&rhythm, &next_rhythm, sizeof(rhythm)) != 0;
        rhythm = next_rhythm;
    }
    *runtime->ui = ui;
    *runtime->preset = preset;
    *runtime->settings = settings;
    *runtime->looper = looper;
    *runtime->effects = effects;
    *runtime->rhythm = rhythm;
    *result = next_result;
    return true;
}

bool gp100_midi_cc_decode_and_apply(
    uint8_t controller,
    uint8_t midi_value,
    gp100_midi_cc_runtime_t *runtime,
    gp100_midi_cc_apply_result_t *result)
{
    gp100_midi_cc_action_t action;

    return gp100_midi_cc_decode(controller, midi_value, &action) &&
           gp100_midi_cc_apply_action(&action, runtime, result);
}
