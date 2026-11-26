#include "audio/effects/delay/echo_999.h"

#include <math.h>
#include <stdio.h>

static gp100_echo_999_t echo_state;

int main(void)
{
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int echo_observed = 0;

    if (!gp100_echo_999_reset(&echo_state, 20u, 20.0f, 50u,
                                   false, true) ||
        !gp100_echo_999_process(&echo_state, 99u, 20.0f, 99u,
                                     false, true, 500u, audio, 1u, true) ||
        echo_state.current_delay_samples != 882.0f ||
        !(echo_state.current_mix > 0.09f) ||
        !(echo_state.current_feedback > 0.4f) || audio[0] != audio[1]) {
        fprintf(stderr, "999 Echo control/routing test failed\n");
        return 1;
    }
    gp100_echo_999_reset(&echo_state, 80u, 20.0f, 50u, false, true);
    for (frame = 0u; frame < 2200u; ++frame) {
        audio[0] = frame == 0u ? 0.25f : 0.0f;
        audio[1] = frame == 0u ? -0.125f : 0.0f;
        if (!gp100_echo_999_process(&echo_state, 80u, 20.0f, 50u,
                                         false, true, 500u,
                                         audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1]) ||
            audio[0] != audio[1]) {
            fprintf(stderr, "999 Echo stability test failed\n");
            return 2;
        }
        if (frame > 850u && fabsf(audio[0]) > 0.000001f) {
            echo_observed = 1;
        }
    }
    if (!echo_observed) {
        fprintf(stderr, "999 Echo impulse test failed\n");
        return 3;
    }
    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_echo_999_process(&echo_state, 20u, 4.0f, 30u,
                                     true, false, 500u, audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        gp100_echo_999_process(&echo_state, 20u, 10.5f, 30u,
                                    true, true, 500u, audio, 1u, true) ||
        gp100_echo_999_process(&echo_state, 20u, 4.0f, 30u,
                                    true, true, 0u, audio, 1u, true)) {
        fprintf(stderr, "999 Echo validation test failed\n");
        return 4;
    }
    return 0;
}
