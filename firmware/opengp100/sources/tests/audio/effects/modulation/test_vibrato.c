#include "audio/effects/modulation/vibrato.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float left, float right, float tolerance)
{
    return fabsf(left - right) <= tolerance;
}

int main(void)
{
    gp100_vibrato_t vibrato;
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int wet_observed = 0;
    int stereo_observed = 0;

    if (!gp100_vibrato_reset(&vibrato, 50u, 0.5f, false) ||
        !close_enough(vibrato.current_depth, 0.5f, 0.000001f) ||
        vibrato.current_rate_step != 0.0f ||
        !gp100_vibrato_process(&vibrato, 99u, 10.0f, false, 500u,
                                    audio, 1u, true) ||
        !(vibrato.current_depth > 0.5f) ||
        !(vibrato.current_rate_step > 0.0f) ||
        !isfinite(audio[0]) || !isfinite(audio[1])) {
        fprintf(stderr, "Vibrato control/state test failed\n");
        return 1;
    }

    for (frame = 0u; frame < 8192u; ++frame) {
        audio[0] = sinf((float)frame * 0.019f) * 0.2f;
        audio[1] = -0.75f;
        if (!gp100_vibrato_process(&vibrato, 80u, 3.0f, false, 500u,
                                        audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            fprintf(stderr, "Vibrato stability test failed\n");
            return 2;
        }
        if (fabsf(audio[0]) > 0.000001f || fabsf(audio[1]) > 0.000001f) {
            wet_observed = 1;
        }
        if (fabsf(audio[0] - audio[1]) > 0.000001f) {
            stereo_observed = 1;
        }
    }
    if (!wet_observed || !stereo_observed) {
        fprintf(stderr, "Vibrato wet/stereo test failed\n");
        return 3;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_vibrato_process(&vibrato, 50u, 4.0f, true, 500u,
                                    audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        gp100_vibrato_reset(NULL, 50u, 0.5f, false) ||
        gp100_vibrato_reset(&vibrato, 100u, 0.5f, false) ||
        gp100_vibrato_process(&vibrato, 50u, 11.0f, false, 500u,
                                   audio, 1u, true) ||
        gp100_vibrato_process(&vibrato, 50u, 10.5f, true, 500u,
                                   audio, 1u, true) ||
        gp100_vibrato_process(&vibrato, 50u, 4.0f, true, 0u,
                                   audio, 1u, true) ||
        gp100_vibrato_process(&vibrato, 50u, 0.5f, false, 500u,
                                   NULL, 1u, true)) {
        fprintf(stderr, "Vibrato bypass/validation test failed\n");
        return 4;
    }
    return 0;
}
