#include "super_overdrive.h"

#include <math.h>
#include <string.h>

typedef struct super_coefficients { float b0,b1,b2,b3,a0,a1,a2,a3; }
    super_coefficients_t;

static const float reconstruction_fir[40] = {
    -0.0011f,-0.0007f,0.0001f,0.0015f,0.0032f,0.0048f,0.0053f,0.0034f,
    -0.0016f,-0.0092f,-0.0175f,-0.0233f,-0.0227f,-0.0123f,0.0097f,
    0.0421f,0.0807f,0.1193f,0.1504f,0.1678f,0.1678f,0.1504f,0.1193f,
    0.0807f,0.0421f,0.0097f,-0.0123f,-0.0227f,-0.0233f,-0.0175f,
    -0.0092f,-0.0016f,0.0034f,0.0053f,0.0048f,0.0032f,0.0015f,
    0.0001f,-0.0007f,-0.0011f
};

static const super_coefficients_t input_c = {
    0.947437286f,-1.65387714f,0.706439912f,0.0f,1.0f,
    -1.73134434f,0.73578012f,0.0f
};
static const super_coefficients_t output_c[4] = {
    {0.64239192f,-1.17795849f,0.535801113f,0,1,-1.90031195f,0.901443422f,0},
    {0.755399108f,-1.47890425f,0.724171579f,0,1,-1.91436565f,0.91527319f,0},
    {1.47890425f,-1.36205029f,0.223254502f,0,1,-0.732653916f,0.062495202f,0},
    {1.47890425f,-0.0232218f,0.0002093f,0,1,-0.135606393f,0.0139669f,0}
};

static float curve(uint8_t raw, float power)
{
    unsigned index=(unsigned)((float)raw*2.55f); float x;
    if (index <= 2u) return 0.0f;
    if (index >= 253u) return 1.0f;
    x=(float)(index-2u)/251.0f;
    return floorf(powf(x,power)*10000.0f)*0.0001f;
}

static float biquad(gp100_super_filter_t *s, super_coefficients_t c,
                    float x)
{
    float y=(x*c.b0+s->x1*c.b1+s->x2*c.b2-s->y1*c.a1-s->y2*c.a2)/c.a0;
    s->x2=s->x1; s->x1=x; s->y2=s->y1; s->y1=y; return y;
}

static super_coefficients_t drive_c(float gain)
{
    return (super_coefficients_t){gain*202.3787f+7.678497f,
        gain*-202.3787f-7.580857f,0,0,1,-0.9023599f,0,0};
}

static super_coefficients_t tone_c(float tone)
{
    float q=tone*tone;
    return (super_coefficients_t){
        q*0.05925575f-tone*0.06227284f-0.002636647f,
        q*-0.04718737f+tone*0.04417027f-0.000388622f,
        q*-0.05925575f+tone*0.06227284f+0.00212987f,
        q*0.04718737f-tone*0.04417027f-0.0001181547f,
        q-tone*0.9703721f-0.07412393f,
        q*-2.677823f+tone*2.654229f+0.1007541f,
        q*2.379782f-tone*2.40941f-0.005807722f,
        q*-0.7019593f+tone*0.725553f-0.02183601f};
}

static float third_order(gp100_super_filter_t *s,
                         super_coefficients_t c,float x)
{
    float y=(x*c.b0+s->x1*c.b1+s->x2*c.b2+s->x3*c.b3-
             s->y1*c.a1-s->y2*c.a2-s->y3*c.a3)/c.a0;
    s->x3=s->x2;s->x2=s->x1;s->x1=x;
    s->y3=s->y2;s->y2=s->y1;s->y1=y;return y;
}

static float clip(gp100_super_overdrive_t *e,float input)
{
    float step=(input-e->previous_clip_input)/3.0f,s[4],out=0.0f;
    float pos=1.4f-e->gain*0.3f,neg=0.65f-e->gain*0.15f; size_t i;
    for(i=0;i<4u;++i){float x=e->previous_clip_input+step*(float)i;
        float y=x<=0?-(1-expf(x*2))*neg:(1-expf(-x))*pos;
        s[i]=y*0.995f+x*0.004999995f;}
    memmove(&e->oversample_history[4],e->oversample_history,36*sizeof(float));
    for(i=0;i<4u;++i)e->oversample_history[i]=s[3u-i];
    for(i=0;i<40u;++i)out+=reconstruction_fir[i]*e->oversample_history[i];
    e->previous_clip_input=input;return out;
}

bool gp100_super_overdrive_reset(gp100_super_overdrive_t *e,
                                      uint8_t gain,uint8_t tone,uint8_t volume)
{
    if(!e||gain>99u||tone>99u||volume>99u)return false;
    memset(e,0,sizeof(*e));e->gain=curve(gain,1.5f);e->tone=curve(tone,2.0f);
    e->volume=e->volume_target=(float)volume*0.01f;e->configured=true;return true;
}

bool gp100_super_overdrive_process(gp100_super_overdrive_t *e,
    uint8_t gain,uint8_t tone,uint8_t volume,float *audio,size_t frames,bool on)
{
    size_t n,i;if(!e||!e->configured||gain>99u||tone>99u||volume>99u||
        (frames&&!audio)||frames>SIZE_MAX/2u)return false;
    e->volume_target=(float)volume*0.01f;
    if(e->volume<e->volume_target)e->volume=fminf(e->volume+0.005f,e->volume_target);
    else if(e->volume>e->volume_target)e->volume=fmaxf(e->volume-0.005f,e->volume_target);
    for(n=0;n<frames;++n){float conditioned,out;if(!on)continue;
        e->gain=curve(gain,1.5f);e->tone=curve(tone,2.0f);
        conditioned=biquad(&e->input_filter,input_c,audio[n*2u]);
        out=biquad(&e->gain_filter,drive_c(e->gain),conditioned)*3.0f;
        out=clip(e,out)+conditioned;
        out=third_order(&e->tone_filter,tone_c(e->tone),out)*0.8f*e->volume;
        for(i=0;i<4u;++i)out=biquad(&e->output_filter[i],output_c[i],out);
        audio[n*2u]=out;audio[n*2u+1u]=out;}return true;
}
