#include "audio/effects/amp/bellman59b_amplifier.h"

#include <math.h>

int main(void)
{
    gp100_bellman59b_amplifier_t amplifier;
    float audio[2] = {.000001f, -.5f};
    float initial_volume;
    size_t i;
    if (!gp100_bellman59b_amplifier_reset(
            &amplifier, 50, 50, 50, 50, 50, 50)) return 1;
    initial_volume = amplifier.volume;
    if (!gp100_bellman59b_amplifier_process(
            &amplifier, 99, 99, 99, 99, 99, 99, audio, 1u, false) ||
        audio[0] != .000001f || audio[1] != -.5f ||
        amplifier.volume <= initial_volume) return 2;
    for (i = 0u; i < 4096u; ++i) {
        audio[0] = sinf(6.2831855f * (float)i / 67.0f) * .000001f;
        if (!gp100_bellman59b_amplifier_process(
                &amplifier, 65, 55, 60, 50, 55, 60,
                audio, 1u, true) || !isfinite(audio[0]) ||
            audio[0] != audio[1]) return 3;
    }
    if (gp100_bellman59b_amplifier_reset(
            NULL, 0, 0, 0, 0, 0, 0) ||
        gp100_bellman59b_amplifier_reset(
            &amplifier, 100, 0, 0, 0, 0, 0) ||
        gp100_bellman59b_amplifier_process(
            &amplifier, 0, 0, 0, 0, 0, 100, NULL, 0u, true)) return 4;
    return 0;
}
