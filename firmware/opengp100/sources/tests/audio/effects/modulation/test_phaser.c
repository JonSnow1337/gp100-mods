#include "audio/effects/modulation/phaser.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_phaser_t phaser;
    float audio[2] = {0.25f, -0.5f};
    float impulse[128] = {0.25f, -0.75f};
    size_t frame;
    int stereo_observed = 0;

    if (!gp100_phaser_reset(&phaser, 0.5f, false) ||
        phaser.current_rate_step != 0.0f ||
        !gp100_phaser_process(&phaser, 10.0f, false, 500u,
                                   audio, 1u, true) ||
        !(phaser.current_rate_step > 0.0f) ||
        !isfinite(audio[0]) || !isfinite(audio[1])) {
        fprintf(stderr, "Phaser control/state test failed\n");
        return 1;
    }

    for (frame = 0u; frame < 8192u; ++frame) {
        audio[0] = sinf((float)frame * 0.021f) * 0.2f;
        audio[1] = -0.75f;
        if (!gp100_phaser_process(&phaser, 3.0f, false, 500u,
                                       audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            fprintf(stderr, "Phaser stability test failed\n");
            return 2;
        }
        if (fabsf(audio[0] - audio[1]) > 0.000001f) {
            stereo_observed = 1;
        }
    }
    if (!stereo_observed) {
        fprintf(stderr, "Phaser stereo-motion test failed\n");
        return 3;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_phaser_process(&phaser, 4.0f, true, 500u,
                                   audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        gp100_phaser_reset(NULL, 0.5f, false) ||
        gp100_phaser_reset(&phaser, 0.0f, false) ||
        gp100_phaser_process(&phaser, 11.0f, false, 500u,
                                  audio, 1u, true) ||
        gp100_phaser_process(&phaser, 10.5f, true, 500u,
                                  audio, 1u, true) ||
        gp100_phaser_process(&phaser, 4.0f, true, 0u,
                                  audio, 1u, true) ||
        gp100_phaser_process(&phaser, 0.5f, false, 500u,
                                  NULL, 1u, true)) {
        fprintf(stderr, "Phaser bypass/validation test failed\n");
        return 4;
    }
    if (!gp100_phaser_process(&phaser, 0.5f, true, 500u,
                                   audio, 1u, true) ||
        phaser.target_rate_step != 1.0f / 22050.0f) {
        return 5;
    }
    if (!gp100_phaser_reset(&phaser, 0.5f, true) ||
        !gp100_phaser_process(&phaser, 0.5f, true, 500u,
                                   impulse, 64u, true) ||
        fabsf(impulse[2] - -0.19560356f) > 0.000001f ||
        fabsf(impulse[32] - -0.000467376f) > 0.0000001f ||
        fabsf(impulse[33] - -0.000467894f) > 0.0000001f) {
        fprintf(stderr, "Phaser factory impulse response mismatch\n");
        return 6;
    }
    return 0;
}
