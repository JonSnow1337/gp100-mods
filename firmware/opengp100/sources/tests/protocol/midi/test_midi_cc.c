#include "protocol/midi/midi_cc.h"
#include "storage/preset_record.h"
#include "ui/ui_model.h"

#include <stdio.h>

static int expect_action(
    uint8_t controller,
    uint8_t midi_value,
    gp100_midi_cc_target_t target,
    int16_t value,
    uint8_t index)
{
    gp100_midi_cc_action_t action;

    if (!gp100_midi_cc_decode(controller, midi_value, &action) ||
        action.target != target || action.value != value ||
        action.index != index) {
        return 1;
    }
    return 0;
}

static int test_common_controls(void)
{
    if (expect_action(7u, 100u, GP100_MIDI_CC_OUTPUT_LEVEL,
                      100, 0u) != 0 ||
        expect_action(11u, 127u, GP100_MIDI_CC_EXPRESSION_VALUE,
                      253, 0u) != 0 ||
        expect_action(13u, 64u, GP100_MIDI_CC_EXPRESSION_TARGET,
                      1, 0u) != 0 ||
        expect_action(24u, 0u, GP100_MIDI_CC_PRESET_STEP,
                      1, 0u) != 0 ||
        expect_action(25u, 127u, GP100_MIDI_CC_PRESET_STEP,
                      -1, 0u) != 0 ||
        expect_action(48u, 0u, GP100_MIDI_CC_EFFECT_ENABLE,
                      1, 0u) != 0 ||
        expect_action(56u, 64u, GP100_MIDI_CC_EFFECT_ENABLE,
                      0, 8u) != 0) {
        return 1;
    }
    return 0;
}

static int test_rhythm_looper_and_tempo(void)
{
    if (expect_action(57u, 0u, GP100_MIDI_CC_UI_MODE,
                      GP100_UI_MODE_TUNER, 0u) != 0 ||
        expect_action(57u, 127u, GP100_MIDI_CC_UI_MODE,
                      GP100_UI_MODE_PLAY, 0u) != 0 ||
        expect_action(58u, 0u, GP100_MIDI_CC_UI_MODE,
                      GP100_UI_MODE_DRUM, 0u) != 0 ||
        expect_action(59u, 0u, GP100_MIDI_CC_RHYTHM_ENABLE,
                      1, 0u) != 0 ||
        expect_action(60u, 88u, GP100_MIDI_CC_RHYTHM_STYLE,
                      88, 0u) != 0 ||
        expect_action(61u, 100u, GP100_MIDI_CC_RHYTHM_VOLUME,
                      100, 0u) != 0 ||
        expect_action(62u, 0u, GP100_MIDI_CC_UI_MODE,
                      GP100_UI_MODE_LOOPER, 0u) != 0 ||
        expect_action(63u, 127u, GP100_MIDI_CC_LOOPER_EVENT,
                      GP100_LOOPER_RECORD_OR_OVERDUB, 0u) != 0 ||
        expect_action(64u, 0u, GP100_MIDI_CC_LOOPER_EVENT,
                      GP100_LOOPER_STOP, 0u) != 0 ||
        expect_action(64u, 127u, GP100_MIDI_CC_LOOPER_EVENT,
                      GP100_LOOPER_PLAY, 0u) != 0 ||
        expect_action(65u, 0u, GP100_MIDI_CC_LOOPER_EVENT,
                      GP100_LOOPER_CLEAR, 0u) != 0 ||
        expect_action(69u, 63u, GP100_MIDI_CC_VOLUME_PEDAL_POSITION,
                      GP100_VOLUME_PEDAL_PRE, 0u) != 0 ||
        expect_action(69u, 64u, GP100_MIDI_CC_VOLUME_PEDAL_POSITION,
                      GP100_VOLUME_PEDAL_POST, 0u) != 0 ||
        expect_action(70u, 127u, GP100_MIDI_CC_TEMPO_BPM,
                      250, 0u) != 0 ||
        expect_action(71u, 0u, GP100_MIDI_CC_TEMPO_BPM,
                      40, 0u) != 0 ||
        expect_action(73u, 72u, GP100_MIDI_CC_RHYTHM_TEMPO_BPM,
                      200, 0u) != 0 ||
        expect_action(74u, 100u, GP100_MIDI_CC_RHYTHM_TEMPO_BPM,
                      100, 0u) != 0) {
        return 1;
    }
    return 0;
}

static int test_ranges_and_steps(void)
{
    gp100_midi_cc_action_t action;

    if (gp100_midi_cc_decode(7u, 101u, &action) ||
        gp100_midi_cc_decode(72u, 21u, &action) ||
        gp100_midi_cc_decode(1u, 0u, &action) ||
        gp100_midi_cc_decode(7u, 128u, &action) ||
        gp100_midi_step_preset(197u, 1) != 0u ||
        gp100_midi_step_preset(0u, -1) != 197u ||
        gp100_midi_step_preset(90u, 1) != 91u) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_common_controls();
    if (result != 0) {
        fprintf(stderr, "common-control test failed: %d\n", result);
        return 10 + result;
    }
    result = test_rhythm_looper_and_tempo();
    if (result != 0) {
        fprintf(stderr, "rhythm/looper/tempo test failed: %d\n", result);
        return 20 + result;
    }
    result = test_ranges_and_steps();
    if (result != 0) {
        fprintf(stderr, "range/step test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
