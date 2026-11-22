#include "a_chorus.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define A_CHORUS_SAMPLE_RATE 44100.0f
#define A_CHORUS_CONTROL_SCALE 0.01f
#define A_CHORUS_CONTROL_GLIDE 0x1.db8cdap-13f
#define A_CHORUS_FOUR_PI 12.566371f
#define A_CHORUS_TWO_OVER_PI 0.63661975f

typedef struct a_chorus_iir2_coefficients {
    float b[3];
    float a[2];
} a_chorus_iir2_coefficients_t;

typedef struct a_chorus_iir3_coefficients {
    float b[4];
    float a[3];
} a_chorus_iir3_coefficients_t;

typedef struct a_chorus_iir4_coefficients {
    float b[5];
    float a[4];
} a_chorus_iir4_coefficients_t;

static const a_chorus_iir2_coefficients_t input_coefficients = {
    {-3.55475354f, -0.202053159f, 3.35270047f},
    {-0.300300509f, -0.295593143f}
};

static const a_chorus_iir2_coefficients_t pre_delay_coefficients = {
    {2.49129629f, -4.91623545f, 2.42493939f},
    {-1.93130302f, 0.931453526f}
};

static const a_chorus_iir4_coefficients_t feedback_coefficients = {
    {0.0435417667f, 0.0870835334f, 0.0f,
     -0.0870835334f, -0.0435417667f},
    {-2.18200111f, 1.90018499f, -0.856478989f, 0.140688732f}
};

static const a_chorus_iir2_coefficients_t center_coefficients = {
    {-0.183726221f, -0.0525112115f, 0.131215006f},
    {-0.0780445263f, -0.816933036f}
};

static const a_chorus_iir3_coefficients_t delay_coefficients = {
    {0.033947017f, 0.101841055f, 0.101841055f, 0.033947017f},
    {-1.28771806f, 0.855327129f, -0.232212573f}
};

static const a_chorus_iir3_coefficients_t output_coefficients = {
    {-0.218525991f, 0.156068578f, 0.218525991f, -0.156068578f},
    {-1.05196059f, -0.740924239f, 0.795624256f}
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
    return rate / A_CHORUS_SAMPLE_RATE;
}

static float depth_value(uint8_t depth)
{
    return 0.25f + (float)depth * A_CHORUS_CONTROL_SCALE * 0.6f;
}

static a_chorus_iir2_coefficients_t tone_coefficients(float tone)
{
    a_chorus_iir2_coefficients_t coefficients;

    coefficients.b[0] = tone * 0.8888539f + 0.028126698f;
    coefficients.b[1] = 0.0020984341f - tone * 1.7777078f;
    coefficients.b[2] = tone * 0.8888539f - 0.026028262f;
    coefficients.a[0] = -1.8472564f;
    coefficients.a[1] = 0.8514533f;
    return coefficients;
}

static float iir2(gp100_a_chorus_iir2_t *state,
                  const a_chorus_iir2_coefficients_t *coefficients,
                  float input)
{
    float output = input * coefficients->b[0] +
                   state->x[0] * coefficients->b[1] +
                   state->x[1] * coefficients->b[2] -
                   state->y[0] * coefficients->a[0] -
                   state->y[1] * coefficients->a[1];

    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static float iir3(gp100_a_chorus_iir3_t *state,
                  const a_chorus_iir3_coefficients_t *coefficients,
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

static float iir4(gp100_a_chorus_iir4_t *state,
                  const a_chorus_iir4_coefficients_t *coefficients,
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

static float delay_sample(gp100_a_chorus_t *chorus,
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
    if (chorus->write_index[channel] == GP100_A_CHORUS_DELAY_SAMPLES) {
        chorus->write_index[channel] = 0u;
    }
    index = (chorus->write_index[channel] +
             GP100_A_CHORUS_DELAY_SAMPLES - integer_delay) %
            GP100_A_CHORUS_DELAY_SAMPLES;
    previous = index == 0u ? GP100_A_CHORUS_DELAY_SAMPLES - 1u
                           : index - 1u;
    newer = chorus->delay[channel][index];
    older = chorus->delay[channel][previous];
    return older * fraction + newer * (1.0f - fraction);
}

static void process_sample(gp100_a_chorus_t *chorus,
                           float input,
                           float *left,
                           float *right)
{
    a_chorus_iir2_coefficients_t tone = tone_coefficients(chorus->tone);
    float filtered;
    float center;
    float delayed_source;
    float triangle;
    float left_delay;
    float right_delay;
    float wet_left;
    float wet_right;

    filtered = iir2(&chorus->input_filter, &input_coefficients, input);
    filtered = iir2(&chorus->tone_filter, &tone, filtered);
    filtered = iir2(&chorus->pre_delay_filter,
                    &pre_delay_coefficients, filtered);
    center = iir2(&chorus->center_filter, &center_coefficients, filtered);
    delayed_source = iir4(&chorus->feedback_filter,
                          &feedback_coefficients,
                          filtered + chorus->feedback_left * 0.35f);

    triangle = asinf(sinf(chorus->current_rate_step * A_CHORUS_FOUR_PI *
                          (float)chorus->lfo_sample)) *
               A_CHORUS_TWO_OVER_PI;
    chorus->lfo_sample++;
    if (chorus->current_rate_step > 0.0f &&
        (float)chorus->lfo_sample >= 1.0f / chorus->current_rate_step) {
        chorus->lfo_sample = 0u;
    }
    left_delay = 2.0f + 255.0f * (triangle + 1.0f) * 0.5f *
                 chorus->current_depth;
    right_delay = 2.0f + 255.0f * (1.0f - triangle) * 0.5f *
                  chorus->current_depth;
    wet_left = delay_sample(chorus, 0u, delayed_source, left_delay);
    wet_right = delay_sample(chorus, 1u, delayed_source, right_delay);
    wet_left = iir3(&chorus->delay_filter[0], &delay_coefficients, wet_left);
    wet_right = iir3(&chorus->delay_filter[1], &delay_coefficients, wet_right);
    wet_left = iir3(&chorus->output_filter[0], &output_coefficients, wet_left);
    wet_right = iir3(&chorus->output_filter[1], &output_coefficients,
                     wet_right);
    chorus->feedback_left = wet_left;
    chorus->last_wet[0] = wet_left;
    chorus->last_wet[1] = wet_right;
    *left = wet_left + center;
    *right = wet_right + center;
}

bool gp100_a_chorus_reset(gp100_a_chorus_t *chorus,
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
    chorus->tone = (float)tone * A_CHORUS_CONTROL_SCALE;
    chorus->rate_selector = (uint8_t)rate;
    chorus->tempo_sync = tempo_sync;
    chorus->configured = true;
    return true;
}

bool gp100_a_chorus_process(gp100_a_chorus_t *chorus,
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
    size_t frame;

    if (chorus == NULL || !chorus->configured ||
        !parameters_valid(depth, rate, tone, tempo_sync, tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    chorus->target_depth = depth_value(depth);
    chorus->target_rate_step = rate_step(rate, tempo_sync, tempo_period_ms);
    chorus->tone = (float)tone * A_CHORUS_CONTROL_SCALE;
    chorus->rate_selector = (uint8_t)rate;
    chorus->tempo_sync = tempo_sync;
    depth_delta = (chorus->target_depth - chorus->current_depth) *
                  A_CHORUS_CONTROL_GLIDE;
    rate_delta = (chorus->target_rate_step - chorus->current_rate_step) *
                 A_CHORUS_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float left;
        float right;

        if (!enabled) {
            continue;
        }
        chorus->current_depth += depth_delta;
        chorus->current_rate_step += rate_delta;
        process_sample(chorus, interleaved_stereo[frame * 2u],
                       &left, &right);
        interleaved_stereo[frame * 2u] = left;
        interleaved_stereo[frame * 2u + 1u] = right;
    }
    return true;
}
