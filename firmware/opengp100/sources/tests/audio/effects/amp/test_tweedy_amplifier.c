#include "audio/effects/amp/tweedy_amplifier.h"

#include <math.h>

int main(void)
{
    gp100_tweedy_amplifier_t amplifier;
    float audio[2] = {0.25f, -0.5f};
    float initial_gain;
    size_t i;
    if (!gp100_tweedy_amplifier_reset(&amplifier, 50u, 50u, 50u))
        return 1;
    initial_gain = amplifier.gain;
    if (!gp100_tweedy_amplifier_process(
            &amplifier, 99u, 99u, 99u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        amplifier.gain <= initial_gain) return 2;
    for (i = 0u; i < 4096u; ++i) {
        audio[0] = sinf(6.2831855f * (float)i / 47.0f) * 0.01f;
        if (!gp100_tweedy_amplifier_process(
                &amplifier, 70u, 60u, 70u, audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) return 3;
    }
    if (gp100_tweedy_amplifier_reset(NULL, 0u, 0u, 0u) ||
        gp100_tweedy_amplifier_reset(&amplifier, 100u, 0u, 0u) ||
        gp100_tweedy_amplifier_process(
            &amplifier, 0u, 100u, 0u, NULL, 0u, true)) return 4;
    return 0;
}
