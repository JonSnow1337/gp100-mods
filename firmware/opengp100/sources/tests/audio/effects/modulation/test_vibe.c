#include "audio/effects/modulation/vibe.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_vibe_t vibe;
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int stereo_observed = 0;

    if (!gp100_vibe_reset(&vibe, 50u, 0.5f, false) ||
        vibe.current_depth != 0.5f || vibe.current_rate_step != 0.0f ||
        !gp100_vibe_process(&vibe, 99u, 10.0f, false, 500u,
                                 audio, 1u, true) ||
        !(vibe.current_depth > 0.5f) ||
        !(vibe.current_rate_step > 0.0f) ||
        !isfinite(audio[0]) || !isfinite(audio[1])) {
        fprintf(stderr, "Vibe control/state test failed\n");
        return 1;
    }

    for (frame = 0u; frame < 8192u; ++frame) {
        audio[0] = sinf((float)frame * 0.021f) * 0.2f;
        audio[1] = -0.75f;
        if (!gp100_vibe_process(&vibe, 80u, 3.0f, false, 500u,
                                     audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            fprintf(stderr, "Vibe stability test failed\n");
            return 2;
        }
        if (fabsf(audio[0] - audio[1]) > 0.000001f) {
            stereo_observed = 1;
        }
    }
    if (!stereo_observed) {
        fprintf(stderr, "Vibe stereo-motion test failed\n");
        return 3;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_vibe_process(&vibe, 50u, 4.0f, true, 500u,
                                 audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        gp100_vibe_reset(NULL, 50u, 0.5f, false) ||
        gp100_vibe_reset(&vibe, 100u, 0.5f, false) ||
        gp100_vibe_process(&vibe, 50u, 11.0f, false, 500u,
                                audio, 1u, true) ||
        gp100_vibe_process(&vibe, 50u, 10.5f, true, 500u,
                                audio, 1u, true) ||
        gp100_vibe_process(&vibe, 50u, 4.0f, true, 0u,
                                audio, 1u, true) ||
        gp100_vibe_process(&vibe, 50u, 0.5f, false, 500u,
                                NULL, 1u, true)) {
        fprintf(stderr, "Vibe bypass/validation test failed\n");
        return 4;
    }
    if (!gp100_vibe_reset(&vibe, 9u, 0.5f, true) ||
        !gp100_vibe_process(&vibe, 9u, 0.5f, true, 500u,
                                audio, 1u, true) ||
        fabsf(vibe.target_rate_step - 1.0f / 22050.0f) > 1.0e-10f) {
        fprintf(stderr, "Vibe fractional rate did not use the quarter-note fallback\n");
        return 5;
    }
    return 0;
}
