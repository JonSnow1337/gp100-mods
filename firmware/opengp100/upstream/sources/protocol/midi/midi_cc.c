#include "midi_cc.h"

#include "midi_message.h"
#include "storage/preset_record.h"
#include "rhythm/rhythm_engine.h"
#include "ui/ui_model.h"

#include <stddef.h>

static bool binary_low_is_on(uint8_t value)
{
    return value < 64u;
}

static bool set_action(
    gp100_midi_cc_action_t *action,
    gp100_midi_cc_target_t target,
    int16_t value,
    uint8_t index)
{
    action->target = target;
    action->value = value;
    action->index = index;
    return true;
}

bool gp100_midi_cc_decode(
    uint8_t controller,
    uint8_t midi_value,
    gp100_midi_cc_action_t *action)
{
    uint16_t scaled;

    if (action == NULL) {
        return false;
    }
    action->target = GP100_MIDI_CC_IGNORED;
    action->value = 0;
    action->index = 0u;
    if (midi_value > 127u) {
        return false;
    }

    if (controller >= 48u && controller <= 56u) {
        return set_action(
            action,
            GP100_MIDI_CC_EFFECT_ENABLE,
            binary_low_is_on(midi_value) ? 1 : 0,
            (uint8_t)(controller - 48u));
    }

    switch (controller) {
    case 7u:
        if (midi_value > 100u) {
            return false;
        }
        return set_action(
            action, GP100_MIDI_CC_OUTPUT_LEVEL,
            (int16_t)midi_value, 0u);
    case 11u:
        scaled = ((uint16_t)midi_value * 255u) >> 7;
        return set_action(
            action, GP100_MIDI_CC_EXPRESSION_VALUE,
            (int16_t)scaled, 0u);
    case 13u:
        return set_action(
            action, GP100_MIDI_CC_EXPRESSION_TARGET,
            (int16_t)(midi_value >> 6), 0u);
    case 24u:
        return set_action(
            action, GP100_MIDI_CC_PRESET_STEP, 1, 0u);
    case 25u:
        return set_action(
            action, GP100_MIDI_CC_PRESET_STEP, -1, 0u);
    case 57u:
        return set_action(
            action, GP100_MIDI_CC_UI_MODE,
            binary_low_is_on(midi_value)
                ? GP100_UI_MODE_TUNER
                : GP100_UI_MODE_PLAY,
            0u);
    case 58u:
        return set_action(
            action, GP100_MIDI_CC_UI_MODE,
            binary_low_is_on(midi_value)
                ? GP100_UI_MODE_DRUM
                : GP100_UI_MODE_PLAY,
            0u);
    case 59u:
        return set_action(
            action, GP100_MIDI_CC_RHYTHM_ENABLE,
            binary_low_is_on(midi_value) ? 1 : 0, 0u);
    case 60u:
        if (midi_value >= GP100_RHYTHM_FACTORY_PATTERN_COUNT) {
            return false;
        }
        return set_action(
            action, GP100_MIDI_CC_RHYTHM_STYLE,
            (int16_t)midi_value, 0u);
    case 61u:
        if (midi_value > 100u) {
            return false;
        }
        return set_action(
            action, GP100_MIDI_CC_RHYTHM_VOLUME,
            (int16_t)midi_value, 0u);
    case 62u:
        return set_action(
            action, GP100_MIDI_CC_UI_MODE,
            binary_low_is_on(midi_value)
                ? GP100_UI_MODE_LOOPER
                : GP100_UI_MODE_PLAY,
            0u);
    case 63u:
        return set_action(
            action, GP100_MIDI_CC_LOOPER_EVENT,
            GP100_LOOPER_RECORD_OR_OVERDUB, 0u);
    case 64u:
        return set_action(
            action, GP100_MIDI_CC_LOOPER_EVENT,
            binary_low_is_on(midi_value)
                ? GP100_LOOPER_STOP
                : GP100_LOOPER_PLAY,
            0u);
    case 65u:
        return set_action(
            action, GP100_MIDI_CC_LOOPER_EVENT,
            GP100_LOOPER_CLEAR, 0u);
    case 66u:
    case 67u:
        if (midi_value > 100u) {
            return false;
        }
        return set_action(
            action,
            controller == 66u
                ? GP100_MIDI_CC_LOOP_RECORD_VOLUME
                : GP100_MIDI_CC_LOOP_VOLUME,
            (int16_t)midi_value, 0u);
    case 68u:
        return set_action(
            action, GP100_MIDI_CC_FOOTSWITCH_MODE,
            binary_low_is_on(midi_value) ? 1 : 0, 0u);
    case 69u:
        return set_action(
            action, GP100_MIDI_CC_VOLUME_PEDAL_POSITION,
            binary_low_is_on(midi_value)
                ? GP100_VOLUME_PEDAL_PRE
                : GP100_VOLUME_PEDAL_POST,
            0u);
    case 70u:
    case 73u:
        scaled = (uint16_t)midi_value + 128u;
        if (scaled > 250u) {
            scaled = 250u;
        }
        return set_action(
            action,
            controller == 70u
                ? GP100_MIDI_CC_TEMPO_BPM
                : GP100_MIDI_CC_RHYTHM_TEMPO_BPM,
            (int16_t)scaled, 0u);
    case 71u:
    case 74u:
        scaled = midi_value < 40u ? 40u : midi_value;
        return set_action(
            action,
            controller == 71u
                ? GP100_MIDI_CC_TEMPO_BPM
                : GP100_MIDI_CC_RHYTHM_TEMPO_BPM,
            (int16_t)scaled, 0u);
    case 72u:
        if (midi_value > 20u) {
            return false;
        }
        return set_action(
            action, GP100_MIDI_CC_INPUT_LEVEL,
            (int16_t)midi_value, 0u);
    default:
        return false;
    }
}

uint16_t gp100_midi_step_preset(
    uint16_t current_preset,
    int16_t step)
{
    if (current_preset >= GP100_MIDI_PRESET_COUNT) {
        current_preset = GP100_MIDI_PRESET_COUNT - 1u;
    }
    if (step > 0) {
        return current_preset + 1u >= GP100_MIDI_PRESET_COUNT
            ? 0u
            : current_preset + 1u;
    }
    if (step < 0) {
        return current_preset == 0u
            ? GP100_MIDI_PRESET_COUNT - 1u
            : current_preset - 1u;
    }
    return current_preset;
}
