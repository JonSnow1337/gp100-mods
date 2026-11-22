#include "equalizer.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define GP100_TWO_PI 6.2831855f
#define GP100_EQ_LEVEL_STEP 0.005f

static const float eq1_frequencies[GP100_EQUALIZER_BAND_COUNT] = {
    125.0f, 400.0f, 800.0f, 1600.0f, 4000.0f
};

static const float eq1_two_q[GP100_EQUALIZER_BAND_COUNT] = {
    1.414f, 1.76f, 1.76f, 2.334f, 1.414f
};

static const float eq2_frequencies[GP100_EQUALIZER_BAND_COUNT] = {
    50.0f, 120.0f, 400.0f, 800.0f, 2000.0f
};

static const float eq2_two_q[GP100_EQUALIZER_BAND_COUNT] = {
    1.414f, 1.414f, 1.238f, 1.414f, 1.414f
};

static bool gain_parameter_is_valid(float parameter)
{
    return parameter >= -50.0f && parameter <= 50.0f;
}

static void set_peaking_coefficients(gp100_equalizer_band_t *band,
                                     float frequency_hz,
                                     float gain_db,
                                     float two_q)
{
    float angle = frequency_hz / GP100_EQUALIZER_SAMPLE_RATE *
                  GP100_TWO_PI;
    float amplitude = powf(10.0f, gain_db / 40.0f);
    float alpha = sinf(angle) / two_q;
    float cosine = cosf(angle);

    band->frequency_hz = frequency_hz;
    band->gain_db = gain_db;
    band->two_q = two_q;
    band->coefficients.b0 = 1.0f + alpha * amplitude;
    band->coefficients.b1 = -2.0f * cosine;
    band->coefficients.b2 = 1.0f - alpha * amplitude;
    band->coefficients.a0 = 1.0f + alpha / amplitude;
    band->coefficients.a1 = -2.0f * cosine;
    band->coefficients.a2 = 1.0f - alpha / amplitude;
}

static void configure_standard_band(gp100_equalizer_t *equalizer,
                                    size_t band_index,
                                    float parameter)
{
    const float *frequencies = equalizer->type == GP100_EQUALIZER_1
                                   ? eq1_frequencies
                                   : eq2_frequencies;
    const float *two_q = equalizer->type == GP100_EQUALIZER_1
                             ? eq1_two_q
                             : eq2_two_q;

    set_peaking_coefficients(&equalizer->bands[band_index],
                             frequencies[band_index], parameter * 0.24f,
                             two_q[band_index]);
}

static void configure_mess_band(gp100_equalizer_t *equalizer,
                                size_t band_index,
                                float parameter)
{
    float normalized = parameter * 0.01f + 0.5f;
    float frequency_hz;
    float gain_db;
    float two_q;

    switch (band_index) {
    case 0u:
        frequency_hz = 31.0f;
        gain_db = normalized * 30.0f - 15.0f;
        two_q = 0.432f;
        break;
    case 1u:
        frequency_hz = 370.0f;
        gain_db = normalized * 18.0f - 8.0f;
        two_q = 1.424f;
        break;
    case 2u:
        frequency_hz = normalized * 49.0f + 664.0f;
        gain_db = normalized * 20.0f - 10.0f;
        two_q = (0.727f - normalized * 0.245f) * 2.0f;
        break;
    case 3u:
        frequency_hz = normalized * 318.0f + 1446.0f;
        gain_db = normalized * 16.0f - 8.0f;
        two_q = 0.884f;
        break;
    default:
        frequency_hz = normalized * 352.0f + 5849.0f;
        gain_db = normalized * 20.0f - 10.0f;
        two_q = (0.923f - normalized * 0.241f) * 2.0f;
        break;
    }
    set_peaking_coefficients(&equalizer->bands[band_index], frequency_hz,
                             gain_db, two_q);
}

static bool parameters_are_valid(gp100_equalizer_type_t type,
                                 const float *parameters)
{
    size_t band;

    if (parameters == NULL ||
        (type != GP100_EQUALIZER_1 &&
         type != GP100_EQUALIZER_2 &&
         type != GP100_EQUALIZER_MESS)) {
        return false;
    }
    for (band = 0u; band < GP100_EQUALIZER_BAND_COUNT; ++band) {
        if (!gain_parameter_is_valid(parameters[band])) {
            return false;
        }
    }
    return type == GP100_EQUALIZER_MESS ||
           (parameters[5] >= 0.0f && parameters[5] <= 99.0f);
}

static void configure_changed_bands(gp100_equalizer_t *equalizer,
                                    const float *parameters,
                                    bool force)
{
    size_t band;

    for (band = 0u; band < GP100_EQUALIZER_BAND_COUNT; ++band) {
        if (!force && equalizer->bands[band].parameter == parameters[band]) {
            continue;
        }
        equalizer->bands[band].parameter = parameters[band];
        if (equalizer->type == GP100_EQUALIZER_MESS) {
            configure_mess_band(equalizer, band, parameters[band]);
        } else {
            configure_standard_band(equalizer, band, parameters[band]);
        }
    }
}

bool gp100_equalizer_reset(
    gp100_equalizer_t *equalizer,
    gp100_equalizer_type_t type,
    const float *parameters)
{
    if (equalizer == NULL ||
        (type != GP100_EQUALIZER_1 &&
         type != GP100_EQUALIZER_2 &&
         type != GP100_EQUALIZER_MESS) ||
        !parameters_are_valid(type, parameters)) {
        return false;
    }

    memset(equalizer, 0, sizeof(*equalizer));
    equalizer->type = type;
    equalizer->level = type == GP100_EQUALIZER_MESS
                           ? 1.0f
                           : parameters[5] * 0.01f;
    configure_changed_bands(equalizer, parameters, true);
    equalizer->configured = true;
    return true;
}

static float process_band(gp100_equalizer_band_t *band, float input)
{
    gp100_biquad_channel_state_t *state = &band->state;
    const gp100_biquad_coefficients_t *coefficients =
        &band->coefficients;
    float output =
        (input * coefficients->b0 + state->x1 * coefficients->b1 +
         state->x2 * coefficients->b2 - state->y1 * coefficients->a1 -
         state->y2 * coefficients->a2) /
        coefficients->a0;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static void slew_level(gp100_equalizer_t *equalizer, float target)
{
    if (target > equalizer->level) {
        equalizer->level += GP100_EQ_LEVEL_STEP;
        if (equalizer->level > target) {
            equalizer->level = target;
        }
    } else if (target < equalizer->level) {
        equalizer->level -= GP100_EQ_LEVEL_STEP;
        if (equalizer->level < target) {
            equalizer->level = target;
        }
    }
}

bool gp100_equalizer_process(
    gp100_equalizer_t *equalizer,
    const float *parameters,
    float *interleaved_stereo,
    size_t frame_count,
    bool enabled)
{
    size_t frame;

    if (equalizer == NULL || !equalizer->configured ||
        !parameters_are_valid(equalizer->type, parameters) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }

    if (equalizer->type != GP100_EQUALIZER_MESS) {
        slew_level(equalizer, parameters[5] * 0.01f);
    }
    if (!enabled || frame_count == 0u) {
        return true;
    }

    configure_changed_bands(equalizer, parameters, false);
    for (frame = 0u; frame < frame_count; ++frame) {
        size_t sample = frame * 2u;
        float output = interleaved_stereo[sample];
        size_t band;

        for (band = 0u; band < GP100_EQUALIZER_BAND_COUNT; ++band) {
            output = process_band(&equalizer->bands[band], output);
        }
        if (equalizer->type != GP100_EQUALIZER_MESS) {
            output *= equalizer->level * 2.0f;
        }
        interleaved_stereo[sample] = output;
        interleaved_stereo[sample + 1u] = output;
    }
    return true;
}
