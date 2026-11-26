#include "audio/effects/tone/equalizer.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static int test_eq1_flat_response_and_level_slew(void)
{
    gp100_equalizer_t equalizer;
    float parameters[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 50.0f};
    float audio[4] = {0.25f, -1.0f, -0.5f, 1.0f};

    if (!gp100_equalizer_reset(
            &equalizer, GP100_EQUALIZER_1, parameters) ||
        !close_enough(equalizer.bands[0].frequency_hz, 125.0f, 0.001f) ||
        !close_enough(equalizer.bands[3].two_q, 2.334f, 0.0001f) ||
        !gp100_equalizer_process(
            &equalizer, parameters, audio, 2u, true) ||
        !close_enough(audio[0], 0.25f, 0.000001f) ||
        !close_enough(audio[1], 0.25f, 0.000001f) ||
        !close_enough(audio[2], -0.5f, 0.000001f) ||
        !close_enough(audio[3], -0.5f, 0.000001f)) {
        return 1;
    }

    parameters[5] = 51.0f;
    audio[0] = 1.0f;
    audio[1] = 0.0f;
    if (!gp100_equalizer_process(
            &equalizer, parameters, audio, 1u, true) ||
        !close_enough(equalizer.level, 0.505f, 0.000001f) ||
        !close_enough(audio[0], 1.01f, 0.00001f) ||
        !close_enough(audio[1], 1.01f, 0.00001f)) {
        return 2;
    }
    return 0;
}

static int test_eq2_layout(void)
{
    gp100_equalizer_t equalizer;
    const float parameters[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 50.0f};
    static const float frequencies[5] = {
        50.0f, 120.0f, 400.0f, 800.0f, 2000.0f
    };
    static const float two_q[5] = {
        1.414f, 1.414f, 1.238f, 1.414f, 1.414f
    };
    size_t band;

    if (!gp100_equalizer_reset(
            &equalizer, GP100_EQUALIZER_2, parameters)) {
        return 1;
    }
    for (band = 0u; band < 5u; ++band) {
        if (!close_enough(equalizer.bands[band].frequency_hz,
                          frequencies[band], 0.001f) ||
            !close_enough(equalizer.bands[band].two_q,
                          two_q[band], 0.0001f)) {
            return 2;
        }
    }
    return 0;
}

static int test_mess_eq_factory_curves(void)
{
    gp100_equalizer_t equalizer;
    const float parameters[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    static const float frequencies[5] = {
        31.0f, 370.0f, 688.5f, 1605.0f, 6025.0f
    };
    static const float gains[5] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    static const float two_q[5] = {
        0.432f, 1.424f, 1.209f, 0.884f, 1.605f
    };
    size_t band;

    if (!gp100_equalizer_reset(
            &equalizer, GP100_EQUALIZER_MESS, parameters)) {
        return 1;
    }
    for (band = 0u; band < 5u; ++band) {
        if (!close_enough(equalizer.bands[band].frequency_hz,
                          frequencies[band], 0.001f) ||
            !close_enough(equalizer.bands[band].gain_db,
                          gains[band], 0.0001f) ||
            !close_enough(equalizer.bands[band].two_q,
                          two_q[band], 0.0001f)) {
            return 2;
        }
    }
    return 0;
}

static int test_bypass_and_validation(void)
{
    gp100_equalizer_t equalizer;
    const float valid[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 50.0f};
    float invalid[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 100.0f};
    float audio[2] = {0.25f, -0.5f};

    if (!gp100_equalizer_reset(
            &equalizer, GP100_EQUALIZER_1, valid) ||
        !gp100_equalizer_process(
            &equalizer, valid, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        gp100_equalizer_process(
            &equalizer, invalid, audio, 1u, true) ||
        gp100_equalizer_reset(
            NULL, GP100_EQUALIZER_1, valid) ||
        gp100_equalizer_reset(
            &equalizer, (gp100_equalizer_type_t)3, valid) ||
        gp100_equalizer_process(
            &equalizer, valid, NULL, 1u, true)) {
        return 1;
    }

    invalid[5] = 50.0f;
    invalid[2] = -51.0f;
    if (gp100_equalizer_reset(
            &equalizer, GP100_EQUALIZER_MESS, invalid)) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result = test_eq1_flat_response_and_level_slew();

    if (result != 0) {
        fprintf(stderr, "EQ 1 test failed: %d\n", result);
        return 10 + result;
    }
    result = test_eq2_layout();
    if (result != 0) {
        fprintf(stderr, "EQ 2 test failed: %d\n", result);
        return 20 + result;
    }
    result = test_mess_eq_factory_curves();
    if (result != 0) {
        fprintf(stderr, "Mess EQ test failed: %d\n", result);
        return 30 + result;
    }
    result = test_bypass_and_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
