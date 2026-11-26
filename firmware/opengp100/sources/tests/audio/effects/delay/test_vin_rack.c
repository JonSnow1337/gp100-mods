#include "audio/effects/delay/vin_rack.h"

#include <math.h>

static gp100_vin_rack_t state;

int main(void)
{
    float audio[2] = {0.25f, -0.5f};
    float impulse[64u * 2u];
    size_t block;

    if (!gp100_vin_rack_reset(&state, 20u, 50u, 20.0f,
                                    50u, 50u, false, true) ||
        state.mod_direction[0] != 1.0f ||
        state.mod_direction[1] != -1.0f ||
        state.current_delay_samples != 2205.0f ||
        fabsf(state.current_feedback - 0.2f) > 0.000001f ||
        !gp100_vin_rack_process(&state, 99u, 99u, 20.0f,
                                     99u, 99u, false, true, 500u,
                                     audio, 1u, true) ||
        !(state.current_delay_samples < 2205.0f) ||
        !(state.current_mod > 0.98f) ||
        !(state.current_mix > 0.2f) ||
        !(state.current_feedback > 0.2f) ||
        !(state.current_tone > 0.5f)) {
        return 1;
    }

    if (!gp100_vin_rack_reset(&state, 62u, 54u, 29.0f,
                                    60u, 65u, false, false)) {
        return 2;
    }
    for (block = 0u; block < 34u; ++block) {
        size_t sample;

        for (sample = 0u; sample < 64u * 2u; ++sample) {
            impulse[sample] = 0.0f;
        }
        if (block == 0u) {
            impulse[0] = 0.25f;
            impulse[1] = -0.125f;
        }
        if (!gp100_vin_rack_process(&state, 62u, 54u, 29.0f,
                                         60u, 65u, false, false, 500u,
                                         impulse, 64u, true)) {
            return 3;
        }
        for (sample = 0u; sample < 64u * 2u; ++sample) {
            if (!isfinite(impulse[sample])) {
                return 3;
            }
        }
    }
    /* Stereo echoes and sample holds at factory frames 2116 and 2162. */
    if (fabsf(impulse[4u * 2u] - 0.035750918f) > 0.000001f ||
        impulse[4u * 2u] != impulse[5u * 2u] ||
        fabsf(impulse[50u * 2u + 1u] - -0.017973138f) > 0.000001f ||
        impulse[50u * 2u + 1u] != impulse[51u * 2u + 1u]) {
        return 4;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_vin_rack_process(&state, 20u, 30u, 20.0f,
                                     20u, 20u, false, false, 500u,
                                     audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        state.rings_active || state.write_index[0] != 0u ||
        state.mod_write_index[1] != 0u || state.mod_phase[1] != 1.0f ||
        !gp100_vin_rack_process(&state, 20u, 30u, 10.0f,
                                     20u, 20u, true, true, 500u,
                                     audio, 1u, true) ||
        state.target_delay_samples != 5512.5f ||
        state.current_delay_samples != 5512.5f ||
        gp100_vin_rack_process(&state, 20u, 30u, 11.0f,
                                    20u, 20u, true, true, 500u,
                                    audio, 1u, true) ||
        gp100_vin_rack_process(&state, 20u, 30u, 20.0f,
                                    100u, 20u, false, true, 500u,
                                    audio, 1u, true)) {
        return 5;
    }
    return 0;
}
