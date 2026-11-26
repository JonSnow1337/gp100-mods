#include "audio/effects/amp/foxy30tb_amplifier.h"

#include <math.h>

int main(void)
{
    gp100_foxy30tb_amplifier_t amplifier;
    float audio[2] = {.000001f,-.25f};
    float initial_cut;
    size_t i;
    if (!gp100_foxy30tb_amplifier_reset(
            &amplifier,50,50,50,50,50,false)) return 1;
    initial_cut = amplifier.cut;
    if (!gp100_foxy30tb_amplifier_process(
            &amplifier,99,99,99,99,99,true,audio,1u,false) ||
        audio[0] != .000001f || audio[1] != -.25f ||
        amplifier.cut <= initial_cut || !amplifier.character) return 2;
    for (i = 0u; i < 4096u; ++i) {
        audio[0] = sinf(6.2831855f*(float)i/73.0f)*.000001f;
        if (!gp100_foxy30tb_amplifier_process(
                &amplifier,65,55,60,50,60,(i&511u)!=0u,
                audio,1u,true) || !isfinite(audio[0]) ||
            audio[0] != audio[1]) return 3;
    }
    if (gp100_foxy30tb_amplifier_reset(
            NULL,0,0,0,0,0,false) ||
        gp100_foxy30tb_amplifier_reset(
            &amplifier,100,0,0,0,0,false) ||
        gp100_foxy30tb_amplifier_process(
            &amplifier,0,0,0,0,100,false,NULL,0u,true)) return 4;
    return 0;
}
