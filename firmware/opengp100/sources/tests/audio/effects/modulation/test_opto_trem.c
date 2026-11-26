#include "audio/effects/modulation/opto_trem.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_opto_trem_t tremolo;
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int stereo_observed = 0;

    if (!gp100_opto_trem_reset(&tremolo, 50u, 0.5f, false) ||
        tremolo.current_depth != 0.5f ||
        tremolo.current_rate_step != 0.0f ||
        !gp100_opto_trem_process(&tremolo, 99u, 10.0f, false, 500u,
                                      audio, 1u, true) ||
        !(tremolo.current_depth > 0.5f) ||
        !(tremolo.current_rate_step > 0.0f) ||
        !isfinite(audio[0]) || !isfinite(audio[1])) {
        fprintf(stderr, "Opto Trem control/state test failed\n");
        return 1;
    }

    for (frame = 0u; frame < 8192u; ++frame) {
        audio[0] = sinf((float)frame * 0.021f) * 0.2f;
        audio[1] = -0.75f;
        if (!gp100_opto_trem_process(&tremolo, 80u, 3.0f, false,
                                          500u, audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            fprintf(stderr, "Opto Trem stability test failed\n");
            return 2;
        }
        if (fabsf(audio[0] - audio[1]) > 0.000001f) {
            stereo_observed = 1;
        }
    }
    if (!stereo_observed) {
        fprintf(stderr, "Opto Trem stereo-motion test failed\n");
        return 3;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_opto_trem_process(&tremolo, 50u, 4.0f, true, 500u,
                                      audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        gp100_opto_trem_reset(NULL, 50u, 0.5f, false) ||
        gp100_opto_trem_reset(&tremolo, 100u, 0.5f, false) ||
        gp100_opto_trem_process(&tremolo, 50u, 11.0f, false, 500u,
                                     audio, 1u, true) ||
        gp100_opto_trem_process(&tremolo, 50u, 10.5f, true, 500u,
                                     audio, 1u, true) ||
        gp100_opto_trem_process(&tremolo, 50u, 4.0f, true, 0u,
                                     audio, 1u, true) ||
        gp100_opto_trem_process(&tremolo, 50u, 0.5f, false, 500u,
                                     NULL, 1u, true)) {
        fprintf(stderr, "Opto Trem bypass/validation test failed\n");
        return 4;
    }
    return 0;
}
