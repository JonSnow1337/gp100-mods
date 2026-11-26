#include "opto_trem.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define OPTO_TREM_SAMPLE_RATE 44100.0f
#define OPTO_TREM_CONTROL_GLIDE 0x1.db8cdap-13f
#define OPTO_TREM_FOUR_PI 12.566371f
#define OPTO_TREM_TWO_OVER_PI 0.63661975f

static bool parameters_valid(uint8_t depth,
                             float rate,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    bool rate_valid = isfinite(rate) &&
        (tempo_sync ? rate >= 0.0f && rate <= 10.0f
                    : rate >= 0.1f && rate <= 10.0f);

    return depth <= 99u && rate_valid &&
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
    return rate / OPTO_TREM_SAMPLE_RATE;
}

static float opto_lfo(float rate_step_value, uint32_t sample)
{
    float triangle = asinf(sinf(rate_step_value * OPTO_TREM_FOUR_PI *
                                (float)sample)) * OPTO_TREM_TWO_OVER_PI;

    if (triangle > 0.5f) {
        return 0.5f;
    }
    if (triangle < -0.5f) {
        return -0.5f;
    }
    return triangle;
}

bool gp100_opto_trem_reset(gp100_opto_trem_t *tremolo,
                                uint8_t depth,
                                float rate,
                                bool tempo_sync)
{
    if (tremolo == NULL ||
        !parameters_valid(depth, rate, tempo_sync, 1u)) {
        return false;
    }
    memset(tremolo, 0, sizeof(*tremolo));
    tremolo->target_depth = (float)depth * 0.01f;
    tremolo->current_depth = tremolo->target_depth;
    tremolo->rate_selector = (uint8_t)rate;
    tremolo->tempo_sync = tempo_sync;
    tremolo->configured = true;
    return true;
}

bool gp100_opto_trem_process(gp100_opto_trem_t *tremolo,
                                  uint8_t depth,
                                  float rate,
                                  bool tempo_sync,
                                  uint16_t tempo_period_ms,
                                  float *interleaved_stereo,
                                  size_t frame_count,
                                  bool enabled)
{
    float depth_delta;
    float rate_delta;
    size_t frame;

    if (tremolo == NULL || !tremolo->configured ||
        !parameters_valid(depth, rate, tempo_sync, tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    tremolo->target_depth = (float)depth * 0.01f;
    tremolo->target_rate_step = rate_step(rate, tempo_sync, tempo_period_ms);
    tremolo->rate_selector = (uint8_t)rate;
    tremolo->tempo_sync = tempo_sync;
    depth_delta = (tremolo->target_depth - tremolo->current_depth) *
                  OPTO_TREM_CONTROL_GLIDE;
    rate_delta = (tremolo->target_rate_step - tremolo->current_rate_step) *
                 OPTO_TREM_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float lfo;
        float dry;

        if (!enabled) {
            continue;
        }
        tremolo->current_depth += depth_delta;
        tremolo->current_rate_step += rate_delta;
        input = interleaved_stereo[frame * 2u];
        lfo = opto_lfo(tremolo->current_rate_step, tremolo->lfo_sample);
        tremolo->lfo_sample++;
        if (tremolo->current_rate_step > 0.0f &&
            (float)tremolo->lfo_sample >=
                1.0f / tremolo->current_rate_step) {
            tremolo->lfo_sample = 0u;
        }
        dry = (1.0f - tremolo->current_depth) * input;
        interleaved_stereo[frame * 2u] =
            dry + (lfo + 0.5f) * input * tremolo->current_depth;
        interleaved_stereo[frame * 2u + 1u] =
            dry + (0.5f - lfo) * input * tremolo->current_depth;
    }
    return true;
}
