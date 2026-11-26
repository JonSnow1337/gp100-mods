#include "input/expression_runtime.h"

#include <math.h>
#include <string.h>

int main(void)
{
    gp100_preset_record_t preset;
    gp100_effect_runtime_t effects;
    gp100_expression_apply_result_t result;
    gp100_control_target_t target;
    uint32_t algorithm_id;
    uint16_t position;
    size_t index;
    float expected;

    if (!gp100_expression_runtime_normalize_adc(
            0u, 10u, 210u, &position) || position != 0u ||
        !gp100_expression_runtime_normalize_adc(
            111u, 10u, 210u, &position) || position != 126u ||
        !gp100_expression_runtime_normalize_adc(
            300u, 10u, 210u, &position) || position != 255u ||
        gp100_expression_runtime_normalize_adc(
            10u, 10u, 13u, &position) ||
        gp100_expression_runtime_normalize_adc(
            10u, 10u, 210u, NULL)) {
        return 1;
    }

    memset(&preset, 0, sizeof(preset));
    gp100_effect_runtime_reset(&effects);
    for (index = 0u; index < GP100_CONTROL_TARGET_COUNT; ++index) {
        gp100_control_target_t unassigned = {false, 0, 0u, 0u};
        if (!gp100_preset_set_control_target(
                &preset, index, &unassigned) ||
            !gp100_preset_set_control_range(
                &preset,
                (gp100_control_range_t)(
                    GP100_CONTROL_RANGE_EXPRESSION_TARGET_1 + index),
                0.0f, 99.0f)) {
            return 2;
        }
    }
    if (!gp100_preset_set_control_range(
            &preset, GP100_CONTROL_RANGE_VOLUME, 10.0f, 90.0f) ||
        !gp100_preset_set_expression_pedal_target(
            &preset, GP100_EXPRESSION_PEDAL_VOLUME) ||
        !gp100_preset_set_volume_pedal_position(
            &preset, GP100_VOLUME_PEDAL_POST) ||
        !gp100_expression_runtime_apply_adc(
            &preset, &effects, 210u, 10u, 210u, &result) ||
        !result.volume_changed ||
        result.volume_position != GP100_VOLUME_PEDAL_POST ||
        fabsf(result.volume_percent - (90.0f * 100.0f / 99.0f)) > 0.001f) {
        return 3;
    }

    if (!gp100_effect_algorithm_id(
            GP100_EFFECT_DELAY, 0u, &algorithm_id)) return 4;
    target.assigned = true;
    target.family = GP100_EFFECT_DELAY;
    target.algorithm_id = algorithm_id;
    target.parameter_index = 1u;
    effects.slots[GP100_EFFECT_DELAY].algorithm_loaded = true;
    effects.slots[GP100_EFFECT_DELAY].algorithm_index = 0u;
    effects.slots[GP100_EFFECT_DELAY].parameters[1] = 500.0f;
    if (!gp100_preset_set_control_target(&preset, 0u, &target) ||
        !gp100_preset_set_control_range(
            &preset, GP100_CONTROL_RANGE_EXPRESSION_TARGET_1,
            25.0f, 75.0f) ||
        !gp100_preset_set_expression_pedal_target(
            &preset, GP100_EXPRESSION_PEDAL_EFFECT) ||
        !gp100_expression_runtime_apply(
            &preset, &effects, 127u, 255u, &result)) {
        return 5;
    }
    expected = (20.0f + 25.0f * 3980.0f / 99.0f) +
               (50.0f * 3980.0f / 99.0f) * 127.0f / 255.0f;
    if (result.effect_changed_slots[GP100_EFFECT_DELAY] != 2u ||
        fabsf(effects.slots[GP100_EFFECT_DELAY].parameters[1] -
              expected) > 0.01f ||
        gp100_expression_runtime_apply(
            &preset, &effects, 256u, 255u, &result)) {
        return 6;
    }
    return 0;
}
