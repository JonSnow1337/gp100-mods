#include "audio/effects/dynamics/compressor.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static int test_reset_and_impulse(void)
{
    gp100_comp_t compressor;
    float audio[2] = {0.25f, -9.0f};

    if (!gp100_comp_reset(&compressor, 20u, 50u) ||
        !close_enough(compressor.sustain, 0.0014f, 0.0000001f) ||
        !close_enough(compressor.output_level, 0.5f, 0.0000001f) ||
        !close_enough(compressor.attack_coefficient,
                      0.9835294485f, 0.0000001f) ||
        !close_enough(compressor.release_coefficient,
                      0.9997923970f, 0.0000001f) ||
        !close_enough(compressor.filter_coefficients[0].b0,
                      0.6179965138f, 0.0000001f) ||
        !close_enough(compressor.filter_coefficients[1].b0,
                      1.143829346f, 0.0000001f) ||
        !gp100_comp_process(
            &compressor, 20u, 50u, audio, 1u, true) ||
        !close_enough(audio[0], 0.4414038f, 0.000002f) ||
        audio[0] != audio[1] ||
        !close_enough(compressor.gain_reduction_db,
                      -0.1285841f, 0.000002f)) {
        return 1;
    }

    if (!gp100_comp_reset(&compressor, 99u, 99u) ||
        !close_enough(compressor.sustain, 0.9841f, 0.0000001f)) {
        return 2;
    }
    return 0;
}

static int test_bypass_slew_and_silence(void)
{
    gp100_comp_t compressor;
    gp100_biquad_channel_state_t zero_state;
    float audio[4] = {0.25f, -0.5f, -0.75f, 0.125f};

    memset(&zero_state, 0, sizeof(zero_state));
    if (!gp100_comp_reset(&compressor, 0u, 0u) ||
        !gp100_comp_process(
            &compressor, 99u, 99u, audio, 2u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        audio[2] != -0.75f || audio[3] != 0.125f ||
        !close_enough(compressor.sustain, 0.0002f, 0.00000001f) ||
        !close_enough(compressor.output_level, 0.0002f, 0.00000001f) ||
        compressor.gain_reduction_db != 0.0f ||
        memcmp(&compressor.filter_state[0], &zero_state,
               sizeof(zero_state)) != 0 ||
        memcmp(&compressor.filter_state[1], &zero_state,
               sizeof(zero_state)) != 0) {
        return 1;
    }

    audio[0] = 0.0f;
    audio[1] = 1.0f;
    if (!gp100_comp_reset(&compressor, 50u, 50u) ||
        !gp100_comp_process(
            &compressor, 50u, 50u, audio, 1u, true) ||
        audio[0] != 0.0f || audio[1] != 0.0f ||
        !isfinite(compressor.gain_reduction_db)) {
        return 2;
    }
    return 0;
}

static int test_comp4_mapping_and_impulse(void)
{
    gp100_comp4_t compressor;
    float audio[2] = {1.0f, -4.0f};

    if (!gp100_comp4_reset(&compressor, 20u, 60u, 50u, 10u) ||
        !close_enough(compressor.sustain, 0.0014f, 0.0000001f) ||
        !close_enough(compressor.attack, 0.6f, 0.0000001f) ||
        !close_enough(compressor.output_level, 0.5f, 0.0000001f) ||
        !close_enough(compressor.clip, 0.0092f, 0.0000001f) ||
        !close_enough(compressor.attack_time_seconds,
                      0.0068f, 0.0000001f) ||
        !close_enough(compressor.attack_coefficient,
                      0.9926998f, 0.0000002f) ||
        !close_enough(compressor.release_coefficient,
                      0.9998535f, 0.0000002f) ||
        !close_enough(compressor.prefilter_coefficients.numerator[0],
                      0.003713367f, 0.0000001f) ||
        !close_enough(compressor.prefilter_coefficients.denominator[0],
                      0.012138661f, 0.0000001f) ||
        !gp100_comp4_process(
            &compressor, 20u, 60u, 50u, 10u, audio, 1u, true) ||
        !close_enough(audio[0], 1.4221065f, 0.000004f) ||
        audio[0] != audio[1] ||
        !close_enough(compressor.gain_reduction_db,
                      -0.06966236f, 0.000003f)) {
        return 1;
    }
    return 0;
}

static int test_comp4_bypass_and_live_update(void)
{
    gp100_comp4_t compressor;
    gp100_comp4_prefilter_coefficients_t saved_coefficients;
    gp100_comp4_prefilter_state_t zero_state;
    float saved_attack_coefficient;
    float audio[4] = {0.25f, -0.5f, -0.75f, 0.125f};

    memset(&zero_state, 0, sizeof(zero_state));
    if (!gp100_comp4_reset(&compressor, 0u, 0u, 0u, 0u)) {
        return 1;
    }
    saved_coefficients = compressor.prefilter_coefficients;
    saved_attack_coefficient = compressor.attack_coefficient;
    if (!gp100_comp4_process(
            &compressor, 99u, 99u, 99u, 99u, audio, 2u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        audio[2] != -0.75f || audio[3] != 0.125f ||
        !close_enough(compressor.sustain, 0.0002f, 0.00000001f) ||
        !close_enough(compressor.attack, 0.0002f, 0.00000001f) ||
        !close_enough(compressor.output_level, 0.0002f, 0.00000001f) ||
        !close_enough(compressor.clip, 0.0002f, 0.00000001f) ||
        compressor.attack_coefficient != saved_attack_coefficient ||
        memcmp(&compressor.prefilter_coefficients, &saved_coefficients,
               sizeof(saved_coefficients)) != 0 ||
        memcmp(&compressor.prefilter_state, &zero_state,
               sizeof(zero_state)) != 0) {
        return 2;
    }

    audio[0] = 0.25f;
    audio[1] = 8.0f;
    if (!gp100_comp4_process(
            &compressor, 99u, 99u, 99u, 99u, audio, 1u, true) ||
        !close_enough(compressor.attack, 0.0003f, 0.00000001f) ||
        !close_enough(compressor.clip, 0.0003f, 0.00000001f) ||
        compressor.attack_coefficient == saved_attack_coefficient ||
        memcmp(&compressor.prefilter_coefficients, &saved_coefficients,
               sizeof(saved_coefficients)) == 0 ||
        audio[0] != audio[1]) {
        return 3;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_comp_t compressor;
    gp100_comp4_t compressor4;
    float audio[2] = {0.0f, 0.0f};

    if (gp100_comp_reset(NULL, 0u, 0u) ||
        gp100_comp_reset(&compressor, 100u, 0u) ||
        gp100_comp_reset(&compressor, 0u, 100u) ||
        !gp100_comp_reset(&compressor, 0u, 0u) ||
        gp100_comp_process(
            &compressor, 100u, 0u, audio, 1u, true) ||
        gp100_comp_process(
            &compressor, 0u, 100u, audio, 1u, true) ||
        gp100_comp_process(
            &compressor, 0u, 0u, NULL, 1u, true) ||
        gp100_comp4_reset(NULL, 0u, 0u, 0u, 0u) ||
        gp100_comp4_reset(&compressor4, 100u, 0u, 0u, 0u) ||
        gp100_comp4_reset(&compressor4, 0u, 100u, 0u, 0u) ||
        gp100_comp4_reset(&compressor4, 0u, 0u, 100u, 0u) ||
        gp100_comp4_reset(&compressor4, 0u, 0u, 0u, 100u) ||
        !gp100_comp4_reset(&compressor4, 0u, 0u, 0u, 0u) ||
        gp100_comp4_process(
            &compressor4, 0u, 0u, 0u, 100u, audio, 1u, true) ||
        gp100_comp4_process(
            &compressor4, 0u, 0u, 0u, 0u, NULL, 1u, true)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_reset_and_impulse();

    if (result != 0) {
        fprintf(stderr, "COMP model test failed: %d\n", result);
        return 10 + result;
    }
    result = test_bypass_slew_and_silence();
    if (result != 0) {
        fprintf(stderr, "COMP bypass test failed: %d\n", result);
        return 20 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "COMP validation test failed: %d\n", result);
        return 30 + result;
    }
    result = test_comp4_mapping_and_impulse();
    if (result != 0) {
        fprintf(stderr, "COMP4 model test failed: %d\n", result);
        return 40 + result;
    }
    result = test_comp4_bypass_and_live_update();
    if (result != 0) {
        fprintf(stderr, "COMP4 bypass test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
