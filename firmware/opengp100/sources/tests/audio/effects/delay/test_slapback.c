#include "audio/effects/delay/slapback.h"

#include <math.h>
#include <stdio.h>

static gp100_slapback_t state;

int main(void)
{
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int observed = 0;
    if (!gp100_slapback_reset(&state, 20u, 20.0f, 50u, true) ||
        !gp100_slapback_process(&state, 99u, 20.0f, 99u, true,
                                     audio, 1u, true) ||
        state.current_delay_samples != 882.0f ||
        !(state.current_mix > 0.2f) || !(state.current_feedback > 0.5f)) {
        return 1;
    }
    gp100_slapback_reset(&state, 80u, 20.0f, 50u, true);
    for (frame = 0u; frame < 2100u; ++frame) {
        audio[0] = frame == 0u ? 0.25f : 0.0f;
        audio[1] = frame == 0u ? -0.125f : 0.0f;
        if (!gp100_slapback_process(&state, 80u, 20.0f, 50u, true,
                                         audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            return 2;
        }
        if (frame > 880u &&
            (fabsf(audio[0]) > 0.000001f || fabsf(audio[1]) > 0.000001f)) {
            observed = 1;
        }
    }
    if (!observed) {
        return 3;
    }
    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_slapback_process(&state, 20u, 20.0f, 30u, false,
                                     audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        !gp100_slapback_process(&state, 20u, 4000.0f, 30u, true,
                                     audio, 1u, true) ||
        state.target_delay_samples != 13230.0f ||
        gp100_slapback_process(&state, 20u, 4001.0f, 30u, true,
                                    audio, 1u, true)) {
        return 4;
    }
    return 0;
}
