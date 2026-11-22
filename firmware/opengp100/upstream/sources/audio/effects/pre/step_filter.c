#include "step_filter.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define STEP_FILTER_SAMPLE_RATE 44100.0f
#define STEP_FILTER_TWO_PI 6.2831855f

static bool parameters_valid(uint8_t step_1,
                             uint8_t step_2,
                             uint8_t step_3,
                             uint8_t step_4,
                             float rate,
                             bool tempo_sync)
{
    return step_1 <= 99u && step_2 <= 99u && step_3 <= 99u &&
           step_4 <= 99u && isfinite(rate) &&
           (tempo_sync ? rate >= 0.0f && rate <= 10.0f
                       : rate >= 0.1f && rate <= 10.0f);
}

static void set_parameters(gp100_step_filter_t *filter,
                           uint8_t step_1,
                           uint8_t step_2,
                           uint8_t step_3,
                           uint8_t step_4,
                           bool tempo_sync)
{
    filter->steps[0] = (float)step_1 * 0.01f;
    filter->steps[1] = (float)step_2 * 0.01f;
    filter->steps[2] = (float)step_3 * 0.01f;
    filter->steps[3] = (float)step_4 * 0.01f;
    filter->tempo_sync = tempo_sync;
}

static float biquad(gp100_biquad_channel_state_t *state,
                    float input,
                    float b0,
                    float b1,
                    float b2,
                    float a0,
                    float a1,
                    float a2)
{
    float output = (input * b0 + state->x1 * b1 + state->x2 * b2 -
                    state->y1 * a1 - state->y2 * a2) / a0;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static float input_high_pass(gp100_step_filter_t *filter, float input)
{
    const float omega = STEP_FILTER_TWO_PI * 100.0f / STEP_FILTER_SAMPLE_RATE;
    float sine = sinf(omega);
    float cosine = cosf(omega);

    return biquad(&filter->input_high_pass, input,
                  (cosine + 1.0f) * 0.5f, -1.0f - cosine,
                  (cosine + 1.0f) * 0.5f, sine / 1.414f + 1.0f,
                  -2.0f * cosine, 1.0f - sine / 1.414f);
}

static float dynamic_band_pass(gp100_step_filter_t *filter,
                               float input,
                               float position)
{
    float omega;
    float sine;
    float cosine;
    float alpha;

    filter->cutoff_hz = position * 1680.0f + 420.0f;
    omega = (filter->cutoff_hz / STEP_FILTER_SAMPLE_RATE) * STEP_FILTER_TWO_PI;
    sine = sinf(omega);
    cosine = cosf(omega);
    alpha = sine / ((2.0f + (1.0f - position) * 4.0f) * 2.0f);
    return biquad(&filter->band_pass, input,
                  alpha * 7.0795f, 0.0f, -alpha * 7.0795f,
                  alpha + 1.0f, -2.0f * cosine, 1.0f - alpha);
}

static float cycle_increment(float rate,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    if (!tempo_sync) {
        return rate / STEP_FILTER_SAMPLE_RATE;
    }
    if (tempo_period_ms == 0u) {
        return 0.0f;
    }
    return 1.0f / ((float)tempo_period_ms * 44.1f *
                   gp100_effect_tempo_beats_for_rate(rate));
}

bool gp100_step_filter_reset(gp100_step_filter_t *filter,
                                  uint8_t step_1,
                                  uint8_t step_2,
                                  uint8_t step_3,
                                  uint8_t step_4,
                                  float rate,
                                  bool tempo_sync)
{
    if (filter == NULL || !parameters_valid(
            step_1, step_2, step_3, step_4, rate, tempo_sync)) {
        return false;
    }
    memset(filter, 0, sizeof(*filter));
    set_parameters(filter, step_1, step_2, step_3, step_4, tempo_sync);
    filter->selected_step = filter->steps[0];
    filter->configured = true;
    return true;
}

bool gp100_step_filter_process(gp100_step_filter_t *filter,
                                    uint8_t step_1,
                                    uint8_t step_2,
                                    uint8_t step_3,
                                    uint8_t step_4,
                                    float rate,
                                    bool tempo_sync,
                                    uint16_t tempo_period_ms,
                                    float *interleaved_stereo,
                                    size_t frame_count,
                                    bool enabled)
{
    float cycle_increment_delta;
    size_t frame;

    if (filter == NULL || !filter->configured || !parameters_valid(
            step_1, step_2, step_3, step_4, rate, tempo_sync) ||
        (tempo_sync && tempo_period_ms == 0u) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    set_parameters(filter, step_1, step_2, step_3, step_4, tempo_sync);
    filter->target_cycle_increment =
        cycle_increment(rate, tempo_sync, tempo_period_ms);
    cycle_increment_delta =
        (filter->target_cycle_increment - filter->current_cycle_increment) *
        0.00022676f;
    for (frame = 0u; frame < frame_count; ++frame) {
        float quarter_samples;
        float position;
        float output;

        if (!enabled) {
            continue;
        }
        filter->current_cycle_increment += cycle_increment_delta;
        quarter_samples = filter->current_cycle_increment > 0.0f
            ? 0.25f * (1.0f / filter->current_cycle_increment) : INFINITY;
        position = filter->phase_counter;
        if (position <= quarter_samples * 4.0f) {
            const unsigned step_index = (unsigned)(position > quarter_samples) +
                (unsigned)(position > quarter_samples * 2.0f) +
                (unsigned)(position > quarter_samples * 3.0f);
            filter->selected_step = filter->steps[step_index];
        }
        filter->phase_counter += 1.0f;
        if (filter->phase_counter >= quarter_samples * 4.0f) {
            filter->phase_counter = 0.0f;
        }
        output = input_high_pass(filter, interleaved_stereo[frame * 2u]);
        output = dynamic_band_pass(filter, output, filter->selected_step);
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}
