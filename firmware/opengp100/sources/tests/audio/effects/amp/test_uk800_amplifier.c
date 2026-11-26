#include "audio/effects/amp/uk800_amplifier.h"

#include <math.h>

int main(void)
{
    gp100_uk800_amplifier_t amplifier;
    float audio[2] = {.000001f,-.5f};
    float initial_gain;
    size_t i;
    if (!gp100_uk800_amplifier_reset(
            &amplifier,50,50,50,50,50,50)) return 1;
    initial_gain = amplifier.gain;
    if (!gp100_uk800_amplifier_process(
            &amplifier,99,99,99,99,99,99,audio,1,false) ||
        audio[0] != .000001f || audio[1] != -.5f ||
        amplifier.gain <= initial_gain) return 2;
    for (i = 0; i < 4096; ++i) {
        audio[0] = sinf(6.2831855f*(float)i/59.f)*.0000001f;
        if (!gp100_uk800_amplifier_process(
                &amplifier,65,55,60,50,55,60,audio,1,true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) return 3;
    }
    if (gp100_uk800_amplifier_reset(NULL,0,0,0,0,0,0) ||
        gp100_uk800_amplifier_reset(&amplifier,100,0,0,0,0,0) ||
        gp100_uk800_amplifier_process(
            &amplifier,0,0,0,0,0,100,NULL,0,true)) return 4;

    if (!gp100_uk800_amplifier_reset(&amplifier, 50, 98, 50, 50, 50, 50)) {
        return 5;
    }
    amplifier.presence_filter.y[0] = 1.0f;
    audio[0] = 0.0f;
    if (!gp100_uk800_amplifier_process(
            &amplifier, 50, 98, 50, 50, 50, 50, audio, 1, true) ||
        fabsf(amplifier.presence_filter.y[0] - 0.12151847f) > 1.0e-6f) {
        return 6;
    }
    return 0;
}
