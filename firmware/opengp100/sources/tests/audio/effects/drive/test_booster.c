#include "audio/effects/drive/booster.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static int test_coefficients_and_gain_curve(void)
{
    gp100_boost14_t booster;

    if (!gp100_boost14_reset(&booster, 50u) ||
        !close_enough(booster.current_gain, 0.0615f, 0.0000001f) ||
        !close_enough(booster.coefficients[0].b0,
                      6.765678882598877f, 0.0000001f) ||
        !close_enough(booster.coefficients[1].a1,
                      -0.064660802483558655f, 0.0000001f) ||
        !close_enough(booster.coefficients[3].a2,
                      0.28333240747451782f, 0.0000001f)) {
        return 1;
    }
    return 0;
}

static int test_boost_mapping_and_impulse(void)
{
    gp100_boost_t booster;
    float audio[6] = {1.0f, -9.0f, 0.0f, 4.0f, 0.0f, 8.0f};

    if (!gp100_boost_reset(&booster, 0u, false) ||
        !close_enough(booster.drive, 1.0f, 0.0000001f) ||
        !close_enough(booster.input_numerator,
                      -1.4842246770858765f, 0.0000001f) ||
        !close_enough(booster.input_denominator,
                      -0.999797523f, 0.000000001f) ||
        !gp100_boost_process(
            &booster, 0u, false, audio, 3u, true) ||
        !close_enough(audio[0], 0.00039913613f, 0.0000002f) ||
        !close_enough(audio[2], -0.00182743906f, 0.0000002f) ||
        !close_enough(audio[4], -0.00232645124f, 0.0000002f) ||
        audio[0] != audio[1] || audio[2] != audio[3] ||
        audio[4] != audio[5]) {
        return 1;
    }

    audio[0] = 1.0f;
    audio[1] = -9.0f;
    if (!gp100_boost_reset(&booster, 99u, true) ||
        !close_enough(booster.drive, 0.1166815f, 0.0000001f) ||
        !gp100_boost_process(
            &booster, 99u, true, audio, 1u, true) ||
        !close_enough(audio[0], 0.00521470886f, 0.0000003f) ||
        audio[0] != audio[1]) {
        return 2;
    }
    return 0;
}

static int test_boost_bypass_and_live_update(void)
{
    gp100_boost_t booster;
    gp100_boost_t saved;
    float audio[2] = {0.25f, -0.5f};

    if (!gp100_boost_reset(&booster, 0u, false)) {
        return 1;
    }
    saved = booster;
    if (!gp100_boost_process(
            &booster, 99u, true, audio, 1u, false) ||
        memcmp(&booster, &saved, sizeof(booster)) != 0 ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        !gp100_boost_process(
            &booster, 99u, true, audio, 1u, true) ||
        booster.gain != 99u || !booster.bright ||
        !close_enough(booster.drive, 0.1166815f, 0.0000001f) ||
        audio[0] != audio[1]) {
        return 2;
    }
    return 0;
}

static int test_impulse_and_dual_mono(void)
{
    gp100_boost14_t booster;
    float audio[4] = {1.0f, -4.0f, 0.0f, 7.0f};

    if (!gp100_boost14_reset(&booster, 0u) ||
        !gp100_boost14_process(&booster, 0u, audio, 2u, true) ||
        !close_enough(audio[0], 1.75083625f, 0.000002f) ||
        audio[0] != audio[1] || audio[2] != audio[3]) {
        return 1;
    }
    return 0;
}

static int test_live_gain_slew_and_bypass(void)
{
    gp100_boost14_t booster;
    float bypassed[2] = {0.25f, -0.5f};
    float audio[2] = {1.0f, 2.0f};

    if (!gp100_boost14_reset(&booster, 0u) ||
        !gp100_boost14_process(
            &booster, 99u, bypassed, 1u, false) ||
        bypassed[0] != 0.25f || bypassed[1] != -0.5f ||
        booster.current_gain != 0.0f ||
        !close_enough(booster.target_gain, 0.9841f, 0.0000001f) ||
        !gp100_boost14_process(&booster, 99u, audio, 1u, true) ||
        !close_enough(booster.current_gain, 0.0001f, 0.00000001f) ||
        !close_enough(audio[0], 1.7560887f, 0.000002f) ||
        audio[0] != audio[1]) {
        return 1;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_boost_t boost;
    gp100_boost14_t booster;
    float audio[2] = {0.0f, 0.0f};

    if (gp100_boost_reset(NULL, 0u, false) ||
        gp100_boost_reset(&boost, 100u, false) ||
        !gp100_boost_reset(&boost, 0u, false) ||
        gp100_boost_process(
            &boost, 100u, false, audio, 1u, true) ||
        gp100_boost_process(
            &boost, 0u, false, NULL, 1u, true) ||
        gp100_boost14_reset(NULL, 0u) ||
        gp100_boost14_reset(&booster, 100u) ||
        !gp100_boost14_reset(&booster, 0u) ||
        gp100_boost14_process(&booster, 100u, audio, 1u, true) ||
        gp100_boost14_process(&booster, 0u, NULL, 1u, true)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_boost_mapping_and_impulse();

    if (result != 0) {
        fprintf(stderr, "Boost model test failed: %d\n", result);
        return 10 + result;
    }
    result = test_boost_bypass_and_live_update();
    if (result != 0) {
        fprintf(stderr, "Boost bypass test failed: %d\n", result);
        return 20 + result;
    }
    result = test_coefficients_and_gain_curve();

    if (result != 0) {
        fprintf(stderr, "coefficient test failed: %d\n", result);
        return 30 + result;
    }
    result = test_impulse_and_dual_mono();
    if (result != 0) {
        fprintf(stderr, "impulse test failed: %d\n", result);
        return 40 + result;
    }
    result = test_live_gain_slew_and_bypass();
    if (result != 0) {
        fprintf(stderr, "gain test failed: %d\n", result);
        return 50 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 60 + result;
    }
    return 0;
}
