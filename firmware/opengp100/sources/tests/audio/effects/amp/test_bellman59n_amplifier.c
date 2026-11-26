#include "audio/effects/amp/bellman59n_amplifier.h"
#include <math.h>

int main(void)
{
    gp100_bellman59n_amplifier_t a;
    float audio[2] = {.1f, -.2f};
    size_t i;
    if (!gp100_bellman59n_amplifier_reset(&a,50,50,50,50,50,50) ||
        !gp100_bellman59n_amplifier_process(
            &a,99,99,99,99,99,99,audio,1,false) ||
        audio[0] != .1f || audio[1] != -.2f) return 1;
    for (i=0;i<4096u;++i) {
        audio[0]=sinf(6.2831855f*(float)i/47.f)*.01f;
        if (!gp100_bellman59n_amplifier_process(
                &a,70,60,70,60,55,65,audio,1,true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) return 2;
    }
    if (gp100_bellman59n_amplifier_reset(NULL,0,0,0,0,0,0) ||
        gp100_bellman59n_amplifier_process(
            &a,100,0,0,0,0,0,NULL,0,true)) return 3;
    return 0;
}
