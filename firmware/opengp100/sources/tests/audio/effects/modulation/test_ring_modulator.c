#include "audio/effects/modulation/ring_modulator.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float a, float b, float tolerance)
{
    return fabsf(a - b) <= tolerance;
}

int main(void)
{
    gp100_ring_modulator_t modulator;
    float audio[2] = {1.0f, -1.0f};
    float saved_phase;

    if (!gp100_ring_modulator_reset(&modulator, 50u, 50u, 0, 50u) ||
        !close_enough(modulator.mix, 0.4980f, 0.000001f) ||
        !close_enough(modulator.normalized_frequency * 44100.0f,
                      346.9f, 0.2f) ||
        !gp100_ring_modulator_process(
            &modulator, 99u, 50u, 0, 50u, audio, 1u, true) ||
        !close_enough(audio[0], 0.774958432f, 0.000002f) ||
        audio[0] != audio[1] ||
        modulator.oscillator_samples != 1.0f ||
        !close_enough(modulator.mix, 0.4981f, 0.000001f)) {
        fprintf(stderr, "Ring Mod signal/control test failed: %.9g\n",
                (double)audio[0]);
        return 1;
    }
    saved_phase = modulator.oscillator_samples;
    audio[0] = 0.25f;
    audio[1] = -0.5f;
    if (!gp100_ring_modulator_process(
            &modulator, 0u, 99u, 50, 99u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        modulator.oscillator_samples != saved_phase ||
        gp100_ring_modulator_reset(NULL, 0u, 0u, 0, 0u) ||
        gp100_ring_modulator_reset(&modulator, 100u, 0u, 0, 0u) ||
        gp100_ring_modulator_process(
            &modulator, 0u, 0u, -51, 0u, audio, 1u, true) ||
        gp100_ring_modulator_process(
            &modulator, 0u, 0u, 0, 0u, NULL, 1u, true)) {
        fprintf(stderr, "Ring Mod bypass/validation test failed\n");
        return 2;
    }
    return 0;
}
