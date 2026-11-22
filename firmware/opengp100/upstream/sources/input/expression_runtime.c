#include "expression_runtime.h"
#include "storage/patch_chain.h"

#include <math.h>
#include <string.h>

bool gp100_expression_runtime_normalize_adc(
    uint16_t raw_position,
    uint16_t heel,
    uint16_t toe,
    uint16_t *position)
{
    uint32_t active_heel = (uint32_t)heel + 3u;
    uint32_t clamped;

    if (position == NULL || active_heel >= toe) {
        return false;
    }
    clamped = raw_position;
    if (clamped < active_heel) clamped = active_heel;
    if (clamped > toe) clamped = toe;
    *position = (uint16_t)(
        ((clamped - active_heel) * UINT32_C(255)) /
        ((uint32_t)toe - active_heel));
    return true;
}

bool gp100_expression_runtime_apply_adc(
    const gp100_preset_record_t *preset,
    gp100_effect_runtime_t *effects,
    uint16_t raw_position,
    uint16_t heel,
    uint16_t toe,
    gp100_expression_apply_result_t *result)
{
    uint16_t position;

    return gp100_expression_runtime_normalize_adc(
               raw_position, heel, toe, &position) &&
           gp100_expression_runtime_apply(
               preset, effects, position, 255u, result);
}

bool gp100_expression_runtime_apply(
    const gp100_preset_record_t *preset,
    gp100_effect_runtime_t *effects,
    uint16_t position,
    uint16_t full_scale,
    gp100_expression_apply_result_t *result)
{
    gp100_expression_pedal_target_t pedal_target;
    gp100_expression_apply_result_t next;
    gp100_effect_runtime_t next_effects;
    float normalized;
    size_t target_index;

    if (preset == NULL || effects == NULL || result == NULL ||
        full_scale == 0u || position > full_scale ||
        !gp100_preset_expression_pedal_target(
            preset, &pedal_target)) {
        return false;
    }
    memset(&next, 0, sizeof(next));
    next_effects = *effects;
    normalized = (float)position / (float)full_scale;
    gp100_patch_expression_t assignment;
    gp100_patch_block_t block;
    const bool native = gp100_patch_is_native(preset);
    const bool assigned = native && gp100_patch_expression_get(preset, &assignment, &block);

    if (pedal_target == GP100_EXPRESSION_PEDAL_VOLUME || (native && !assigned)) {
        float minimum;
        float maximum;

        if (!gp100_preset_control_range(
                preset, GP100_CONTROL_RANGE_VOLUME,
                &minimum, &maximum) ||
            !gp100_preset_volume_pedal_position(
                preset, &next.volume_position)) {
            return false;
        }
        next.volume_percent =
            (minimum + (maximum - minimum) * normalized) * 100.0f / 99.0f;
        if (next.volume_percent > 100.0f) next.volume_percent = 100.0f;
        next.volume_changed = true;
        if (assigned) {
            /* Restore the knob and any parameters affected by its selector. */
            memcpy(next_effects.slots[block.id].parameters, block.parameters,
                   sizeof(block.parameters));
        }
    } else if (assigned) {
        gp100_effect_slot_state_t *slot = &next_effects.slots[block.id];
        gp100_parameter_rule_t rule;
        if (!slot->algorithm_loaded || slot->instance_id != block.id ||
            slot->family != block.family || slot->algorithm_index != block.algorithm ||
            !gp100_effect_parameter_rule(block.family, block.algorithm,
                 assignment.parameter, block.parameters, &rule)) {
            return false;
        }
        float value = assignment.heel + (assignment.toe - assignment.heel) * normalized;
        if (rule.minimum == 0.0f && rule.maximum <= 10.0f) {
            value = roundf(value);
        }
        value = fminf(fmaxf(value, rule.minimum), rule.maximum);
        if (!gp100_effect_apply_parameter_change(block.family, block.algorithm,
                assignment.parameter, value, slot->parameters,
                &next.effect_changed_slots[block.id])) {
            return false;
        }
    } else {
        for (target_index = 0u;
             target_index < GP100_CONTROL_TARGET_COUNT;
             ++target_index) {
            gp100_control_target_t target;
            gp100_control_update_result_t update;
            gp100_parameter_rule_t rule;
            int algorithm_index;
            float minimum;
            float maximum;
            float physical_minimum;
            float physical_maximum;
            float value;
            uint16_t changed_slots = 0u;

            if (!gp100_preset_control_target(
                    preset, target_index, &target) ||
                !gp100_preset_control_range(
                    preset,
                    (gp100_control_range_t)(
                        GP100_CONTROL_RANGE_EXPRESSION_TARGET_1 +
                        target_index),
                    &minimum, &maximum)) {
                return false;
            }
            if (!target.assigned) continue;
            algorithm_index = gp100_effect_algorithm_index(
                target.family, target.algorithm_id);
            if (algorithm_index < 0 ||
                !gp100_effect_parameter_rule(
                    target.family, (size_t)algorithm_index,
                    target.parameter_index,
                    next_effects.slots[(size_t)target.family].parameters,
                    &rule)) {
                return false;
            }
            physical_minimum = rule.minimum +
                minimum * (rule.maximum - rule.minimum) / 99.0f;
            physical_maximum = rule.minimum +
                maximum * (rule.maximum - rule.minimum) / 99.0f;
            value = physical_minimum +
                    (physical_maximum - physical_minimum) * normalized;
            if (value < rule.minimum) value = rule.minimum;
            if (value > rule.maximum) value = rule.maximum;
            update = gp100_effect_runtime_set_control_target(
                &next_effects, preset, target_index, value,
                &changed_slots);
            if (update == GP100_CONTROL_UPDATE_INVALID) return false;
            if (update == GP100_CONTROL_UPDATE_APPLIED) {
                next.effect_changed_slots[(size_t)target.family] |=
                    changed_slots;
            }
        }
    }
    if (assigned) {
        next_effects.slots[block.id].enabled = block.enabled;
    }
    *effects = next_effects;
    *result = next;
    return true;
}
