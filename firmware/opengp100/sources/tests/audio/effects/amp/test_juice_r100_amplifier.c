#include "audio/effects/amp/juice_r100_amplifier.h"

#include <math.h>

int main(void)
{
    gp100_juice_r100_amplifier_t amplifier;
    float audio[2] = {.000001f, -.5f};
    float gain;
    float bass;
    size_t i;

    if (!gp100_juice_r100_amplifier_reset(&amplifier,50,50,50,50,50))
        return 1;
    gain = amplifier.gain;
    bass = amplifier.bass;
    if (!gp100_juice_r100_amplifier_process(
            &amplifier,99,99,99,99,99,audio,1,false)
        || audio[0] != .000001f || audio[1] != -.5f
        || amplifier.gain <= gain || amplifier.bass != bass) return 2;
    for (i = 0; i < 4096; ++i) {
        audio[0] = sinf(6.2831855f * (float)i / 83.f) * .001f;
        if (!gp100_juice_r100_amplifier_process(
                &amplifier,65,60,50,55,60,audio,1,true)
            || !isfinite(audio[0]) || audio[0] != audio[1]) return 3;
    }
    if (gp100_juice_r100_amplifier_reset(NULL,0,0,0,0,0)
        || gp100_juice_r100_amplifier_reset(&amplifier,100,0,0,0,0)
        || gp100_juice_r100_amplifier_process(
            &amplifier,0,0,0,0,100,NULL,0,true)) return 4;
    return 0;
}
