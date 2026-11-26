#include "audio/effects/delay/dual_echo.h"

#include <math.h>

static gp100_dual_echo_t state;

int main(void)
{
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int left_seen = 0;
    int right_seen = 0;

    if (!gp100_dual_echo_reset(&state, 20u, 50u, 20.0f,
                                     30u, 60u, 40.0f,
                                     false, false, true) ||
        !gp100_dual_echo_process(&state, 99u, 99u, 20.0f,
                                      98u, 98u, 40.0f,
                                      false, false, true, 500u,
                                      audio, 1u, true) ||
        state.current_delay_samples[0] != 882.0f ||
        state.current_delay_samples[1] != 1764.0f ||
        !(state.current_mix[0] > 0.2f) ||
        !(state.current_feedback[1] > 0.6f)) {
        return 1;
    }

    if (!gp100_dual_echo_reset(&state, 80u, 40u, 20.0f,
                                     80u, 40u, 40.0f,
                                     false, false, true)) {
        return 2;
    }
    for (frame = 0u; frame < 2500u; ++frame) {
        audio[0] = frame == 0u ? 0.25f : 0.0f;
        audio[1] = frame == 0u ? -0.125f : 0.0f;
        if (!gp100_dual_echo_process(&state, 80u, 40u, 20.0f,
                                          80u, 40u, 40.0f,
                                          false, false, true, 500u,
                                          audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            return 3;
        }
        if (frame > 875u && fabsf(audio[0]) > 0.000001f) {
            left_seen = 1;
        }
        if (frame > 1750u && fabsf(audio[1]) > 0.000001f) {
            right_seen = 1;
        }
    }
    if (!left_seen || !right_seen) {
        return 4;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_dual_echo_process(&state, 20u, 30u, 20.0f,
                                      20u, 30u, 20.0f,
                                      false, false, false, 500u,
                                      audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        !gp100_dual_echo_process(&state, 20u, 30u, 10.0f,
                                      20u, 30u, 9.0f,
                                      true, true, true, 500u,
                                      audio, 1u, true) ||
        state.current_delay_samples[0] != 5512.5f ||
        fabsf(state.current_delay_samples[1] - 7350.0f) > 0.001f ||
        gp100_dual_echo_process(&state, 20u, 30u, 11.0f,
                                     20u, 30u, 9.0f,
                                     true, true, true, 500u,
                                     audio, 1u, true)) {
        return 5;
    }
    return 0;
}
