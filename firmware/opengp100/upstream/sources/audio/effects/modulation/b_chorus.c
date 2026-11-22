#include "b_chorus.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define B_CHORUS_SAMPLE_RATE 44100.0f
#define B_CHORUS_CONTROL_GLIDE 0x1.db8cdap-13f
#define B_CHORUS_FOUR_PI 12.566371f
#define B_CHORUS_TWO_OVER_PI 0.63661975f

typedef struct b_chorus_iir3_coefficients {
    float b[4];
    float a[3];
} b_chorus_iir3_coefficients_t;

static const b_chorus_iir3_coefficients_t chorus_coefficients = {
    {0.044904191f, 0.134712562f, 0.134712562f, 0.044904191f},
    {-1.22987509f, 0.744768083f, -0.155659422f}
};

static bool parameters_valid(uint8_t depth,
                             float rate,
                             uint8_t tone,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    bool rate_valid = isfinite(rate) &&
        (tempo_sync ? rate >= 0.0f && rate <= 10.0f
                    : rate >= 0.1f && rate <= 10.0f);

    return depth <= 99u && tone <= 99u && rate_valid &&
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
    return rate / B_CHORUS_SAMPLE_RATE;
}

static float input_filter(gp100_b_chorus_iir1_t *state, float input)
{
    float output = input * 0.998577297f + state->x1 * -0.998577297f -
                   state->y1 * -0.997154534f;

    state->x1 = input;
    state->y1 = output;
    return output;
}

static float iir3(gp100_b_chorus_iir3_t *state,
                  const b_chorus_iir3_coefficients_t *coefficients,
                  float input)
{
    float output = input * coefficients->b[0] +
                   state->x[0] * coefficients->b[1] +
                   state->x[1] * coefficients->b[2] +
                   state->x[2] * coefficients->b[3] -
                   state->y[0] * coefficients->a[0] -
                   state->y[1] * coefficients->a[1] -
                   state->y[2] * coefficients->a[2];

    state->x[2] = state->x[1];
    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[2] = state->y[1];
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static float delay_sample(gp100_b_chorus_t *chorus,
                          size_t channel,
                          float input,
                          float delay_samples)
{
    size_t integer_delay = (size_t)delay_samples;
    float fraction = delay_samples - (float)integer_delay;
    size_t index;
    size_t previous;
    float newer;
    float older;

    chorus->delay[channel][chorus->write_index[channel]] = input;
    chorus->write_index[channel]++;
    if (chorus->write_index[channel] == GP100_B_CHORUS_DELAY_SAMPLES) {
        chorus->write_index[channel] = 0u;
    }
    index = (chorus->write_index[channel] +
             GP100_B_CHORUS_DELAY_SAMPLES - integer_delay) %
            GP100_B_CHORUS_DELAY_SAMPLES;
    previous = index == 0u ? GP100_B_CHORUS_DELAY_SAMPLES - 1u
                           : index - 1u;
    newer = chorus->delay[channel][index];
    older = chorus->delay[channel][previous];
    return older * fraction + newer * (1.0f - fraction);
}

static void process_sample(gp100_b_chorus_t *chorus,
                           float input,
                           float *left,
                           float *right)
{
    float filtered = input_filter(&chorus->input_filter, input);
    float delay_input = iir3(&chorus->pre_delay_filter,
                             &chorus_coefficients, filtered);
    float triangle = asinf(sinf(chorus->current_rate_step * B_CHORUS_FOUR_PI *
                                (float)chorus->lfo_sample)) *
                     B_CHORUS_TWO_OVER_PI;
    float phase[2] = {(triangle + 1.0f) * 0.5f,
                      (1.0f - triangle) * 0.5f};
    float output[2];
    size_t channel;

    chorus->lfo_sample++;
    if (chorus->current_rate_step > 0.0f &&
        (float)chorus->lfo_sample >= 1.0f / chorus->current_rate_step) {
        chorus->lfo_sample = 0u;
    }
    for (channel = 0u; channel < 2u; ++channel) {
        float delay_time = 154.0f + phase[channel] * 91.0f *
                           chorus->current_depth;
        float wet = delay_sample(chorus, channel, delay_input, delay_time);

        wet = iir3(&chorus->output_filter[channel],
                   &chorus_coefficients, wet);
        output[channel] = input + wet * chorus->current_tone;
    }
    *left = output[0];
    *right = output[1];
}

bool gp100_b_chorus_reset(gp100_b_chorus_t *chorus,
                               uint8_t depth,
                               float rate,
                               uint8_t tone,
                               bool tempo_sync)
{
    if (chorus == NULL ||
        !parameters_valid(depth, rate, tone, tempo_sync, 1u)) {
        return false;
    }
    memset(chorus, 0, sizeof(*chorus));
    chorus->target_depth = (float)depth * 0.007f;
    chorus->current_depth = chorus->target_depth;
    chorus->target_tone = (float)tone * 0.01f;
    chorus->current_tone = chorus->target_tone;
    chorus->rate_selector = (uint8_t)rate;
    chorus->tempo_sync = tempo_sync;
    chorus->configured = true;
    return true;
}

bool gp100_b_chorus_process(gp100_b_chorus_t *chorus,
                                 uint8_t depth,
                                 float rate,
                                 uint8_t tone,
                                 bool tempo_sync,
                                 uint16_t tempo_period_ms,
                                 float *interleaved_stereo,
                                 size_t frame_count,
                                 bool enabled)
{
    float depth_delta;
    float rate_delta;
    float tone_delta;
    size_t frame;

    if (chorus == NULL || !chorus->configured ||
        !parameters_valid(depth, rate, tone, tempo_sync, tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    chorus->target_depth = (float)depth * 0.007f;
    chorus->target_rate_step = rate_step(rate, tempo_sync, tempo_period_ms);
    chorus->target_tone = (float)tone * 0.01f;
    chorus->rate_selector = (uint8_t)rate;
    chorus->tempo_sync = tempo_sync;
    depth_delta = (chorus->target_depth - chorus->current_depth) *
                  B_CHORUS_CONTROL_GLIDE;
    rate_delta = (chorus->target_rate_step - chorus->current_rate_step) *
                 B_CHORUS_CONTROL_GLIDE;
    tone_delta = (chorus->target_tone - chorus->current_tone) *
                 B_CHORUS_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float left;
        float right;

        if (!enabled) {
            continue;
        }
        chorus->current_depth += depth_delta;
        chorus->current_rate_step += rate_delta;
        chorus->current_tone += tone_delta;
        process_sample(chorus, interleaved_stereo[frame * 2u], &left, &right);
        interleaved_stereo[frame * 2u] = left;
        interleaved_stereo[frame * 2u + 1u] = right;
    }
    return true;
}
