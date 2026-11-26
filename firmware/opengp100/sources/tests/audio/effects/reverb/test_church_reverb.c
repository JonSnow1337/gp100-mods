#include "audio/effects/reverb/church_reverb.h"

#include <math.h>

static gp100_church_reverb_t state;

int main(void)
{
    float audio[2] = {0.25f, -0.125f};
    int wet_seen = 0;
    int stereo_seen = 0;
    size_t frame;

    if (!gp100_church_reverb_reset(&state, 20u, 50u, 0u, true) ||
        fabsf(state.decay - 0.4f) > 0.000001f ||
        !gp100_church_reverb_process(&state, 99u, 100u, 99u, true,
                                          audio, 1u, true) ||
        !(state.current_mix > 0.2f) ||
        !(state.current_predelay > 0.5f) ||
        !(state.decay > 0.948f && state.decay < 0.95f)) {
        return 1;
    }

    if (!gp100_church_reverb_reset(&state, 80u, 0u, 60u, true)) {
        return 2;
    }
    for (frame = 0u; frame < 12000u; ++frame) {
        audio[0] = frame == 0u ? 1.0f : 0.0f;
        audio[1] = frame == 0u ? 1.0f : 0.0f;
        if (!gp100_church_reverb_process(&state, 80u, 0u, 60u, true,
                                              audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            return 3;
        }
        if (frame > 2100u && (fabsf(audio[0]) > 0.000001f ||
                              fabsf(audio[1]) > 0.000001f)) {
            wet_seen = 1;
        }
        if (fabsf(audio[0] - audio[1]) > 0.000001f) {
            stereo_seen = 1;
        }
    }
    if (!wet_seen || !stereo_seen) {
        return 4;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_church_reverb_process(&state, 20u, 50u, 50u, false,
                                          audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        gp100_church_reverb_process(&state, 100u, 50u, 50u, false,
                                         audio, 1u, true) ||
        gp100_church_reverb_process(&state, 20u, 101u, 50u, false,
                                         audio, 1u, true) ||
        gp100_church_reverb_process(&state, 20u, 50u, 100u, false,
                                         audio, 1u, true)) {
        return 5;
    }
    return 0;
}
