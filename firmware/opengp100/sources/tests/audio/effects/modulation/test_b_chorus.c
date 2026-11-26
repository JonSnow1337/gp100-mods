#include "audio/effects/modulation/b_chorus.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float left, float right, float tolerance)
{
    return fabsf(left - right) <= tolerance;
}

int main(void)
{
    gp100_b_chorus_t chorus;
    float audio[2] = {0.25f, -0.75f};
    size_t frame;
    int stereo_difference = 0;

    if (!gp100_b_chorus_reset(&chorus, 50u, 0.5f, 50u, false) ||
        !close_enough(chorus.current_depth, 0.35f, 0.000001f) ||
        !close_enough(chorus.current_tone, 0.5f, 0.000001f) ||
        !gp100_b_chorus_process(&chorus, 99u, 10.0f, 99u, false,
                                     500u, audio, 1u, true) ||
        !isfinite(audio[0]) || !isfinite(audio[1]) ||
        !close_enough(chorus.current_depth,
                      0.35f + (0.693f - 0.35f) * 0x1.db8cdap-13f,
                      0.000001f) ||
        !close_enough(chorus.current_tone,
                      0.5f + 0.49f * 0x1.db8cdap-13f, 0.000001f) ||
        !close_enough(chorus.current_rate_step,
                      (10.0f / 44100.0f) * 0x1.db8cdap-13f,
                      0.000000001f)) {
        fprintf(stderr, "B-Chorus control/signal test failed\n");
        return 1;
    }

    for (frame = 0u; frame < 4096u; ++frame) {
        audio[0] = frame == 0u ? 0.5f : 0.0f;
        audio[1] = 0.0f;
        if (!gp100_b_chorus_process(&chorus, 99u, 7.0f, 70u, true,
                                         500u, audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            fprintf(stderr, "B-Chorus stability test failed\n");
            return 2;
        }
        if (fabsf(audio[0] - audio[1]) > 0.000001f) {
            stereo_difference = 1;
        }
    }
    if (!stereo_difference || chorus.write_index[0] != chorus.write_index[1]) {
        fprintf(stderr, "B-Chorus stereo-delay test failed\n");
        return 3;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_b_chorus_process(&chorus, 0u, 0.1f, 0u, false,
                                     0u, audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        gp100_b_chorus_reset(NULL, 0u, 0.1f, 0u, false) ||
        gp100_b_chorus_reset(&chorus, 100u, 0.1f, 0u, false) ||
        gp100_b_chorus_process(&chorus, 0u, 11.0f, 0u, true,
                                    500u, audio, 1u, true) ||
        gp100_b_chorus_process(&chorus, 0u, 4.0f, 0u, true,
                                    0u, audio, 1u, true) ||
        gp100_b_chorus_process(&chorus, 0u, 0.1f, 0u, false,
                                    500u, NULL, 1u, true)) {
        fprintf(stderr, "B-Chorus bypass/validation test failed\n");
        return 4;
    }
    return 0;
}
