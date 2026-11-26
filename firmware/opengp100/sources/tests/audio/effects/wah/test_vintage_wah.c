#include "audio/effects/wah/vintage_wah.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static int test_impulse_and_sweep(void)
{
    gp100_vintage_wah_t wah;
    float audio[2] = {1.0f, -1.0f};

    if (!gp100_vintage_wah_reset(&wah, 50u, 50u, 50u) ||
        !gp100_vintage_wah_process(
            &wah, 50u, 50u, 50u, audio, 1u, true) ||
        !close_enough(wah.sweep_position, 0.2f, 0.0000001f) ||
        !close_enough(wah.cutoff_hz, 808.0f, 0.0002f) ||
        !close_enough(audio[0], 0.21372025f, 0.0000003f) ||
        audio[0] != audio[1]) {
        return 1;
    }
    return 0;
}

static int test_bypass_smoothing(void)
{
    gp100_vintage_wah_t wah;
    gp100_biquad_channel_state_t input_state;
    gp100_biquad_channel_state_t wah_state;
    float audio[2] = {0.25f, -0.5f};

    if (!gp100_vintage_wah_reset(&wah, 0u, 0u, 0u)) {
        return 1;
    }
    input_state = wah.input_filter_state;
    wah_state = wah.wah_filter_state;
    if (!gp100_vintage_wah_process(
            &wah, 99u, 99u, 99u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        !close_enough(wah.range, 0.99f, 0.0000001f) ||
        !close_enough(wah.current_resonance, 0.0001f, 0.00000001f) ||
        !close_enough(wah.current_output_level, 0.0001f, 0.00000001f) ||
        memcmp(&wah.input_filter_state, &input_state, sizeof(input_state)) != 0 ||
        memcmp(&wah.wah_filter_state, &wah_state, sizeof(wah_state)) != 0) {
        return 2;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_vintage_wah_t wah;
    float audio[2] = {0.0f, 0.0f};

    if (gp100_vintage_wah_reset(NULL, 0u, 0u, 0u) ||
        gp100_vintage_wah_reset(&wah, 100u, 0u, 0u) ||
        !gp100_vintage_wah_reset(&wah, 0u, 0u, 0u) ||
        gp100_vintage_wah_process(
            &wah, 0u, 100u, 0u, audio, 1u, true) ||
        gp100_vintage_wah_process(
            &wah, 0u, 0u, 0u, NULL, 1u, true)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_impulse_and_sweep();

    if (result != 0) {
        fprintf(stderr, "V-Wah impulse test failed: %d\n", result);
        return 10 + result;
    }
    result = test_bypass_smoothing();
    if (result != 0) {
        fprintf(stderr, "V-Wah bypass test failed: %d\n", result);
        return 20 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "V-Wah validation test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
