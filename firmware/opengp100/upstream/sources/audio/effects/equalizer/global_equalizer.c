#include "global_equalizer.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define GP100_GLOBAL_EQ_TWO_PI 6.2831853071795864769f
#define GP100_GLOBAL_EQ_CUT_Q 0.71f
#define GP100_GLOBAL_EQ_GAIN_STEP 0.0001f
#define GP100_GLOBAL_EQ_GAIN_TARGET 0.5f

enum global_filter_index {
    GLOBAL_FILTER_LOW_CUT = 0,
    GLOBAL_FILTER_BAND_1,
    GLOBAL_FILTER_BAND_2,
    GLOBAL_FILTER_BAND_3,
    GLOBAL_FILTER_BAND_4,
    GLOBAL_FILTER_HIGH_CUT
};

static float word_as_float(uint32_t word)
{
    float value;

    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool decode_state(
    const gp100_sysex_global_state_t *state,
    float parameters[GP100_SYSEX_GLOBAL_PARAMETER_COUNT])
{
    size_t index;

    if (state == NULL || parameters == NULL) {
        return false;
    }
    for (index = 0u; index < GP100_SYSEX_GLOBAL_PARAMETER_COUNT;
         ++index) {
        parameters[index] = word_as_float(state->parameters[index]);
        if (!gp100_global_eq_parameter_value_valid(
                (gp100_global_eq_parameter_t)index,
                parameters[index])) {
            return false;
        }
    }
    return true;
}

static void set_cut_coefficients(gp100_biquad_coefficients_t *output,
                                 float frequency_hz,
                                 bool high_pass)
{
    const float angle = frequency_hz /
                        GP100_GLOBAL_EQUALIZER_SAMPLE_RATE *
                        GP100_GLOBAL_EQ_TWO_PI;
    const float cosine = cosf(angle);
    const float alpha = sinf(angle) /
                        (2.0f * GP100_GLOBAL_EQ_CUT_Q);

    if (high_pass) {
        output->b0 = (1.0f + cosine) * 0.5f;
        output->b1 = -(1.0f + cosine);
        output->b2 = output->b0;
    } else {
        output->b0 = (1.0f - cosine) * 0.5f;
        output->b1 = 1.0f - cosine;
        output->b2 = output->b0;
    }
    output->a0 = 1.0f + alpha;
    output->a1 = -2.0f * cosine;
    output->a2 = 1.0f - alpha;
}

static void set_peak_coefficients(gp100_biquad_coefficients_t *output,
                                  float frequency_hz,
                                  float q,
                                  float gain_db)
{
    const float angle = frequency_hz /
                        GP100_GLOBAL_EQUALIZER_SAMPLE_RATE *
                        GP100_GLOBAL_EQ_TWO_PI;
    const float amplitude = powf(10.0f, gain_db / 40.0f);
    const float alpha = sinf(angle) / (2.0f * q);
    const float cosine = cosf(angle);

    output->b0 = 1.0f + alpha * amplitude;
    output->b1 = -2.0f * cosine;
    output->b2 = 1.0f - alpha * amplitude;
    output->a0 = 1.0f + alpha / amplitude;
    output->a1 = output->b1;
    output->a2 = 1.0f - alpha / amplitude;
}

void gp100_global_equalizer_reset(
    gp100_global_equalizer_t *equalizer)
{
    if (equalizer != NULL) {
        memset(equalizer, 0, sizeof(*equalizer));
    }
}

bool gp100_global_equalizer_configure(
    gp100_global_equalizer_t *equalizer,
    const gp100_sysex_global_state_t *state)
{
    gp100_global_equalizer_t next;
    float parameters[GP100_SYSEX_GLOBAL_PARAMETER_COUNT];
    size_t band;
    size_t index;

    if (equalizer == NULL || !decode_state(state, parameters)) {
        return false;
    }
    next = *equalizer;
    if (!next.configured) {
        gp100_global_equalizer_reset(&next);
    }
    next.enabled = parameters[GP100_GLOBAL_EQ_ENABLE] != 0.0f;
    set_cut_coefficients(
        &next.coefficients[GLOBAL_FILTER_LOW_CUT],
        parameters[GP100_GLOBAL_EQ_LOW_CUT_FREQUENCY],
        true);
    set_cut_coefficients(
        &next.coefficients[GLOBAL_FILTER_HIGH_CUT],
        parameters[GP100_GLOBAL_EQ_HIGH_CUT_FREQUENCY],
        false);
    next.filter_active[GLOBAL_FILTER_LOW_CUT] = true;
    next.filter_active[GLOBAL_FILTER_HIGH_CUT] = true;
    for (band = 0u; band < 4u; ++band) {
        index = GP100_GLOBAL_EQ_BAND_1_FREQUENCY + band * 3u;
        next.filter_active[GLOBAL_FILTER_BAND_1 + band] = true;
        set_peak_coefficients(
            &next.coefficients[GLOBAL_FILTER_BAND_1 + band],
            parameters[index], parameters[index + 1u],
            parameters[index + 2u]);
    }
    memcpy(next.parameters, parameters, sizeof(parameters));
    next.configured = true;
    *equalizer = next;
    return true;
}

bool gp100_global_equalizer_process(
    gp100_global_equalizer_t *equalizer,
    float *interleaved_stereo,
    size_t frame_count)
{
    size_t filter;

    if (equalizer == NULL || !equalizer->configured ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    if (!equalizer->enabled || frame_count == 0u) {
        return true;
    }
    for (filter = 0u; filter < GP100_GLOBAL_EQUALIZER_FILTER_COUNT;
         ++filter) {
        if (equalizer->filter_active[filter] &&
            !gp100_stereo_biquad_process(
                &equalizer->filters[filter],
                &equalizer->coefficients[filter],
                interleaved_stereo, frame_count)) {
            return false;
        }
    }
    /* The generic EQ targets 50% and doubles the result. The gain starts
     * at zero and advances only while Global EQ is enabled. */
    for (size_t frame = 0u;
         frame < frame_count && equalizer->current_output_gain < GP100_GLOBAL_EQ_GAIN_TARGET;
         ++frame) {
        equalizer->current_output_gain = fminf(
            equalizer->current_output_gain + GP100_GLOBAL_EQ_GAIN_STEP,
            GP100_GLOBAL_EQ_GAIN_TARGET);
        const float gain = 2.0f * equalizer->current_output_gain;
        interleaved_stereo[frame * 2u] *= gain;
        interleaved_stereo[frame * 2u + 1u] *= gain;
    }
    return true;
}
