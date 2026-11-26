#include "audio/effects/delay/m_echo2.h"

#include <math.h>

static gp100_m_echo2_t state;

int main(void)
{
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int echo_seen = 0;

    if (!gp100_m_echo2_reset(&state, 20u, 20.0f, 50u, 50u,
                                   false, true) ||
        !gp100_m_echo2_process(&state, 99u, 20.0f, 99u, 99u,
                                    false, true, 500u,
                                    audio, 1u, true) ||
        state.current_delay_samples != 882.0f ||
        audio[0] != audio[1] || !(state.current_mix > 0.2f) ||
        !(state.current_feedback > 0.5f)) {
        return 1;
    }

    if (!gp100_m_echo2_reset(&state, 80u, 20.0f, 45u, 55u,
                                   false, true)) {
        return 2;
    }
    for (frame = 0u; frame < 2000u; ++frame) {
        audio[0] = frame == 0u ? 0.25f : 0.0f;
        audio[1] = 0.0f;
        if (!gp100_m_echo2_process(&state, 80u, 20.0f, 45u, 55u,
                                        false, true, 500u,
                                        audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) {
            return 3;
        }
        if (frame > 200u && fabsf(audio[0]) > 0.000001f) {
            echo_seen = 1;
        }
    }
    if (!echo_seen) {
        return 4;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_m_echo2_process(&state, 20u, 20.0f, 30u, 20u,
                                    false, false, 500u,
                                    audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        !gp100_m_echo2_process(&state, 20u, 10.0f, 30u, 20u,
                                    true, true, 500u,
                                    audio, 1u, true) ||
        state.current_delay_samples != 5512.5f ||
        gp100_m_echo2_process(&state, 20u, 10.5f, 30u, 20u,
                                   true, true, 500u,
                                   audio, 1u, true)) {
        return 5;
    }
    return 0;
}
