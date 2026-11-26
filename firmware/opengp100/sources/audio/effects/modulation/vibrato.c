#include "vibrato.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define VIBRATO_SAMPLE_RATE 44100.0f
#define VIBRATO_CONTROL_GLIDE 0x1.db8cdap-13f
#define VIBRATO_TWO_PI 6.2831855f

typedef struct iir2_coefficients {
    float b[3];
    float a[3];
} iir2_coefficients_t;

typedef struct iir3_coefficients {
    float b[4];
    float a[4];
} iir3_coefficients_t;

typedef struct iir4_coefficients {
    float b[5];
    float a[5];
} iir4_coefficients_t;

static const iir2_coefficients_t input_coefficients[2] = {
    {
        {-0x1.58ef62p+2f, -0x1.47a978p-2f, 0x1.4474cap+2f},
        {1.0f, 0x1.f8257ap-4f, -0x1.eeb67p-2f}
    },
    {
        {0x1.617614p-1f, 0x1.617614p+0f, 0x1.617614p-1f},
        {1.0f, 0x1.c07456p-1f, 0x1.c563f6p-1f}
    }
};

static const iir4_coefficients_t delay_coefficients = {
    {0x1.de0c8cp-4f, 0x1.de0c8cp-3f, 0.0f,
     -0x1.de0c8cp-3f, -0x1.de0c8cp-4f},
    {1.0f, -0x1.3f7268p+0f, 0x1.47ab8cp-1f,
     -0x1.1479fp-1f, 0x1.31987ep-3f}
};

static const iir2_coefficients_t shaping_coefficients = {
    {0x1.0547c8p-1f, 0x1.0547c8p+0f, 0x1.0547c8p-1f},
    {1.0f, 0x1.9af642p-2f, 0x1.47a3fcp-1f}
};

static const iir3_coefficients_t output_coefficients = {
    {-0x1.8b8cd4p-3f, 0x1.1a7f3ep-3f,
     0x1.8b8cd4p-3f, -0x1.1a7f3ep-3f},
    {1.0f, -0x1.f872fap-1f, -0x1.d17c6p-1f,
     0x1.ca352ap-1f}
};

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
    return rate / VIBRATO_SAMPLE_RATE;
}

static float iir2(gp100_vibrato_iir2_t *state,
                  const iir2_coefficients_t *coefficients,
                  float input)
{
    float output = (input * coefficients->b[0] +
                    state->x[0] * coefficients->b[1] +
                    state->x[1] * coefficients->b[2] -
                    state->y[0] * coefficients->a[1] -
                    state->y[1] * coefficients->a[2]) /
                   coefficients->a[0];

    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static float iir3(gp100_vibrato_iir3_t *state,
                  const iir3_coefficients_t *coefficients,
                  float input)
{
    float output = (input * coefficients->b[0] +
                    state->x[0] * coefficients->b[1] +
                    state->x[1] * coefficients->b[2] +
                    state->x[2] * coefficients->b[3] -
                    state->y[0] * coefficients->a[1] -
                    state->y[1] * coefficients->a[2] -
                    state->y[2] * coefficients->a[3]) /
                   coefficients->a[0];
    size_t index;

    for (index = 2u; index != 0u; --index) {
        state->x[index] = state->x[index - 1u];
        state->y[index] = state->y[index - 1u];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static float iir4(gp100_vibrato_iir4_t *state,
                  const iir4_coefficients_t *coefficients,
                  float input)
{
    float output = (input * coefficients->b[0] +
                    state->x[0] * coefficients->b[1] +
                    state->x[1] * coefficients->b[2] +
                    state->x[2] * coefficients->b[3] +
                    state->x[3] * coefficients->b[4] -
                    state->y[0] * coefficients->a[1] -
                    state->y[1] * coefficients->a[2] -
                    state->y[2] * coefficients->a[3] -
                    state->y[3] * coefficients->a[4]) /
                   coefficients->a[0];
    size_t index;

    for (index = 3u; index != 0u; --index) {
        state->x[index] = state->x[index - 1u];
        state->y[index] = state->y[index - 1u];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static float delay_sample(gp100_vibrato_t *vibrato,
                          size_t channel,
                          float input,
                          float delay_samples)
{
    size_t integer_delay = (size_t)delay_samples;
    float fraction = delay_samples - (float)integer_delay;
    size_t newer;
    size_t older;

    vibrato->delay[channel][vibrato->write_index[channel]] = input;
    vibrato->write_index[channel]++;
    if (vibrato->write_index[channel] == GP100_VIBRATO_DELAY_SAMPLES) {
        vibrato->write_index[channel] = 0u;
    }
    newer = (vibrato->write_index[channel] +
             GP100_VIBRATO_DELAY_SAMPLES - integer_delay) %
            GP100_VIBRATO_DELAY_SAMPLES;
    older = newer == 0u ? GP100_VIBRATO_DELAY_SAMPLES - 1u
                        : newer - 1u;
    return vibrato->delay[channel][older] * fraction +
           vibrato->delay[channel][newer] * (1.0f - fraction);
}

static void process_sample(gp100_vibrato_t *vibrato,
                           float input,
                           float *left,
                           float *right)
{
    float filtered = iir2(&vibrato->input_filter[0],
                          &input_coefficients[0], input);
    float sine;
    float delay[2];
    float wet[2];
    size_t channel;

    filtered = iir2(&vibrato->input_filter[1],
                    &input_coefficients[1], filtered);
    sine = sinf(vibrato->current_rate_step * VIBRATO_TWO_PI *
                (float)vibrato->lfo_sample);
    vibrato->lfo_sample++;
    if (vibrato->current_rate_step > 0.0f &&
        (float)vibrato->lfo_sample >= 1.0f / vibrato->current_rate_step) {
        vibrato->lfo_sample = 0u;
    }
    delay[0] = 20.0f + (1.0f + sine) * 0.5f * 69.0f *
               vibrato->current_depth;
    delay[1] = 20.0f + (1.0f - sine) * 0.5f * 69.0f *
               vibrato->current_depth;
    for (channel = 0u; channel < 2u; ++channel) {
        wet[channel] = delay_sample(vibrato, channel, filtered,
                                    delay[channel]);
        wet[channel] = iir4(&vibrato->delay_filter[channel],
                            &delay_coefficients, wet[channel]);
        wet[channel] = iir2(&vibrato->shaping_filter[channel],
                            &shaping_coefficients, wet[channel]);
        wet[channel] = iir3(&vibrato->output_filter[channel],
                            &output_coefficients, wet[channel]);
    }
    *left = wet[0];
    *right = wet[1];
}

bool gp100_vibrato_reset(gp100_vibrato_t *vibrato,
                              uint8_t depth,
                              float rate,
                              bool tempo_sync)
{
    if (vibrato == NULL ||
        !parameters_valid(depth, rate, tempo_sync, 1u)) {
        return false;
    }
    memset(vibrato, 0, sizeof(*vibrato));
    vibrato->target_depth = (float)depth * 0.01f;
    vibrato->current_depth = vibrato->target_depth;
    vibrato->rate_selector = (uint8_t)rate;
    vibrato->tempo_sync = tempo_sync;
    vibrato->configured = true;
    return true;
}

bool gp100_vibrato_process(gp100_vibrato_t *vibrato,
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

    if (vibrato == NULL || !vibrato->configured ||
        !parameters_valid(depth, rate, tempo_sync, tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    vibrato->target_depth = (float)depth * 0.01f;
    vibrato->target_rate_step = rate_step(rate, tempo_sync, tempo_period_ms);
    vibrato->rate_selector = (uint8_t)rate;
    vibrato->tempo_sync = tempo_sync;
    depth_delta = (vibrato->target_depth - vibrato->current_depth) *
                  VIBRATO_CONTROL_GLIDE;
    rate_delta = (vibrato->target_rate_step - vibrato->current_rate_step) *
                 VIBRATO_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float left;
        float right;

        if (!enabled) {
            continue;
        }
        vibrato->current_depth += depth_delta;
        vibrato->current_rate_step += rate_delta;
        process_sample(vibrato, interleaved_stereo[frame * 2u],
                       &left, &right);
        interleaved_stereo[frame * 2u] = left;
        interleaved_stereo[frame * 2u + 1u] = right;
    }
    return true;
}
