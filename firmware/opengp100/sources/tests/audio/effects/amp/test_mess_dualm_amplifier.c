#include "audio/effects/amp/mess_dualm_amplifier.h"
#include <math.h>

int main(void)
{
    gp100_mess_dualm_amplifier_t amplifier;
    float audio[2] = {.00001f, -.5f};
    float initial, bass, presence;
    size_t i;
    if (!gp100_mess_dualm_amplifier_reset(
            &amplifier, 50, 50, 50, 50, 50, 50)) return 1;
    initial = amplifier.gain;
    bass = amplifier.bass;
    presence = amplifier.presence;
    if (!gp100_mess_dualm_amplifier_process(
            &amplifier, 99, 99, 99, 99, 99, 99, audio, 1, false) ||
        audio[0] != .00001f || audio[1] != -.5f ||
        amplifier.gain <= initial || amplifier.bass != bass ||
        amplifier.presence != presence) return 2;
    for (i = 0; i < 4096; ++i) {
        audio[0] = sinf(6.2831855f * (float)i / 47.f) * .000001f;
        if (!gp100_mess_dualm_amplifier_process(
                &amplifier, 70, 60, 65, 55, 60, 65,
                audio, 1, true) || !isfinite(audio[0]) ||
            audio[0] != audio[1]) return 3;
    }
    if (gp100_mess_dualm_amplifier_reset(NULL, 0, 0, 0, 0, 0, 0) ||
        gp100_mess_dualm_amplifier_reset(
            &amplifier, 100, 0, 0, 0, 0, 0) ||
        gp100_mess_dualm_amplifier_process(
            &amplifier, 0, 0, 0, 0, 0, 100, NULL, 0, true)) return 4;
    return 0;
}
