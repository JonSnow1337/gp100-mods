#include "flanger.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define FLANGER_SAMPLE_RATE 44100.0f
#define FLANGER_CONTROL_GLIDE 0x1.db8cdap-13f
#define FLANGER_TWO_PI 6.2831855f

static bool parameters_valid(uint8_t depth,
                             float rate,
                             uint8_t feedback,
                             uint8_t bias,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    bool rate_valid = isfinite(rate) &&
        (tempo_sync ? rate >= 0.0f && rate <= 10.0f
                    : rate >= 0.1f && rate <= 10.0f);

    return depth <= 99u && feedback <= 99u && bias <= 99u && rate_valid &&
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
    return rate / FLANGER_SAMPLE_RATE;
}

static float delay_sample(gp100_flanger_t *flanger,
                          size_t channel,
                          float input,
                          float delay_samples)
{
    size_t integer_delay = (size_t)delay_samples;
    float fraction = delay_samples - (float)integer_delay;
    size_t newer;
    size_t older;

    flanger->delay[channel][flanger->write_index[channel]] = input;
    flanger->write_index[channel]++;
    if (flanger->write_index[channel] == GP100_FLANGER_DELAY_SAMPLES) {
        flanger->write_index[channel] = 0u;
    }
    newer = (flanger->write_index[channel] +
             GP100_FLANGER_DELAY_SAMPLES - integer_delay) %
            GP100_FLANGER_DELAY_SAMPLES;
    older = newer == 0u ? GP100_FLANGER_DELAY_SAMPLES - 1u
                        : newer - 1u;
    return flanger->delay[channel][older] * fraction +
           flanger->delay[channel][newer] * (1.0f - fraction);
}

static void process_sample(gp100_flanger_t *flanger,
                           float input,
                           float *left,
                           float *right)
{
    float sine = sinf(flanger->current_rate_step * FLANGER_TWO_PI *
                      (float)flanger->lfo_sample);
    float base_delay = 5.0f + flanger->current_feedback * 105.0f;
    float sweep = 13.0f + flanger->current_depth * 119.0f;
    float delay_left = base_delay + sweep * (sine + 1.0f) * 0.5f;
    float delay_right = base_delay + sweep * (1.0f - sine) * 0.5f;

    flanger->lfo_sample++;
    if (flanger->current_rate_step > 0.0f &&
        (float)flanger->lfo_sample >= 1.0f / flanger->current_rate_step) {
        flanger->lfo_sample = 0u;
    }
    flanger->last_tap[0] = delay_sample(
        flanger, 0u,
        input + flanger->last_tap[0] * flanger->current_bias,
        delay_left);
    flanger->last_tap[1] = delay_sample(
        flanger, 1u,
        input + flanger->last_tap[1] * flanger->current_bias,
        delay_right);
    *left = input * 0.8f + flanger->last_tap[0] * 0.5f;
    *right = input * 0.8f + flanger->last_tap[1] * 0.5f;
}

bool gp100_flanger_reset(gp100_flanger_t *flanger,
                              uint8_t depth,
                              float rate,
                              uint8_t feedback,
                              uint8_t bias,
                              bool tempo_sync)
{
    if (flanger == NULL ||
        !parameters_valid(depth, rate, feedback, bias, tempo_sync, 1u)) {
        return false;
    }
    memset(flanger, 0, sizeof(*flanger));
    flanger->target_depth = (float)depth * 0.01f;
    flanger->current_depth = flanger->target_depth;
    flanger->target_feedback = (float)feedback * 0.01f;
    flanger->current_feedback = flanger->target_feedback;
    flanger->target_bias = (float)bias * 0.01f;
    flanger->current_bias = flanger->target_bias;
    flanger->rate_selector = (uint8_t)rate;
    flanger->tempo_sync = tempo_sync;
    flanger->configured = true;
    return true;
}

bool gp100_flanger_process(gp100_flanger_t *flanger,
                                uint8_t depth,
                                float rate,
                                uint8_t feedback,
                                uint8_t bias,
                                bool tempo_sync,
                                uint16_t tempo_period_ms,
                                float *interleaved_stereo,
                                size_t frame_count,
                                bool enabled)
{
    float depth_delta;
    float rate_delta;
    float feedback_delta;
    float bias_delta;
    size_t frame;

    if (flanger == NULL || !flanger->configured ||
        !parameters_valid(depth, rate, feedback, bias, tempo_sync,
                          tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    flanger->target_depth = (float)depth * 0.01f;
    flanger->target_rate_step = rate_step(rate, tempo_sync, tempo_period_ms);
    flanger->target_feedback = (float)feedback * 0.01f;
    flanger->target_bias = (float)bias * 0.01f;
    flanger->rate_selector = (uint8_t)rate;
    flanger->tempo_sync = tempo_sync;
    depth_delta = (flanger->target_depth - flanger->current_depth) *
                  FLANGER_CONTROL_GLIDE;
    rate_delta = (flanger->target_rate_step - flanger->current_rate_step) *
                 FLANGER_CONTROL_GLIDE;
    feedback_delta =
        (flanger->target_feedback - flanger->current_feedback) *
        FLANGER_CONTROL_GLIDE;
    bias_delta = (flanger->target_bias - flanger->current_bias) *
                 FLANGER_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float left;
        float right;

        if (!enabled) {
            continue;
        }
        flanger->current_depth += depth_delta;
        flanger->current_rate_step += rate_delta;
        flanger->current_feedback += feedback_delta;
        flanger->current_bias += bias_delta;
        process_sample(flanger, interleaved_stereo[frame * 2u],
                       &left, &right);
        interleaved_stereo[frame * 2u] = left;
        interleaved_stereo[frame * 2u + 1u] = right;
    }
    return true;
}
