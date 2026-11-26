#include "audio/effects/pitch/detune.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float left, float right, float tolerance)
{
    return fabsf(left - right) <= tolerance;
}

int main(void)
{
    gp100_detune_t detune;
    float audio[2] = {0.25f, -0.75f};
    size_t frame;
    int wet_observed = 0;

    if (!gp100_detune_reset(&detune, 0, 50u, 50u) ||
        detune.channel[0].phase[0] != 0u ||
        detune.channel[0].phase[1] != 0x80000u ||
        detune.window[0] != 0u || detune.window[255] == 0u ||
        !gp100_detune_process(&detune, 50, 99u, 0u,
                                   audio, 1u, true) ||
        !close_enough(detune.current_wet, 0.505f, 0.000001f) ||
        !close_enough(detune.current_dry, 0.495f, 0.000001f) ||
        !isfinite(audio[0]) || !isfinite(audio[1])) {
        fprintf(stderr, "Detune control/state test failed\n");
        return 1;
    }

    for (frame = 0u; frame < 8192u; ++frame) {
        audio[0] = sinf((float)frame * 0.031f) * 0.2f;
        audio[1] = cosf((float)frame * 0.023f) * 0.2f;
        if (!gp100_detune_process(&detune, -50, 99u, 0u,
                                       audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            fprintf(stderr, "Detune stability test failed\n");
            return 2;
        }
        if (fabsf(audio[0]) > 0.000001f || fabsf(audio[1]) > 0.000001f) {
            wet_observed = 1;
        }
    }
    if (!wet_observed) {
        fprintf(stderr, "Detune wet-path test failed\n");
        return 3;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_detune_process(&detune, 0, 0u, 0u,
                                   audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        gp100_detune_reset(NULL, 0, 0u, 0u) ||
        gp100_detune_reset(&detune, -51, 0u, 0u) ||
        gp100_detune_reset(&detune, 51, 0u, 0u) ||
        gp100_detune_process(&detune, 0, 100u, 0u,
                                  audio, 1u, true) ||
        gp100_detune_process(&detune, 0, 0u, 0u,
                                  NULL, 1u, true)) {
        fprintf(stderr, "Detune bypass/validation test failed\n");
        return 4;
    }
    return 0;
}
