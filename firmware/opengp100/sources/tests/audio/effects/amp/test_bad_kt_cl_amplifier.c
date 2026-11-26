#include "audio/effects/amp/bad_kt_cl_amplifier.h"
#include <math.h>
int main(void)
{
    gp100_bad_kt_cl_amplifier_t a;float audio[2]={.001f,-.5f},initial;size_t i;
    if(!gp100_bad_kt_cl_amplifier_reset(&a,50,50,50)) return 1;
    initial=a.gain;
    if(!gp100_bad_kt_cl_amplifier_process(&a,99,99,99,audio,1,false)||
       audio[0]!=.001f||audio[1]!=-.5f||a.gain<=initial)return 2;
    for(i=0;i<4096u;++i){audio[0]=sinf(6.2831855f*(float)i/47.f)*.00001f;
      if(!gp100_bad_kt_cl_amplifier_process(&a,70,60,65,audio,1,true)||
         !isfinite(audio[0])||audio[0]!=audio[1])return 3;}
    if(gp100_bad_kt_cl_amplifier_reset(NULL,0,0,0)||
       gp100_bad_kt_cl_amplifier_reset(&a,100,0,0)||
       gp100_bad_kt_cl_amplifier_process(&a,0,100,0,NULL,0,true))return 4;
    return 0;
}
