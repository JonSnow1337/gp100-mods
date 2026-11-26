#ifndef GP100_MIDI_CC_H
#define GP100_MIDI_CC_H

#include <stdbool.h>
#include <stdint.h>

#include "looper/looper_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gp100_midi_cc_target {
    GP100_MIDI_CC_IGNORED = 0,
    GP100_MIDI_CC_MASTER_VOLUME,
    GP100_MIDI_CC_EXPRESSION_VALUE,
    GP100_MIDI_CC_EXPRESSION_TARGET,
    GP100_MIDI_CC_PRESET_STEP,
    GP100_MIDI_CC_EFFECT_ENABLE,
    GP100_MIDI_CC_UI_MODE,
    GP100_MIDI_CC_RHYTHM_ENABLE,
    GP100_MIDI_CC_RHYTHM_STYLE,
    GP100_MIDI_CC_RHYTHM_VOLUME,
    GP100_MIDI_CC_LOOPER_EVENT,
    GP100_MIDI_CC_LOOP_RECORD_VOLUME,
    GP100_MIDI_CC_LOOP_VOLUME,
    GP100_MIDI_CC_FOOTSWITCH_MODE,
    GP100_MIDI_CC_USB_RECORD_SOURCE_LEFT,
    GP100_MIDI_CC_TEMPO_BPM,
    GP100_MIDI_CC_INPUT_LEVEL,
    GP100_MIDI_CC_RHYTHM_TEMPO_BPM,
    GP100_MIDI_CC_VOLUME_PEDAL_POSITION
} gp100_midi_cc_target_t;

/* Compatibility spelling used before setter 0x1cef8 was correlated with the
 * SysEx system Master-volume address. */
#define GP100_MIDI_CC_OUTPUT_LEVEL GP100_MIDI_CC_MASTER_VOLUME

#define GP100_MIDI_CC_OPTION_MODE \
    GP100_MIDI_CC_FOOTSWITCH_MODE
#define GP100_MIDI_CC_UTILITY_LEVEL \
    GP100_MIDI_CC_INPUT_LEVEL
#define GP100_MIDI_CC_APP_MODE GP100_MIDI_CC_UI_MODE
#define GP100_MIDI_CC_OPTION_SELECT \
    GP100_MIDI_CC_VOLUME_PEDAL_POSITION

typedef struct gp100_midi_cc_action {
    gp100_midi_cc_target_t target;
    int16_t value;
    uint8_t index;
} gp100_midi_cc_action_t;

bool gp100_midi_cc_decode(
    uint8_t controller,
    uint8_t midi_value,
    gp100_midi_cc_action_t *action);

uint16_t gp100_midi_step_preset(
    uint16_t current_preset,
    int16_t step);

#ifdef __cplusplus
}
#endif

#endif
