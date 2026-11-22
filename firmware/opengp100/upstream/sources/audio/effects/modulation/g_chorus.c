#include "g_chorus.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define G_CHORUS_SAMPLE_RATE 44100.0f
#define G_CHORUS_CONTROL_GLIDE 0x1.db8cdap-13f
#define G_CHORUS_FOUR_PI 12.566371f
#define G_CHORUS_TWO_OVER_PI 0.63661975f

typedef struct g_chorus_iir3_coefficients {
    float b[4];
    float a[3];
} g_chorus_iir3_coefficients_t;

typedef struct g_chorus_iir4_coefficients {
    float b[5];
    float a[4];
} g_chorus_iir4_coefficients_t;

static const g_chorus_iir3_coefficients_t feedback_coefficients = {
    {0.123172916f, 0.140527666f, -0.0884634107f, -0.105818167f},
    {-1.36554623f, 0.785564423f, -0.197877333f}
};

static const g_chorus_iir4_coefficients_t output_coefficients = {
    {0.0257303026f, 0.10292121f, 0.154381812f,
     0.10292121f, 0.0257303026f},
    {-0.0989265591f, -0.239646748f, -0.106888995f, -0.142852858f}
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
    return rate / G_CHORUS_SAMPLE_RATE;
}

static float depth_value(uint8_t depth)
{
    return 0.1f + (float)depth * 0.006f;
}

static float input_filter(gp100_g_chorus_iir1_t *state, float input)
{
    float scaled = input * 0.25f;
    float output = scaled * 47.8868942f + state->x1 * -47.8820801f -
                   state->y1 * -0.995186985f;

    state->x1 = scaled;
    state->y1 = output;
    return output;
}

static float iir3(gp100_g_chorus_iir3_t *state,
                  const g_chorus_iir3_coefficients_t *coefficients,
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

static float iir4(gp100_g_chorus_iir4_t *state,
                  const g_chorus_iir4_coefficients_t *coefficients,
                  float input)
{
    float output = input * coefficients->b[0] +
                   state->x[0] * coefficients->b[1] +
                   state->x[1] * coefficients->b[2] +
                   state->x[2] * coefficients->b[3] +
                   state->x[3] * coefficients->b[4] -
                   state->y[0] * coefficients->a[0] -
                   state->y[1] * coefficients->a[1] -
                   state->y[2] * coefficients->a[2] -
                   state->y[3] * coefficients->a[3];
    size_t index;

    for (index = 3u; index != 0u; --index) {
        state->x[index] = state->x[index - 1u];
        state->y[index] = state->y[index - 1u];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static float depth_filter(gp100_g_chorus_iir1_t *state,
                          float input,
                          float depth)
{
    const float numerator = 0.60552597f;
    float a0 = depth + 2.605526f;
    float a1 = -depth - 1.394474f;
    float output = (input * numerator + state->x1 * numerator -
                    state->y1 * a1) / a0;

    state->x1 = input;
    state->y1 = output;
    return output;
}

static float delay_sample(gp100_g_chorus_t *chorus,
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
    if (chorus->write_index[channel] == GP100_G_CHORUS_DELAY_SAMPLES) {
        chorus->write_index[channel] = 0u;
    }
    index = (chorus->write_index[channel] +
             GP100_G_CHORUS_DELAY_SAMPLES - integer_delay) %
            GP100_G_CHORUS_DELAY_SAMPLES;
    previous = index == 0u ? GP100_G_CHORUS_DELAY_SAMPLES - 1u
                           : index - 1u;
    newer = chorus->delay[channel][index];
    older = chorus->delay[channel][previous];
    return older * fraction + newer * (1.0f - fraction);
}

static void move_depth(gp100_g_chorus_t *chorus)
{
    if (chorus->current_depth < chorus->target_depth) {
        chorus->current_depth += 0.001f;
        if (chorus->current_depth > chorus->target_depth) {
            chorus->current_depth = chorus->target_depth;
        }
    } else if (chorus->current_depth > chorus->target_depth) {
        chorus->current_depth -= 0.001f;
        if (chorus->current_depth < chorus->target_depth) {
            chorus->current_depth = chorus->target_depth;
        }
    }
}

static void process_sample(gp100_g_chorus_t *chorus,
                           float input,
                           float *left,
                           float *right)
{
    float dry = input_filter(&chorus->input_filter, input);
    float triangle = asinf(sinf(chorus->current_rate_step * G_CHORUS_FOUR_PI *
                                (float)chorus->lfo_sample)) *
                     G_CHORUS_TWO_OVER_PI;
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
        float feedback_input = dry + chorus->feedback_output[channel] * 0.9f;
        float delay_input = iir3(&chorus->feedback_filter[channel],
                                 &feedback_coefficients, feedback_input);
        float delay_time = 82.0f + phase[channel] * 174.0f *
                           chorus->current_depth;
        float delayed = delay_sample(chorus, channel, delay_input, delay_time);
        float filtered = depth_filter(&chorus->depth_filter[channel], delayed,
                                      chorus->current_depth);

        output[channel] = iir4(&chorus->output_filter[channel],
                               &output_coefficients, filtered);
        chorus->feedback_output[channel] = output[channel];
        output[channel] = output[channel] * (phase[channel] * 0.15f + 0.85f) *
                          0.68f * chorus->current_tone + dry * 0.066666f;
    }
    *left = output[0];
    *right = output[1];
}

bool gp100_g_chorus_reset(gp100_g_chorus_t *chorus,
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
    chorus->target_depth = depth_value(depth);
    chorus->current_depth = chorus->target_depth;
    chorus->target_tone = (float)tone * 0.01f;
    chorus->current_tone = chorus->target_tone;
    chorus->rate_selector = (uint8_t)rate;
    chorus->tempo_sync = tempo_sync;
    chorus->configured = true;
    return true;
}

bool gp100_g_chorus_process(gp100_g_chorus_t *chorus,
                                 uint8_t depth,
                                 float rate,
                                 uint8_t tone,
                                 bool tempo_sync,
                                 uint16_t tempo_period_ms,
                                 float *interleaved_stereo,
                                 size_t frame_count,
                                 bool enabled)
{
    float rate_delta;
    float tone_delta;
    size_t frame;

    if (chorus == NULL || !chorus->configured ||
        !parameters_valid(depth, rate, tone, tempo_sync, tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    chorus->target_depth = depth_value(depth);
    chorus->target_rate_step = rate_step(rate, tempo_sync, tempo_period_ms);
    chorus->target_tone = (float)tone * 0.01f;
    chorus->rate_selector = (uint8_t)rate;
    chorus->tempo_sync = tempo_sync;
    move_depth(chorus);
    rate_delta = (chorus->target_rate_step - chorus->current_rate_step) *
                 G_CHORUS_CONTROL_GLIDE;
    tone_delta = (chorus->target_tone - chorus->current_tone) *
                 G_CHORUS_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float left;
        float right;

        if (!enabled) {
            continue;
        }
        chorus->current_rate_step += rate_delta;
        chorus->current_tone += tone_delta;
        process_sample(chorus, interleaved_stereo[frame * 2u], &left, &right);
        interleaved_stereo[frame * 2u] = left;
        interleaved_stereo[frame * 2u + 1u] = right;
    }
    return true;
}
