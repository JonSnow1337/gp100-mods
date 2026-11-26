#include "audio/effects/delay/sweep_echo.h"

#include <math.h>

static gp100_sweep_echo_t state;

int main(void)
{
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int left_seen = 0;
    int right_seen = 0;

    if (!gp100_sweep_echo_reset(&state, 20u, 50u, 20.0f,
                                      50u, 50u, false, false, true) ||
        !gp100_sweep_echo_process(&state, 99u, 99u, 20.0f,
                                       99u, 99u, false, false, true,
                                       500u, audio, 1u, true) ||
        state.current_delay_samples != 882.0f ||
        !(state.current_mix > 0.2f) ||
        !(state.current_feedback > 0.5f) ||
        !(state.current_depth > 0.5f) ||
        !(state.target_sweep_step > 0.0002f)) {
        return 1;
    }

    if (!gp100_sweep_echo_reset(&state, 80u, 40u, 20.0f,
                                      70u, 70u, false, false, true)) {
        return 2;
    }
    for (frame = 0u; frame < 1800u; ++frame) {
        audio[0] = frame == 0u ? 0.25f : 0.0f;
        audio[1] = frame == 0u ? -0.125f : 0.0f;
        if (!gp100_sweep_echo_process(&state, 80u, 40u, 20.0f,
                                           70u, 70u, false, false, true,
                                           500u, audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            return 3;
        }
        if (frame > 875u && fabsf(audio[0]) > 0.000001f) {
            left_seen = 1;
        }
        if (frame > 875u && fabsf(audio[1]) > 0.000001f) {
            right_seen = 1;
        }
    }
    if (!left_seen || !right_seen) {
        return 4;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_sweep_echo_process(&state, 20u, 30u, 20.0f,
                                       20u, 20u, false, false, false,
                                       500u, audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        !gp100_sweep_echo_process(&state, 20u, 30u, 10.0f,
                                       20u, 10u, true, true, true,
                                       500u, audio, 1u, true) ||
        state.target_delay_samples != 5512.5f ||
        state.current_delay_samples != 5512.5f ||
        state.current_sweep_step != 1.0f / 5512.5f ||
        gp100_sweep_echo_process(&state, 20u, 30u, 4000.5f,
                                      20u, 10u, true, true, true,
                                      500u, audio, 1u, true) ||
        gp100_sweep_echo_process(&state, 20u, 30u, 20.0f,
                                      20u, 100u, false, true, true,
                                      500u, audio, 1u, true)) {
        return 5;
    }
    if (!gp100_sweep_echo_process(&state, 20u, 30u, 500.0f,
                                       20u, 52.0f, true, true, false,
                                       500u, audio, 1u, true) ||
        state.current_delay_samples != 22050.0f ||
        state.current_sweep_step != 1.0f / 22050.0f) {
        return 6;
    }
    return 0;
}
