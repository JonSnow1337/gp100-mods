#include "audio/effects/wah/auto_wah.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static int test_unsynced_rate_and_impulse(void)
{
    static const float expected_steps[3] = {
        0.00000226759994f, 0.0000299924120f, 0.000224088551f
    };
    static const uint8_t rates[3] = {0u, 50u, 99u};
    size_t index;

    for (index = 0u; index < 3u; ++index) {
        gp100_auto_wah_t wah;
        float audio[2] = {1.0f, -1.0f};

        if (!gp100_auto_wah_reset(
                &wah, 50u, rates[index], 50u, 50u, 50u, 50u, false) ||
            !gp100_auto_wah_process(
                &wah, 50u, rates[index], 50u, 50u, 50u, 50u, false,
                500u, audio, 1u, true) ||
            !close_enough(wah.target_phase_step,
                          expected_steps[index], 0.0000000002f) ||
            !close_enough(wah.current_phase_step,
                          expected_steps[index] * 0.0002267600066f,
                          0.00000000001f) ||
            !close_enough(audio[0], 0.01127751f, 0.00000003f) ||
            audio[0] != audio[1]) {
            return 1 + (int)index;
        }
    }
    return 0;
}

static int test_tempo_divisions(void)
{
    static const float expected_steps[3] = {
        0.000022675737f, 0.000090702946f, 0.000362811785f
    };
    static const float rates[3] = {0.5f, 4.5f, 10.0f};
    size_t index;

    for (index = 0u; index < 3u; ++index) {
        gp100_auto_wah_t wah;

        if (!gp100_auto_wah_reset(
                &wah, 0u, rates[index], 0u, 0u, 0u, 0u, true) ||
            !gp100_auto_wah_process(
                &wah, 0u, rates[index], 0u, 0u, 0u, 0u, true,
                500u, NULL, 0u, true) ||
            !close_enough(wah.target_phase_step,
                          expected_steps[index], 0.0000000005f)) {
            return 1 + (int)index;
        }
    }
    return 0;
}

static int test_lfo_reflection(void)
{
    gp100_auto_wah_t wah;
    float audio[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if (!gp100_auto_wah_reset(
            &wah, 99u, 99u, 99u, 99u, 99u, 99u, false)) {
        return 1;
    }
    wah.phase = 0.9999f;
    wah.current_phase_step = 0.001f;
    if (!gp100_auto_wah_process(
            &wah, 99u, 99u, 99u, 99u, 99u, 99u, false,
            500u, audio, 1u, true) ||
        wah.phase != 1.0f || wah.phase_direction != -1.0f) {
        return 2;
    }
    wah.phase = 0.0001f;
    wah.phase_direction = -1.0f;
    wah.current_phase_step = 0.001f;
    if (!gp100_auto_wah_process(
            &wah, 99u, 99u, 99u, 99u, 99u, 99u, false,
            500u, &audio[2], 1u, true) ||
        wah.phase != 0.0f || wah.phase_direction != 1.0f) {
        return 3;
    }
    return 0;
}

static int test_control_smoothing(void)
{
    gp100_auto_wah_t wah;
    float audio[2] = {0.0f, 0.0f};

    if (!gp100_auto_wah_reset(
            &wah, 0u, 0u, 0u, 0u, 0u, 0u, false) ||
        !gp100_auto_wah_process(
            &wah, 99u, 0u, 99u, 0u, 0u, 0u, false,
            500u, audio, 1u, true) ||
        !close_enough(wah.current_depth, 0.00022449241f, 0.000000001f) ||
        !close_enough(wah.current_output_level,
                      0.00022449241f, 0.000000001f)) {
        return 1;
    }
    return 0;
}

static int test_bypass_and_validation(void)
{
    gp100_auto_wah_t wah;
    gp100_biquad_channel_state_t saved_filter;
    float saved_phase;
    float saved_depth;
    float audio[2] = {0.25f, -0.5f};

    if (!gp100_auto_wah_reset(
            &wah, 0u, 0u, 0u, 0u, 0u, 0u, false)) {
        return 1;
    }
    saved_filter = wah.filter_state;
    saved_phase = wah.phase;
    saved_depth = wah.current_depth;
    if (!gp100_auto_wah_process(
            &wah, 99u, 10u, 99u, 99u, 99u, 99u, true,
            500u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        wah.phase != saved_phase || wah.current_depth != saved_depth ||
        memcmp(&wah.filter_state, &saved_filter, sizeof(saved_filter)) != 0 ||
        !wah.tempo_sync || wah.rate != 10u ||
        !close_enough(wah.target_depth, 0.99f, 0.0000001f) ||
        !close_enough(wah.target_output_level, 0.99f, 0.0000001f) ||
        !close_enough(wah.low_hz, 497.0f, 0.0001f) ||
        !close_enough(wah.high_hz, 4960.0f, 0.0001f)) {
        return 2;
    }
    if (gp100_auto_wah_reset(
            NULL, 0u, 0u, 0u, 0u, 0u, 0u, false) ||
        gp100_auto_wah_reset(
            &wah, 100u, 0u, 0u, 0u, 0u, 0u, false) ||
        gp100_auto_wah_reset(
            &wah, 0u, 11u, 0u, 0u, 0u, 0u, true) ||
        gp100_auto_wah_process(
            &wah, 0u, 0u, 0u, 0u, 0u, 0u, true,
            0u, audio, 1u, true) ||
        gp100_auto_wah_process(
            &wah, 0u, 0u, 0u, 0u, 0u, 0u, false,
            500u, NULL, 1u, true)) {
        return 3;
    }
    return 0;
}

int main(void)
{
    int result = test_unsynced_rate_and_impulse();

    if (result != 0) {
        fprintf(stderr, "A-Wah rate/impulse test failed: %d\n", result);
        return 10 + result;
    }
    result = test_tempo_divisions();
    if (result != 0) {
        fprintf(stderr, "A-Wah tempo test failed: %d\n", result);
        return 20 + result;
    }
    result = test_lfo_reflection();
    if (result != 0) {
        fprintf(stderr, "A-Wah LFO test failed: %d\n", result);
        return 30 + result;
    }
    result = test_control_smoothing();
    if (result != 0) {
        fprintf(stderr, "A-Wah smoothing test failed: %d\n", result);
        return 40 + result;
    }
    result = test_bypass_and_validation();
    if (result != 0) {
        fprintf(stderr, "A-Wah bypass/validation test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
