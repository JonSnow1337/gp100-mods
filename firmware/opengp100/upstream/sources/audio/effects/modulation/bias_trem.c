#include "bias_trem.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define BIAS_TREM_SAMPLE_RATE 44100.0f
#define BIAS_TREM_CONTROL_GLIDE 0x1.db8cdap-13f
#define BIAS_TREM_TWO_PI 6.2831855f

static bool parameters_valid(uint8_t depth,
                             float rate,
                             uint8_t level,
                             bool tempo_sync,
                             uint8_t bias,
                             uint16_t tempo_period_ms)
{
    bool rate_valid = isfinite(rate) &&
        (tempo_sync ? rate >= 0.0f && rate <= 10.0f
                    : rate >= 0.1f && rate <= 10.0f);

    return depth <= 99u && level <= 99u && bias <= 99u && rate_valid &&
           (!tempo_sync || tempo_period_ms != 0u);
}

static float rate_step(float rate,
                       bool tempo_sync,
                       uint16_t tempo_period_ms)
{
    if (tempo_sync) {
        return 1.0f / ((float)tempo_period_ms * 44.1f *
                       gp100_effect_tempo_beats_for_rate(rate));
    }
    return rate / BIAS_TREM_SAMPLE_RATE;
}

static float output_filter(gp100_bias_trem_filter_t *filter,
                           float input)
{
    const float b0 = 0x1.0774e4p+1f;
    const float b1 = -0x1.da4f5p+1f;
    const float b2 = 0x1.aa2b04p+0f;
    const float a0 = -0x1.0774e6p+1f;
    const float a1 = 0x1.da4f5p+1f;
    const float a2 = -0x1.aa2b04p+0f;
    float output = (input * b0 + filter->previous_input[0] * b1 +
                    filter->previous_input[1] * b2 -
                    filter->previous_output[0] * a1 -
                    filter->previous_output[1] * a2) / a0;

    filter->previous_input[1] = filter->previous_input[0];
    filter->previous_input[0] = input;
    filter->previous_output[1] = filter->previous_output[0];
    filter->previous_output[0] = output;
    return output;
}

bool gp100_bias_trem_reset(gp100_bias_trem_t *tremolo,
                                uint8_t depth,
                                float rate,
                                uint8_t level,
                                bool tempo_sync,
                                uint8_t bias)
{
    if (tremolo == NULL ||
        !parameters_valid(depth, rate, level, tempo_sync, bias, 1u)) {
        return false;
    }
    memset(tremolo, 0, sizeof(*tremolo));
    tremolo->target_depth = (float)depth * 0.01f;
    tremolo->current_depth = tremolo->target_depth;
    tremolo->target_level = (float)level * 0.01f;
    tremolo->current_level = tremolo->target_level;
    tremolo->lfo_limit = (1.0f - (float)bias * 0.01f) * 0.75f + 0.25f;
    tremolo->rate_selector = (uint8_t)rate;
    tremolo->tempo_sync = tempo_sync;
    tremolo->configured = true;
    return true;
}

bool gp100_bias_trem_process(gp100_bias_trem_t *tremolo,
                                  uint8_t depth,
                                  float rate,
                                  uint8_t level,
                                  bool tempo_sync,
                                  uint8_t bias,
                                  uint16_t tempo_period_ms,
                                  float *interleaved_stereo,
                                  size_t frame_count,
                                  bool enabled)
{
    float depth_delta;
    float rate_delta;
    float level_delta;
    size_t frame;

    if (tremolo == NULL || !tremolo->configured ||
        !parameters_valid(depth, rate, level, tempo_sync, bias,
                          tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    tremolo->target_depth = (float)depth * 0.01f;
    tremolo->target_rate_step = rate_step(rate, tempo_sync, tempo_period_ms);
    tremolo->target_level = (float)level * 0.01f;
    tremolo->lfo_limit = (1.0f - (float)bias * 0.01f) * 0.75f + 0.25f;
    tremolo->rate_selector = (uint8_t)rate;
    tremolo->tempo_sync = tempo_sync;
    depth_delta = (tremolo->target_depth - tremolo->current_depth) *
                  BIAS_TREM_CONTROL_GLIDE;
    rate_delta = (tremolo->target_rate_step - tremolo->current_rate_step) *
                 BIAS_TREM_CONTROL_GLIDE;
    level_delta = (tremolo->target_level - tremolo->current_level) *
                  BIAS_TREM_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float lfo;
        float modulated;

        if (!enabled) {
            continue;
        }
        tremolo->current_depth += depth_delta;
        tremolo->current_rate_step += rate_delta;
        tremolo->current_level += level_delta;
        input = interleaved_stereo[frame * 2u];
        lfo = sinf(tremolo->current_rate_step * BIAS_TREM_TWO_PI *
                    tremolo->lfo_sample) > 0.0f ? 1.0f : -1.0f;
        tremolo->square_sum +=
            lfo - tremolo->square_history[tremolo->square_index];
        tremolo->square_history[tremolo->square_index] = lfo;
        tremolo->square_index++;
        if (tremolo->square_index == 50u) {
            tremolo->square_index = 0u;
        }
        lfo = tremolo->square_sum / 50.0f;
        tremolo->lfo_sample += 1.0f;
        if (tremolo->current_rate_step > 0.0f &&
            tremolo->lfo_sample >= 1.0f / tremolo->current_rate_step) {
            tremolo->lfo_sample = 0.0f;
        }
        if (lfo > tremolo->lfo_limit) {
            lfo = tremolo->lfo_limit;
        } else if (lfo < -tremolo->lfo_limit) {
            lfo = -tremolo->lfo_limit;
        }
        lfo /= tremolo->lfo_limit;
        modulated = ((1.0f - tremolo->current_depth) * input +
                     input * (lfo + 1.0f) * 0.5f *
                         tremolo->current_depth) * tremolo->current_level;
        interleaved_stereo[frame * 2u] =
            output_filter(&tremolo->output_filter[0], modulated) * 2.0f;
        interleaved_stereo[frame * 2u + 1u] =
            output_filter(&tremolo->output_filter[1], modulated) * 2.0f;
    }
    return true;
}
