#include "ui_runtime.h"

#include <string.h>

static bool valid_mode(gp100_ui_mode_t mode)
{
    return mode >= GP100_UI_MODE_PLAY &&
           mode <= GP100_UI_MODE_CONSTRUCT;
}

/* Leaving a screen drops any open picker, pending preview or undo (the caller reverted a preview). */
static void reset_editor_state(gp100_patch_editor_t *editor)
{
    editor->picker = GP100_PATCH_PICK_NONE;
    editor->preview_pending = false;
    editor->preview_applied = false;
    editor->undo_available = false;
    editor->edit_held = false;
}

bool gp100_ui_runtime_set_mode(
    gp100_ui_runtime_t *runtime,
    gp100_ui_mode_t mode,
    const gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings)
{
    if (runtime == NULL || !valid_mode(mode)) {
        return false;
    }
    if (runtime->mode == mode) {
        return true;
    }
    runtime->patch_editor.hint_alt = false;
    switch (mode) {
    case GP100_UI_MODE_PLAY:
        runtime->play_footswitch_release_pending = false;
        break;
    case GP100_UI_MODE_DRUM:
        runtime->drum_release_guard = true;
        break;
    case GP100_UI_MODE_TUNER:
        if (!gp100_tuner_screen_load_settings(
                &runtime->tuner, settings)) {
            return false;
        }
        break;
    case GP100_UI_MODE_EDIT:
        gp100_edit_controller_init(&runtime->edit);
        reset_editor_state(&runtime->patch_editor);
        runtime->patch_editor.page = 0u;
        runtime->patch_editor.result = GP100_PATCH_OK;
        if (gp100_patch_is_native(preset)) {
            const size_t count = gp100_patch_count(preset);
            if (runtime->patch_editor.position >= count) {
                runtime->patch_editor.position = count == 0u ? 0u : (uint8_t)(count - 1u);
            }
        }
        break;
    case GP100_UI_MODE_CONSTRUCT:
        reset_editor_state(&runtime->patch_editor);
        runtime->patch_editor.result = GP100_PATCH_OK;
        break;
    case GP100_UI_MODE_LOOPER:
        runtime->looper_pair_release_guard = true;
        runtime->looper_clear_feedback = false;
        break;
    case GP100_UI_MODE_GLOBAL:
        gp100_global_controller_init(&runtime->global);
        break;
    case GP100_UI_MODE_SAVE:
        if (!gp100_save_screen_init(
                &runtime->save, runtime->logical_preset, preset)) {
            return false;
        }
        if (runtime->mode == GP100_UI_MODE_DRUM ||
            runtime->mode == GP100_UI_MODE_EDIT ||
            runtime->mode == GP100_UI_MODE_CONSTRUCT ||
            runtime->mode == GP100_UI_MODE_GLOBAL) {
            runtime->save.return_mode = runtime->mode;
        }
        break;
    case GP100_UI_MODE_HARDWARE_TEST:
        gp100_hardware_test_reset(
            &runtime->hardware_test,
            runtime->hardware_test.effect_enabled);
        break;
    default:
        break;
    }
    runtime->mode = mode;
    return true;
}

bool gp100_ui_runtime_init(
    gp100_ui_runtime_t *runtime,
    gp100_ui_mode_t initial_mode,
    uint16_t logical_preset,
    bool drum_enabled,
    bool effect_enabled,
    const gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings)
{
    if (runtime == NULL || logical_preset >= GP100_TOTAL_LOGICAL_PRESETS) {
        return false;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->logical_preset = logical_preset;
    runtime->drum_enabled = drum_enabled;
    gp100_tuner_reset(&runtime->tuner);
    gp100_hardware_test_reset(&runtime->hardware_test, effect_enabled);
    return gp100_ui_runtime_set_mode(
        runtime, initial_mode, preset, settings);
}

static bool enter_mode(
    gp100_ui_runtime_t *runtime,
    gp100_ui_mode_t next_mode,
    const gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings,
    uint32_t *updates)
{
    if (next_mode == runtime->mode) {
        return true;
    }
    if (!gp100_ui_runtime_set_mode(
            runtime, next_mode, preset, settings)) {
        return false;
    }
    *updates |= GP100_UI_UPDATE_MODE | GP100_UI_UPDATE_REDRAW;
    return true;
}

/* Holding GLOBAL on a normal screen opens the guide page for that screen. */
static bool help_requested(const gp100_ui_runtime_t *runtime, const gp100_event_t *event)
{
    gp100_ui_control_event_t decoded;

    if (runtime->mode != GP100_UI_MODE_PLAY && runtime->mode != GP100_UI_MODE_EDIT &&
        runtime->mode != GP100_UI_MODE_CONSTRUCT && runtime->mode != GP100_UI_MODE_DRUM &&
        runtime->mode != GP100_UI_MODE_LOOPER) {
        return false;
    }
    return gp100_decode_ui_control_event(event, &decoded) &&
           decoded.kind == GP100_UI_CONTROL_BUTTON &&
           decoded.control == GP100_BUTTON_GLOBAL && decoded.action == GP100_BUTTON_HOLD;
}

static bool footswitch_mode(
    const gp100_settings_record_t *settings,
    gp100_footswitch_mode_t *mode)
{
    int16_t value;

    if (!gp100_settings_get_i16(
            settings, GP100_SETTINGS_FOOTSWITCH_MODE, &value) ||
        value < GP100_FOOTSWITCH_MODE_PATCH ||
        value > GP100_FOOTSWITCH_MODE_STOMP) {
        return false;
    }
    *mode = (gp100_footswitch_mode_t)value;
    return true;
}

bool gp100_ui_runtime_handle_event(
    gp100_ui_runtime_t *runtime,
    gp100_preset_record_t *preset,
    gp100_settings_record_t *settings,
    gp100_looper_engine_t *looper,
    const gp100_event_t *event,
    uint32_t *updates)
{
    gp100_ui_mode_t next_mode;
    uint32_t next_updates = GP100_UI_UPDATE_NONE;

    if (runtime == NULL || preset == NULL || settings == NULL ||
        looper == NULL || event == NULL || updates == NULL ||
        !valid_mode(runtime->mode)) {
        return false;
    }
    next_mode = runtime->mode;
    if (help_requested(runtime, event)) {
        const gp100_ui_mode_t context = runtime->mode;
        const bool editor_mode = context == GP100_UI_MODE_EDIT || context == GP100_UI_MODE_CONSTRUCT;
        const bool picker_open = editor_mode &&
                                 runtime->patch_editor.picker == GP100_PATCH_PICK_EFFECT;
        const bool reverted = editor_mode && gp100_patch_is_native(preset) &&
                              gp100_patch_editor_cancel(&runtime->patch_editor, preset);

        if (!enter_mode(runtime, GP100_UI_MODE_GLOBAL, preset, settings, &next_updates)) {
            return false;
        }
        gp100_global_controller_open_guide(&runtime->global, context, picker_open);
        if (reverted) {
            next_updates |= GP100_UI_UPDATE_PRESET;
        }
        *updates = next_updates;
        return true;
    }
    switch (runtime->mode) {
    case GP100_UI_MODE_PLAY: {
        gp100_play_event_result_t result = gp100_play_handle_event(
            settings, preset, &runtime->logical_preset,
            &runtime->drum_enabled, &runtime->tap_tempo_enabled,
            &runtime->play_footswitch_release_pending,
            &runtime->tap_count, &runtime->last_tap_tick,
            event, &next_mode);

        if (result == GP100_PLAY_EVENT_INVALID) return false;
        if (result == GP100_PLAY_EVENT_SETTINGS_CHANGED)
            next_updates |= GP100_UI_UPDATE_SETTINGS;
        if (result == GP100_PLAY_EVENT_PRESET_CHANGED)
            next_updates |= GP100_UI_UPDATE_PRESET;
        if (result == GP100_PLAY_EVENT_PRESET_SELECTED)
            next_updates |= GP100_UI_UPDATE_PRESET_LOAD;
        if (result == GP100_PLAY_EVENT_DRUM_TOGGLED)
            next_updates |= GP100_UI_UPDATE_DRUM;
        if (result == GP100_PLAY_EVENT_TAP_TEMPO_TOGGLED)
            next_updates |= GP100_UI_UPDATE_TAP_TEMPO;
        if (result == GP100_PLAY_EVENT_TAP_TEMPO_APPLIED)
            next_updates |= GP100_UI_UPDATE_PRESET;
        if (result != GP100_PLAY_EVENT_UNHANDLED)
            next_updates |= GP100_UI_UPDATE_REDRAW;
        break;
    }
    case GP100_UI_MODE_TUNER: {
        gp100_tuner_screen_event_result_t result =
            gp100_tuner_screen_handle_event(
                &runtime->tuner, settings, event, &next_mode);

        if (result == GP100_TUNER_SCREEN_EVENT_INVALID) return false;
        if (result == GP100_TUNER_SCREEN_EVENT_SETTINGS_CHANGED)
            next_updates |= GP100_UI_UPDATE_SETTINGS |
                            GP100_UI_UPDATE_REDRAW;
        break;
    }
    case GP100_UI_MODE_DRUM: {
        gp100_drum_event_result_t result = gp100_drum_handle_event(
            settings, preset, &runtime->logical_preset,
            &runtime->drum_enabled, &runtime->drum_release_guard,
            event, &next_mode);

        if (result == GP100_DRUM_EVENT_INVALID) return false;
        if (result == GP100_DRUM_EVENT_SETTINGS_CHANGED)
            next_updates |= GP100_UI_UPDATE_SETTINGS;
        if (result == GP100_DRUM_EVENT_PLAYBACK_TOGGLED)
            next_updates |= GP100_UI_UPDATE_DRUM;
        if (result == GP100_DRUM_EVENT_PRESET_CHANGED)
            next_updates |= GP100_UI_UPDATE_PRESET;
        if (result == GP100_DRUM_EVENT_PRESET_SELECTED)
            next_updates |= GP100_UI_UPDATE_PRESET_LOAD;
        if (result != GP100_DRUM_EVENT_UNHANDLED)
            next_updates |= GP100_UI_UPDATE_REDRAW;
        break;
    }
    case GP100_UI_MODE_LOOPER: {
        gp100_looper_event_result_t result =
            gp100_looper_screen_handle_event(
                looper, settings, &runtime->logical_preset,
                &runtime->drum_enabled, event,
                &runtime->looper_pair_release_guard,
                &runtime->looper_clear_feedback, &next_mode);

        if (result == GP100_LOOPER_EVENT_INVALID) return false;
        if (result == GP100_LOOPER_EVENT_SETTINGS_CHANGED)
            next_updates |= GP100_UI_UPDATE_SETTINGS;
        if (result == GP100_LOOPER_EVENT_PRESET_SELECTED)
            next_updates |= GP100_UI_UPDATE_PRESET_LOAD;
        if (result == GP100_LOOPER_EVENT_ENGINE_ACTION)
            next_updates |= GP100_UI_UPDATE_LOOPER;
        if (result == GP100_LOOPER_EVENT_DRUM_TOGGLED)
            next_updates |= GP100_UI_UPDATE_DRUM;
        if (result != GP100_LOOPER_EVENT_UNHANDLED)
            next_updates |= GP100_UI_UPDATE_REDRAW;
        break;
    }
    case GP100_UI_MODE_CONSTRUCT:
    case GP100_UI_MODE_EDIT: {
        gp100_edit_event_result_t result;
        if (gp100_patch_is_native(preset)) {
            result = gp100_patch_editor_handle(
                &runtime->patch_editor, runtime->mode, preset, settings,
                &runtime->logical_preset, &runtime->drum_enabled, event, &next_mode);
        } else {
            gp100_footswitch_mode_t mode;
            if (!footswitch_mode(settings, &mode)) return false;
            result = gp100_edit_controller_handle_event(
                &runtime->edit, preset, settings, &runtime->logical_preset,
                mode, &runtime->drum_enabled, event, &next_mode);
        }
        if (result == GP100_EDIT_EVENT_INVALID) return false;
        if (result == GP100_EDIT_EVENT_PRESET_CHANGED)
            next_updates |= GP100_UI_UPDATE_PRESET;
        if (result == GP100_EDIT_EVENT_PRESET_SELECTED)
            next_updates |= GP100_UI_UPDATE_PRESET_LOAD;
        if (result == GP100_EDIT_EVENT_DRUM_TOGGLED)
            next_updates |= GP100_UI_UPDATE_DRUM;
        if (result != GP100_EDIT_EVENT_UNHANDLED)
            next_updates |= GP100_UI_UPDATE_REDRAW;
        break;
    }
    case GP100_UI_MODE_GLOBAL: {
        gp100_global_event_result_t result =
            gp100_global_controller_handle_expression_event(
                &runtime->global, preset, event);

        if (result == GP100_GLOBAL_EVENT_UNHANDLED) {
            result = gp100_global_controller_handle_event(
                &runtime->global, settings, event, &next_mode);
        }

        if (result == GP100_GLOBAL_EVENT_INVALID) return false;
        if (result == GP100_GLOBAL_EVENT_SETTINGS_CHANGED)
            next_updates |= GP100_UI_UPDATE_SETTINGS;
        if (result == GP100_GLOBAL_EVENT_PRESET_CHANGED)
            next_updates |= GP100_UI_UPDATE_PRESET;
        if (result == GP100_GLOBAL_EVENT_DRUM_TOGGLE_REQUESTED) {
            runtime->drum_enabled = !runtime->drum_enabled;
            next_updates |= GP100_UI_UPDATE_DRUM;
        }
        if (result == GP100_GLOBAL_EVENT_FACTORY_RESET_REQUESTED)
            next_updates |= GP100_UI_UPDATE_FACTORY_RESET;
        if (result == GP100_GLOBAL_EVENT_CALIBRATION_COMMIT)
            next_updates |= GP100_UI_UPDATE_CALIBRATION_COMMIT;
        if (runtime->global.guide_return_pending) {
            /* The guide was opened by holding GLOBAL: EXIT goes back to that screen. */
            next_mode = runtime->global.guide_return;
            runtime->global.guide_return_pending = false;
        }
        if (result != GP100_GLOBAL_EVENT_UNHANDLED)
            next_updates |= GP100_UI_UPDATE_REDRAW;
        break;
    }
    case GP100_UI_MODE_SAVE: {
        gp100_save_event_result_t result =
            gp100_save_screen_handle_event(
                &runtime->save, &runtime->drum_enabled,
                event, &next_mode);

        if (result == GP100_SAVE_EVENT_INVALID) return false;
        if (result == GP100_SAVE_EVENT_COMMIT_REQUEST)
            next_updates |= GP100_UI_UPDATE_SAVE_COMMIT;
        if (result == GP100_SAVE_EVENT_DRUM_TOGGLED)
            next_updates |= GP100_UI_UPDATE_DRUM;
        if (result != GP100_SAVE_EVENT_UNHANDLED)
            next_updates |= GP100_UI_UPDATE_REDRAW;
        break;
    }
    case GP100_UI_MODE_HARDWARE_TEST: {
        gp100_ui_control_event_t decoded;

        if (gp100_hardware_test_handle_event(
                &runtime->hardware_test, event)) {
            next_updates |= GP100_UI_UPDATE_REDRAW;
            if (runtime->hardware_test.exit_ready &&
                gp100_decode_ui_control_event(event, &decoded) &&
                decoded.kind == GP100_UI_CONTROL_BUTTON &&
                decoded.action == GP100_BUTTON_RELEASE) {
                next_mode = GP100_UI_MODE_PLAY;
            }
        }
        break;
    }
    default:
        return false;
    }
    if (!enter_mode(
            runtime, next_mode, preset, settings, &next_updates)) {
        return false;
    }
    *updates = next_updates;
    return true;
}
