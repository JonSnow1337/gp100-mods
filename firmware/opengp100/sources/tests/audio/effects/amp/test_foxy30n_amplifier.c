#include "audio/effects/amp/foxy30n_amplifier.h"

#include <math.h>

int main(void)
{
    gp100_foxy30n_amplifier_t amplifier;
    float audio[2]={.25f,-.5f};
    float initial_volume;
    size_t i;
    if (!gp100_foxy30n_amplifier_reset(
            &amplifier,50u,50u,50u,false)) return 1;
    initial_volume=amplifier.volume;
    if (!gp100_foxy30n_amplifier_process(
            &amplifier,99u,99u,99u,true,audio,1u,false) ||
        audio[0]!=.25f || audio[1]!=-.5f ||
        amplifier.volume<=initial_volume) return 2;
    for (i=0u;i<4096u;++i) {
        audio[0]=sinf(6.2831855f*(float)i/47.f)*.01f;
        if (!gp100_foxy30n_amplifier_process(
                &amplifier,70u,60u,65u,true,audio,1u,true) ||
            !isfinite(audio[0]) || audio[0]!=audio[1]) return 3;
    }
    if (gp100_foxy30n_amplifier_reset(NULL,0u,0u,0u,false) ||
        gp100_foxy30n_amplifier_reset(
            &amplifier,100u,0u,0u,false) ||
        gp100_foxy30n_amplifier_process(
            &amplifier,0u,100u,0u,false,NULL,0u,true)) return 4;
    return 0;
}
