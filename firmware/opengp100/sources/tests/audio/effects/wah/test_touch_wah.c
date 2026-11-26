#include "audio/effects/wah/touch_wah.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static int test_mode_impulses(void)
{
    static const float expected_output[2] = {
        0.5114268064f, 0.5045807362f
    };
    static const float expected_cutoff[2] = {
        250.0838928f, 100.0408173f
    };
    uint8_t mode;

    for (mode = 0u; mode < 2u; ++mode) {
        gp100_touch_wah_t wah;
        float audio[2] = {1.0f, -9.0f};

        if (!gp100_touch_wah_reset(
                &wah, 50u, 50u, 50u, 50u, mode) ||
            !gp100_touch_wah_process(
                &wah, 50u, 50u, 50u, 50u, mode, audio, 1u, true) ||
            !close_enough(audio[0], expected_output[mode], 0.000002f) ||
            audio[0] != audio[1] ||
            !close_enough(wah.envelope, 0.00022675737f, 0.00000001f) ||
            !close_enough(wah.envelope_position,
                          0.000045351477f, 0.00000001f) ||
            !close_enough(wah.cutoff_hz,
                          expected_cutoff[mode], 0.0002f)) {
            return 1 + mode;
        }
    }
    return 0;
}

static int test_bypass_and_parameter_update(void)
{
    gp100_touch_wah_t wah;
    gp100_biquad_channel_state_t saved_state;
    float audio[2] = {0.25f, -0.5f};

    if (!gp100_touch_wah_reset(&wah, 0u, 0u, 0u, 0u, 0u)) {
        return 1;
    }
    saved_state = wah.filter_state;
    if (!gp100_touch_wah_process(
            &wah, 99u, 99u, 99u, 99u, 3u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        !close_enough(wah.sense, 0.99f, 0.0000001f) ||
        !close_enough(wah.range, 0.99f, 0.0000001f) ||
        !close_enough(wah.resonance, 0.99f, 0.0000001f) ||
        !close_enough(wah.mix, 0.99f, 0.0000001f) ||
        wah.mode != 3u || wah.envelope != 0.0f ||
        memcmp(&wah.filter_state, &saved_state, sizeof(saved_state)) != 0) {
        return 2;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_touch_wah_t wah;
    float audio[2] = {0.0f, 0.0f};

    if (gp100_touch_wah_reset(NULL, 0u, 0u, 0u, 0u, 0u) ||
        gp100_touch_wah_reset(&wah, 100u, 0u, 0u, 0u, 0u) ||
        gp100_touch_wah_reset(&wah, 0u, 0u, 0u, 0u, 4u) ||
        !gp100_touch_wah_reset(&wah, 0u, 0u, 0u, 0u, 0u) ||
        gp100_touch_wah_process(
            &wah, 0u, 0u, 0u, 100u, 0u, audio, 1u, true) ||
        gp100_touch_wah_process(
            &wah, 0u, 0u, 0u, 0u, 0u, NULL, 1u, true)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_mode_impulses();

    if (result != 0) {
        fprintf(stderr, "T-Wah mode test failed: %d\n", result);
        return 10 + result;
    }
    result = test_bypass_and_parameter_update();
    if (result != 0) {
        fprintf(stderr, "T-Wah bypass test failed: %d\n", result);
        return 20 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "T-Wah validation test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
