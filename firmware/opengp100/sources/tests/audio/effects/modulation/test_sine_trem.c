#include "audio/effects/modulation/sine_trem.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_sine_trem_t tremolo;
    float audio[2] = {0.25f, -0.5f};
    size_t frame;
    int motion_observed = 0;
    float previous = 0.0f;

    if (!gp100_sine_trem_reset(&tremolo, 50u, 0.5f, 50u, false) ||
        tremolo.current_depth != 0.5f || tremolo.current_level != 0.5f ||
        tremolo.current_rate_step != 0.0f ||
        !gp100_sine_trem_process(&tremolo, 99u, 10.0f, 99u, false,
                                      500u, audio, 1u, true) ||
        !(tremolo.current_depth > 0.5f) ||
        !(tremolo.current_level > 0.5f) ||
        !(tremolo.current_rate_step > 0.0f) ||
        !isfinite(audio[0]) || !isfinite(audio[1])) {
        fprintf(stderr, "Sine Trem control/state test failed\n");
        return 1;
    }

    for (frame = 0u; frame < 8192u; ++frame) {
        audio[0] = 0.2f;
        audio[1] = -0.75f;
        if (!gp100_sine_trem_process(&tremolo, 80u, 3.0f, 80u, false,
                                          500u, audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            fprintf(stderr, "Sine Trem stability test failed\n");
            return 2;
        }
        if (frame != 0u && fabsf(audio[0] - previous) > 0.000001f) {
            motion_observed = 1;
        }
        previous = audio[0];
    }
    if (!motion_observed || fabsf(audio[0] - audio[1]) > 0.000001f) {
        fprintf(stderr, "Sine Trem mono-motion test failed\n");
        return 3;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_sine_trem_process(&tremolo, 50u, 4.0f, 50u, true,
                                      500u, audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        gp100_sine_trem_reset(NULL, 50u, 0.5f, 50u, false) ||
        gp100_sine_trem_reset(&tremolo, 100u, 0.5f, 50u, false) ||
        gp100_sine_trem_reset(&tremolo, 50u, 0.5f, 100u, false) ||
        gp100_sine_trem_process(&tremolo, 50u, 11.0f, 50u, false,
                                     500u, audio, 1u, true) ||
        gp100_sine_trem_process(&tremolo, 50u, 10.5f, 50u, true,
                                     500u, audio, 1u, true) ||
        gp100_sine_trem_process(&tremolo, 50u, 4.0f, 50u, true,
                                     0u, audio, 1u, true) ||
        gp100_sine_trem_process(&tremolo, 50u, 0.5f, 50u, false,
                                     500u, NULL, 1u, true)) {
        fprintf(stderr, "Sine Trem bypass/validation test failed\n");
        return 4;
    }
    return 0;
}
