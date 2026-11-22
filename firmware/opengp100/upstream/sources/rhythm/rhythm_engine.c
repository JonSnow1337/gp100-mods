#include "rhythm_engine.h"

#include <limits.h>

void gp100_rhythm_reset(gp100_rhythm_state_t *state)
{
    if (state != NULL) {
        state->current_gain_q8 = 0;
    }
}

bool gp100_rhythm_settings_from_record(
    const gp100_settings_record_t *record,
    bool enabled,
    gp100_rhythm_settings_t *settings)
{
    gp100_rhythm_settings_t next;
    int16_t pattern;
    int16_t tempo;
    int16_t volume;

    if (record == NULL || settings == NULL ||
        !gp100_settings_get_i16(
            record, GP100_SETTINGS_DRUM_STYLE, &pattern) ||
        !gp100_settings_get_i16(
            record, GP100_SETTINGS_DRUM_TEMPO, &tempo) ||
        !gp100_settings_get_i16(
            record, GP100_SETTINGS_DRUM_VOLUME, &volume) ||
        pattern < 0 || (uint16_t)pattern >= GP100_RHYTHM_FACTORY_PATTERN_COUNT ||
        tempo < 40 || tempo > 250 ||
        volume < 0 || volume > 100) {
        return false;
    }
    next.pattern_index = (uint16_t)pattern;
    next.enabled = enabled;
    next.tempo_bpm = (uint16_t)tempo;
    next.target_gain_q8 = volume;
    *settings = next;
    return true;
}

static int32_t clamp_i32(int64_t value)
{
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

bool gp100_rhythm_process_block(
    gp100_rhythm_state_t *state,
    const gp100_rhythm_settings_t *settings,
    const gp100_rhythm_hooks_t *hooks,
    int32_t *mono_samples,
    size_t sample_count)
{
    size_t sample;

    if (state == NULL || settings == NULL || hooks == NULL ||
        hooks->update_sequencer == NULL || hooks->render_voices == NULL ||
        mono_samples == NULL ||
        sample_count != GP100_RHYTHM_BLOCK_SAMPLES) {
        return false;
    }
    if (!hooks->update_sequencer(
            hooks->context,
            settings->pattern_index,
            settings->enabled,
            settings->tempo_bpm) ||
        !hooks->render_voices(
            hooks->context,
            mono_samples,
            sample_count)) {
        return false;
    }

    for (sample = 0u; sample < sample_count; ++sample) {
        if (state->current_gain_q8 < settings->target_gain_q8) {
            ++state->current_gain_q8;
        } else if (state->current_gain_q8 > settings->target_gain_q8) {
            --state->current_gain_q8;
        }
        mono_samples[sample] = clamp_i32(
            ((int64_t)mono_samples[sample] * state->current_gain_q8) >> 8);
    }
    return true;
}
