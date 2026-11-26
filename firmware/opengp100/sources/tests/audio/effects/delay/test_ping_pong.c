#include "audio/effects/delay/ping_pong.h"

#include <math.h>

static gp100_ping_pong_t state;

int main(void)
{
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int first_left_seen = 0;
    int second_right_seen = 0;

    if (!gp100_ping_pong_reset(&state, 0u, 50u, 20.0f,
                                     false, true) ||
        !gp100_ping_pong_process(&state, 0u, 50u, 20.0f,
                                      false, true, 500u,
                                      audio, 1u, true) ||
        audio[0] != -0.5f || audio[1] != 0.25f ||
        state.current_delay_samples != 882.0f) {
        return 1;
    }

    if (!gp100_ping_pong_reset(&state, 80u, 60u, 20.0f,
                                     false, true)) {
        return 2;
    }
    for (frame = 0u; frame < 3000u; ++frame) {
        audio[0] = frame == 0u ? 0.25f : 0.0f;
        audio[1] = 0.0f;
        if (!gp100_ping_pong_process(&state, 80u, 60u, 20.0f,
                                          false, true, 500u,
                                          audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            return 3;
        }
        if (frame > 875u && frame < 1700u &&
            fabsf(audio[0]) > 0.000001f) {
            first_left_seen = 1;
        }
        if (frame > 1750u && fabsf(audio[1]) > 0.000001f) {
            second_right_seen = 1;
        }
    }
    if (!first_left_seen || !second_right_seen) {
        return 4;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_ping_pong_process(&state, 20u, 30u, 20.0f,
                                      false, false, 500u,
                                      audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        !gp100_ping_pong_process(&state, 20u, 30u, 10.0f,
                                      true, true, 500u,
                                      audio, 1u, true) ||
        state.target_delay_samples != 5512.5f ||
        state.current_delay_samples != 5512.5f ||
        gp100_ping_pong_process(&state, 20u, 30u, 10.5f,
                                     true, true, 500u,
                                     audio, 1u, true)) {
        return 5;
    }
    return 0;
}
