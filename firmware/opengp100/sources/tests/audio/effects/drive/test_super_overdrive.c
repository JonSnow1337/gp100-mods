#include "audio/effects/drive/super_overdrive.h"
#include <math.h>
int main(void)
{
    gp100_super_overdrive_t e; float a[2]={.1f,-.1f}; size_t i;
    if(!gp100_super_overdrive_reset(&e,50,50,50)||e.gain<.34f||e.gain>.36f||
       e.tone<.24f||e.tone>.27f||!gp100_super_overdrive_process(&e,99,99,99,a,1,false)||
       a[0]!=.1f||a[1]!=-.1f||fabsf(e.volume-.505f)>1e-6f)return 1;
    for(i=0;i<512;i++){a[0]=sinf(6.2831855f*(float)i/41.0f)*.001f;
      if(!gp100_super_overdrive_process(&e,70,60,70,a,1,true)||
         !isfinite(a[0])||a[0]!=a[1])return 2;}
    return gp100_super_overdrive_reset(NULL,0,0,0)||
           gp100_super_overdrive_reset(&e,100,0,0)||
           gp100_super_overdrive_process(&e,0,0,0,NULL,1,true)?3:0;
}
