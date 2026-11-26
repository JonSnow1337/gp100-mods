#include "audio/effects/amp/uk45_amplifier.h"
#include <math.h>

int main(void)
{
    gp100_uk45_amplifier_t amplifier;
    float audio[2] = {.00001f, -.5f};
    float initial;
    size_t i;
    if (!gp100_uk45_amplifier_reset(
            &amplifier, 50, 50, 50, 50, 50, 50)) return 1;
    initial = amplifier.output;
    if (!gp100_uk45_amplifier_process(
            &amplifier, 99, 99, 99, 99, 99, 99, audio, 1, false) ||
        audio[0] != .00001f || audio[1] != -.5f ||
        amplifier.output <= initial) return 2;
    for (i = 0; i < 4096; ++i) {
        audio[0] = sinf(6.2831855f * (float)i / 47.f) * .2f;
        if (!gp100_uk45_amplifier_process(
                &amplifier, 70, 60, 65, 55, 60, 65,
                audio, 1, true) || !isfinite(audio[0]) ||
            audio[0] != audio[1]) return 3;
    }
    if (gp100_uk45_amplifier_reset(NULL, 0, 0, 0, 0, 0, 0) ||
        gp100_uk45_amplifier_reset(
            &amplifier, 100, 0, 0, 0, 0, 0) ||
        gp100_uk45_amplifier_process(
            &amplifier, 0, 0, 0, 0, 0, 100, NULL, 0, true)) return 4;
    return 0;
}
