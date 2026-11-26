#include "input/expression_event.h"

#include <math.h>
#include <string.h>

int main(void)
{
    gp100_preset_record_t preset;
    gp100_effect_runtime_t effects;
    gp100_expression_event_result_t result;
    gp100_event_t event;
    gp100_expression_pedal_target_t target;
    uint32_t crc;

    memset(&preset, 0, sizeof(preset));
    gp100_effect_runtime_reset(&effects);
    if (!gp100_preset_set_control_range(
            &preset, GP100_CONTROL_RANGE_VOLUME, 0.0f, 99.0f) ||
        !gp100_preset_set_expression_pedal_target(
            &preset, GP100_EXPRESSION_PEDAL_VOLUME) ||
        !gp100_preset_set_volume_pedal_position(
            &preset, GP100_VOLUME_PEDAL_PRE)) {
        return 1;
    }

    event.value = 0u;
    event.code = GP100_EXPRESSION_POSITION_EVENT_PREFIX | 111u;
    if (gp100_expression_event_handle(
            GP100_UI_MODE_PLAY, &preset, &effects,
            10u, 210u, &event, &result) !=
            GP100_EXPRESSION_EVENT_POSITION ||
        result.active_target != GP100_EXPRESSION_PEDAL_VOLUME ||
        !result.apply.volume_changed ||
        result.apply.volume_position != GP100_VOLUME_PEDAL_PRE ||
        fabsf(result.apply.volume_percent -
              (126.0f * 100.0f / 255.0f)) > 0.001f) {
        return 2;
    }

    event.value = 1u;
    event.code = GP100_EXPRESSION_ACTIVATE_EVENT_PREFIX | 220u;
    if (gp100_expression_event_handle(
            GP100_UI_MODE_EDIT, &preset, &effects,
            10u, 210u, &event, &result) !=
            GP100_EXPRESSION_EVENT_TARGET_TOGGLED ||
        result.active_target != GP100_EXPRESSION_PEDAL_EFFECT ||
        !gp100_preset_expression_pedal_target(&preset, &target) ||
        target != GP100_EXPRESSION_PEDAL_EFFECT) {
        return 3;
    }
    memcpy(&crc, &preset.bytes[GP100_PRESET_CRC_OFFSET], sizeof(crc));
    if (crc != gp100_crc32(
            preset.bytes, GP100_PRESET_CRC_OFFSET)) {
        return 4;
    }

    event.value = 0u;
    event.code = GP100_EXPRESSION_POSITION_EVENT_PREFIX | 111u;
    if (gp100_expression_event_handle(
            GP100_UI_MODE_DRUM, &preset, &effects,
            10u, 210u, &event, &result) !=
            GP100_EXPRESSION_EVENT_POSITION ||
        gp100_expression_event_handle(
            GP100_UI_MODE_TUNER, &preset, &effects,
            10u, 210u, &event, &result) !=
            GP100_EXPRESSION_EVENT_POSITION ||
        gp100_expression_event_handle(
            GP100_UI_MODE_GLOBAL, &preset, &effects,
            10u, 210u, &event, &result) !=
            GP100_EXPRESSION_EVENT_POSITION ||
        gp100_expression_event_handle(
            GP100_UI_MODE_HARDWARE_TEST, &preset, &effects,
            10u, 210u, &event, &result) !=
            GP100_EXPRESSION_EVENT_UNHANDLED ||
        gp100_expression_event_handle(
            GP100_UI_MODE_PLAY, &preset, &effects,
            10u, 13u, &(gp100_event_t){
                0u, GP100_EXPRESSION_POSITION_EVENT_PREFIX | 10u},
            &result) != GP100_EXPRESSION_EVENT_INVALID ||
        gp100_expression_event_handle(
            GP100_UI_MODE_PLAY, NULL, &effects,
            10u, 210u, &event, &result) !=
            GP100_EXPRESSION_EVENT_INVALID) {
        return 5;
    }
    event.value = 1u;
    event.code = GP100_EXPRESSION_ACTIVATE_EVENT_PREFIX | 220u;
    if (gp100_expression_event_handle(
            GP100_UI_MODE_PLAY, &preset, &effects,
            10u, 210u, &event, &result) !=
            GP100_EXPRESSION_EVENT_TARGET_TOGGLED ||
        result.active_target != GP100_EXPRESSION_PEDAL_VOLUME ||
        !result.apply.volume_changed ||
        fabsf(result.apply.volume_percent - 100.0f) > 0.001f) {
        return 6;
    }
    return 0;
}
