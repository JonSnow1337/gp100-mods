#include "expression_event.h"
#include "storage/patch_chain.h"

#include <string.h>

static bool mode_handles_expression(gp100_ui_mode_t mode)
{
    return (mode >= GP100_UI_MODE_PLAY && mode <= GP100_UI_MODE_SAVE) ||
           mode == GP100_UI_MODE_CONSTRUCT;
}

gp100_expression_event_status_t gp100_expression_event_handle(
    gp100_ui_mode_t mode,
    gp100_preset_record_t *preset,
    gp100_effect_runtime_t *effects,
    uint16_t calibration_heel,
    uint16_t calibration_toe,
    const gp100_event_t *event,
    gp100_expression_event_result_t *result)
{
    gp100_ui_control_event_t decoded;
    gp100_expression_event_result_t next;
    gp100_preset_record_t toggled_preset;

    if (preset == NULL || effects == NULL || event == NULL || result == NULL) {
        return GP100_EXPRESSION_EVENT_INVALID;
    }
    if (!gp100_decode_ui_control_event(event, &decoded) ||
        !mode_handles_expression(mode) ||
        (decoded.kind != GP100_UI_CONTROL_EXPRESSION_POSITION &&
         decoded.kind != GP100_UI_CONTROL_EXPRESSION_ACTIVATE)) {
        return GP100_EXPRESSION_EVENT_UNHANDLED;
    }
    memset(&next, 0, sizeof(next));
    const bool volume_only_push = decoded.kind == GP100_UI_CONTROL_EXPRESSION_ACTIVATE &&
        gp100_patch_is_native(preset) && !gp100_patch_expression_get(preset, NULL, NULL);
    if (decoded.kind == GP100_UI_CONTROL_EXPRESSION_POSITION || volume_only_push) {
        if (!gp100_expression_runtime_apply_adc(
                preset, effects, decoded.sample,
                calibration_heel, calibration_toe, &next.apply) ||
            !gp100_preset_expression_pedal_target(
                preset, &next.active_target)) {
            return GP100_EXPRESSION_EVENT_INVALID;
        }
        if (volume_only_push) {
            next.active_target = GP100_EXPRESSION_PEDAL_VOLUME;
        }
        *result = next;
        return GP100_EXPRESSION_EVENT_POSITION;
    }

    if (!gp100_preset_expression_pedal_target(
            preset, &next.active_target)) {
        return GP100_EXPRESSION_EVENT_INVALID;
    }
    next.active_target = next.active_target ==
            GP100_EXPRESSION_PEDAL_VOLUME
        ? GP100_EXPRESSION_PEDAL_EFFECT
        : GP100_EXPRESSION_PEDAL_VOLUME;
    toggled_preset = *preset;
    if (!gp100_patch_expression_set_mode(
            &toggled_preset, next.active_target) ||
        !gp100_expression_runtime_apply_adc(
            &toggled_preset, effects, decoded.sample,
            calibration_heel, calibration_toe, &next.apply)) {
        return GP100_EXPRESSION_EVENT_INVALID;
    }
    gp100_preset_update_crc(&toggled_preset);
    *preset = toggled_preset;
    *result = next;
    return GP100_EXPRESSION_EVENT_TARGET_TOGGLED;
}
