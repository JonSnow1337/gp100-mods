#include "audio/effects/delay/sweet_delay.h"

#include <math.h>
#include <stdio.h>

static gp100_sweet_delay_t delay;

int main(void)
{
    float audio[2] = {0.25f, -0.5f};
    float impulse[64u * 2u] = {0.25f, -0.75f};
    size_t frame;
    int echo_observed = 0;

    if (!gp100_sweet_delay_reset(&delay, 20u, 20.0f, 30u,
                                      false, true) ||
        !gp100_sweet_delay_process(&delay, 99u, 20.0f, 99u,
                                        false, true, 500u,
                                        audio, 1u, true) ||
        !(delay.current_mix > 0.2f) || !(delay.current_feedback > 0.0f) ||
        delay.current_delay_samples != 882.0f ||
        !isfinite(audio[0]) || !isfinite(audio[1])) {
        fprintf(stderr, "Sweet delay control/state test failed\n");
        return 1;
    }

    for (frame = 0u; frame < 2048u; ++frame) {
        audio[0] = frame == 0u ? 0.25f : 0.0f;
        audio[1] = -0.75f;
        if (!gp100_sweet_delay_process(&delay, 80u, 20.0f, 50u,
                                            false, true, 500u,
                                            audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            fprintf(stderr, "Sweet delay stability test failed\n");
            return 2;
        }
        if (frame > 850u && fabsf(audio[0]) > 0.000001f) {
            echo_observed = 1;
        }
    }
    if (!echo_observed || fabsf(audio[0] - audio[1]) > 0.000001f) {
        fprintf(stderr, "Sweet delay echo/routing test failed\n");
        return 3;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_sweet_delay_process(&delay, 20u, 4.0f, 30u,
                                        true, false, 500u,
                                        audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        delay.write_index != 0u || delay.last_wet != 0.0f || delay.ring_active ||
        gp100_sweet_delay_reset(NULL, 20u, 500.0f, 30u,
                                     false, true) ||
        gp100_sweet_delay_reset(&delay, 100u, 500.0f, 30u,
                                     false, true) ||
        gp100_sweet_delay_process(&delay, 20u, 10.0f, 100u,
                                       false, true, 500u,
                                       audio, 1u, true) ||
        gp100_sweet_delay_process(&delay, 20u, 10.5f, 30u,
                                       true, true, 500u,
                                       audio, 1u, true) ||
        gp100_sweet_delay_process(&delay, 20u, 4.0f, 30u,
                                       true, true, 0u,
                                       audio, 1u, true) ||
        gp100_sweet_delay_process(&delay, 20u, 500.0f, 30u,
                                       false, true, 500u,
                                       NULL, 1u, true)) {
        fprintf(stderr, "Sweet delay bypass/validation test failed\n");
        return 4;
    }

    if (!gp100_sweet_delay_reset(&delay, 62u, 29.0f, 54u,
                                      false, false) ||
        !gp100_sweet_delay_process(&delay, 62u, 29.0f, 54u,
                                        false, false, 500u,
                                        impulse, 64u, true) ||
        fabsf(impulse[0] - 0.12408927f) > 0.000001f ||
        impulse[0] != impulse[1]) {
        fprintf(stderr, "Sweet delay factory direct impulse mismatch\n");
        return 5;
    }
    for (frame = 64u; frame <= 1280u; frame += 64u) {
        size_t sample;

        for (sample = 0u; sample < 64u * 2u; ++sample) {
            impulse[sample] = 0.0f;
        }
        if (!gp100_sweet_delay_process(&delay, 62u, 29.0f, 54u,
                                            false, false, 500u,
                                            impulse, 64u, true)) {
            return 6;
        }
    }
    if (fabsf(impulse[15u * 2u] - 0.02072680f) > 0.000001f ||
        fabsf(impulse[30u * 2u + 1u] - -0.01014598f) > 0.000001f ||
        fabsf(impulse[40u * 2u] - 0.00344167f) > 0.000001f) {
        fprintf(stderr, "Sweet delay factory echo impulse mismatch\n");
        return 7;
    }
    return 0;
}
