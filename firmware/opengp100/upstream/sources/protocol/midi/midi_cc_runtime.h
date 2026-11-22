#ifndef GP100_MIDI_CC_RUNTIME_H
#define GP100_MIDI_CC_RUNTIME_H

#include "audio/effects/effect_runtime.h"
#include "input/expression_runtime.h"
#include "looper/looper_engine.h"
#include "midi_cc.h"
#include "rhythm/rhythm_engine.h"
#include "ui/ui_runtime.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_midi_cc_runtime {
    gp100_ui_runtime_t *ui;
    gp100_preset_record_t *preset;
    gp100_settings_record_t *settings;
    gp100_looper_engine_t *looper;
    gp100_effect_runtime_t *effects;
    gp100_rhythm_settings_t *rhythm;
} gp100_midi_cc_runtime_t;

typedef struct gp100_midi_cc_apply_result {
    uint32_t ui_updates;
    uint32_t effect_actions[GP100_EFFECT_FAMILY_COUNT];
    uint16_t effect_changed_slots[GP100_EFFECT_FAMILY_COUNT];
    bool master_volume_changed;
    uint8_t master_volume_percent;
    bool volume_pedal_changed;
    bool rhythm_changed;
    float volume_pedal_percent;
    gp100_volume_pedal_position_t volume_pedal_position;
} gp100_midi_cc_apply_result_t;

/*
 * Apply one already-decoded CC action transactionally to the portable live
 * state. volume_pedal_percent remains an explicit board/audio-pipeline output
 * because PRE and POST pedal placement are outside the preset-output stage.
 */
bool gp100_midi_cc_apply_action(
    const gp100_midi_cc_action_t *action,
    gp100_midi_cc_runtime_t *runtime,
    gp100_midi_cc_apply_result_t *result);

bool gp100_midi_cc_decode_and_apply(
    uint8_t controller,
    uint8_t midi_value,
    gp100_midi_cc_runtime_t *runtime,
    gp100_midi_cc_apply_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
