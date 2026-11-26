#include "audio/effects/amp/supdual_od_amplifier.h"

#include <math.h>

int main(void)
{
    gp100_supdual_od_amplifier_t amplifier;
    float audio[2]={.000001f,-.5f};
    float initial;
    size_t i;
    if(!gp100_supdual_od_amplifier_reset(&amplifier,50,50,50,50,50))
        return 1;
    initial=amplifier.volume_1;
    if(!gp100_supdual_od_amplifier_process(
            &amplifier,99,99,99,99,99,audio,1u,false) ||
       audio[0]!=.000001f || audio[1]!=-.5f ||
       amplifier.volume_1<=initial) return 2;
    for(i=0u;i<4096u;++i) {
        audio[0]=sinf(6.2831855f*(float)i/79.0f)*.000001f;
        if(!gp100_supdual_od_amplifier_process(
                &amplifier,60,55,65,50,60,audio,1u,true) ||
           !isfinite(audio[0]) || audio[0]!=audio[1]) return 3;
    }
    if(gp100_supdual_od_amplifier_reset(NULL,0,0,0,0,0) ||
       gp100_supdual_od_amplifier_reset(&amplifier,100,0,0,0,0) ||
       gp100_supdual_od_amplifier_process(
           &amplifier,0,0,0,100,0,NULL,0u,true)) return 4;
    return 0;
}
