#ifndef GP100_RHYTHM_ENGINE_H
#define GP100_RHYTHM_ENGINE_H

#include "storage/settings_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_RHYTHM_BLOCK_SAMPLES 64u
#define GP100_RHYTHM_MAX_VOICES 16u
#define GP100_RHYTHM_PATTERN_EVENTS 200u
#define GP100_RHYTHM_FACTORY_PATTERN_COUNT 100u

typedef struct gp100_rhythm_settings {
    uint16_t pattern_index;
    bool enabled;
    uint16_t tempo_bpm;
    int16_t target_gain_q8;
} gp100_rhythm_settings_t;

typedef struct gp100_rhythm_state {
    int32_t current_gain_q8;
} gp100_rhythm_state_t;

typedef struct gp100_rhythm_hooks {
    bool (*update_sequencer)(
        void *context,
        uint16_t pattern_index,
        bool enabled,
        uint16_t tempo_bpm);
    bool (*render_voices)(
        void *context,
        int32_t *mono_samples,
        size_t sample_count);
    void *context;
} gp100_rhythm_hooks_t;

void gp100_rhythm_reset(gp100_rhythm_state_t *state);

/* Build the live DSP inputs from the recovered persistent fields. */
bool gp100_rhythm_settings_from_record(
    const gp100_settings_record_t *record,
    bool enabled,
    gp100_rhythm_settings_t *settings);

bool gp100_rhythm_process_block(
    gp100_rhythm_state_t *state,
    const gp100_rhythm_settings_t *settings,
    const gp100_rhythm_hooks_t *hooks,
    int32_t *mono_samples,
    size_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
