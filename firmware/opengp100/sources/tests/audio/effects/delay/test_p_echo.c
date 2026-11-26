#include "audio/effects/delay/p_echo.h"

#include <math.h>
#include <stdio.h>

static gp100_p_echo_t echo_state;

static bool check_delay_history(void)
{
    const size_t last = GP100_P_ECHO_SAMPLES - 1u;
    float audio[2];
    size_t frame;

    echo_state.delay_line[0][last] = 0.75f;
    echo_state.delay_line[1][last] = -0.25f;
    if (!gp100_p_echo_reset(&echo_state, 99u, 4000.0f, 0u,
                                false, false) ||
        echo_state.delay_line[0][last] != 0.75f ||
        echo_state.delay_line[1][last] != -0.25f) {
        return false;
    }
    for (frame = 0u; frame <= last; ++frame) {
        audio[0] = frame == 0u ? 0.25f : 0.0f;
        audio[1] = frame == 0u ? -0.50f : 0.0f;
        if (!gp100_p_echo_process(&echo_state, 99u, 4000.0f, 0u,
                                      false, false, 500u, audio, 1u, true)) {
            return false;
        }
        if (frame > 0u && frame < last &&
            (audio[0] != 0.0f || audio[1] != 0.0f)) {
            return false;
        }
    }
    if (audio[0] != 0.25f || audio[1] != -0.50f ||
        !echo_state.delay_full[0] || !echo_state.delay_full[1]) {
        return false;
    }
    if (!gp100_p_echo_process(&echo_state, 99u, 4000.0f, 0u,
                                  false, false, 500u, audio, 1u, false) ||
        echo_state.delay_full[0] || echo_state.delay_full[1] ||
        echo_state.delay_line[0][0] != 0.25f ||
        echo_state.delay_line[1][0] != -0.50f) {
        return false;
    }
    return true;
}

int main(void)
{
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int echo_observed = 0;

    if (!gp100_p_echo_reset(&echo_state, 20u, 20.0f, 50u,
                                 false, true) ||
        !gp100_p_echo_process(&echo_state, 99u, 20.0f, 99u,
                                   false, true, 500u, audio, 1u, true) ||
        !(echo_state.current_mix > 0.2f) ||
        !(echo_state.current_feedback > 0.5f) ||
        echo_state.current_delay_samples != 882.0f) {
        fprintf(stderr, "P-Echo control/state test failed\n");
        return 1;
    }

    for (frame = 0u; frame < 2048u; ++frame) {
        audio[0] = frame == 0u ? 0.25f : 0.0f;
        audio[1] = frame == 0u ? -0.125f : 0.0f;
        if (!gp100_p_echo_process(&echo_state, 80u, 20.0f, 50u,
                                       false, true, 500u,
                                       audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            fprintf(stderr, "P-Echo stability test failed\n");
            return 2;
        }
        if (frame > 850u &&
            (fabsf(audio[0]) > 0.000001f || fabsf(audio[1]) > 0.000001f)) {
            echo_observed = 1;
        }
    }
    if (!echo_observed) {
        fprintf(stderr, "P-Echo impulse test failed\n");
        return 3;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_p_echo_process(&echo_state, 20u, 4.0f, 30u,
                                   true, false, 500u,
                                   audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        gp100_p_echo_reset(NULL, 20u, 500.0f, 30u, false, true) ||
        gp100_p_echo_reset(&echo_state, 100u, 500.0f, 30u,
                                false, true) ||
        gp100_p_echo_process(&echo_state, 20u, 10.0f, 100u,
                                  false, true, 500u,
                                  audio, 1u, true) ||
        gp100_p_echo_process(&echo_state, 20u, 10.5f, 30u,
                                  true, true, 500u,
                                  audio, 1u, true) ||
        gp100_p_echo_process(&echo_state, 20u, 4.0f, 30u,
                                  true, true, 0u,
                                  audio, 1u, true) ||
        gp100_p_echo_process(&echo_state, 20u, 500.0f, 30u,
                                  false, true, 500u,
                                  NULL, 1u, true)) {
        fprintf(stderr, "P-Echo bypass/validation test failed\n");
        return 4;
    }
    if (!gp100_p_echo_process(&echo_state, 20u, 0.5f, 30u,
                                   true, false, 500u, audio, 1u, true) ||
        echo_state.target_delay_samples != 88200.0f ||
        !gp100_p_echo_process(&echo_state, 20u, 500.9f, 30u,
                                   false, false, 500u, audio, 1u, true) ||
        echo_state.target_delay_samples != 22050.0f) {
        fprintf(stderr, "P-Echo fractional time did not truncate before mapping\n");
        return 5;
    }
    if (!check_delay_history()) {
        fprintf(stderr, "P-Echo reset or ring wrap used stale samples\n");
        return 6;
    }
    return 0;
}
