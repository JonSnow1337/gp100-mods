#include "bad_kt_cl_amplifier.h"
#include <math.h>
#include <string.h>

typedef struct bad_kt_coefficients
{
    float b0, b1, b2, a0, a1, a2;
} bad_kt_coefficients_t;
static const uint16_t gain_curve[100] = {
    0,    0,    1,    4,    10,   15,   26,   35,   51,   63,   84,   107,
    124,  152,  172,  205,  229,  266,  293,  335,  381,  412,  462,  497,
    552,  590,  650,  691,  755,  800,  869,  941,  990,  1067, 1120, 1201,
    1257, 1343, 1402, 1493, 1587, 1651, 1750, 1817, 1920, 1991, 2099, 2172,
    2285, 2362, 2480, 2600, 2682, 2807, 2892, 3022, 3111, 3245, 3337, 3476,
    3619, 3715, 3862, 3962, 4114, 4217, 4373, 4479, 4641, 4750, 4916, 5085,
    5200, 5373, 5491, 5669, 5790, 5973, 6097, 6285, 6476, 6605, 6801, 6933,
    7133, 7269, 7474, 7612, 7822, 7964, 8179, 8396, 8543, 8765, 8915, 9142,
    9295, 9527, 9683, 9920};
static const float fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
static float filter(gp100_bad_kt_cl_filter_t *s, bad_kt_coefficients_t c,
                    float x)
{
    float y = (x * c.b0 + s->x[0] * c.b1 + s->x[1] * c.b2 - s->y[0] * c.a1 -
               s->y[1] * c.a2) /
              c.a0;
    s->x[1] = s->x[0];
    s->x[0] = x;
    s->y[1] = s->y[0];
    s->y[0] = y;
    return y;
}
static bad_kt_coefficients_t gain_coefficients(float g)
{
    float q = g * g;
    return (bad_kt_coefficients_t){q - g * 1.062988f,
                                   q * -2.f + g * 2.f,
                                   q - g * .9370118f,
                                   q - g * .9924414f - .07102284f,
                                   q * -2.f + g * 2.f - .0009522019f,
                                   q - g * 1.007559f + .07007064f};
}
static float shape(float x)
{
    float m = fabsf(x), p;
    unsigned i;
    if (m >= 2.f)
        return copysignf(2.f, x);
    p = 1.f - m * 4.f / 1024.f;
    for (i = 0; i < 10u; ++i)
        p *= p;
    return copysignf((1.f - p) * 2.f, x);
}
static float power_stage(gp100_bad_kt_cl_amplifier_t *a, float x)
{
    float p[4], d = (x - a->previous_nonlinear_input) / 3.f, y = 0.f;
    size_t i;
    p[0] = a->previous_nonlinear_input;
    p[1] = p[0] + d;
    p[2] = p[0] + 2.f * d;
    p[3] = x;
    memmove(&a->fir_history[4], a->fir_history, 36u * sizeof(float));
    for (i = 0; i < 4u; ++i)
        a->fir_history[i] = shape(p[3u - i]);
    for (i = 0; i < 40u; ++i)
        y += a->fir_history[i] * fir[i];
    a->previous_nonlinear_input = x;
    return y;
}

bool gp100_bad_kt_cl_amplifier_reset(gp100_bad_kt_cl_amplifier_t *a,
                                          uint8_t gain, uint8_t presence,
                                          uint8_t master)
{
    if (a == NULL || gain > 99u || presence > 99u || master > 99u)
        return false;
    memset(a, 0, sizeof(*a));
    a->gain = a->gain_target = (float)gain_curve[gain] * .00005f;
    a->presence = a->presence_target = (float)presence * .01f;
    a->master = a->master_target = (float)master * .01f;
    a->configured = true;
    return true;
}
bool gp100_bad_kt_cl_amplifier_process(gp100_bad_kt_cl_amplifier_t *a,
                                            uint8_t gain, uint8_t presence,
                                            uint8_t master, float *audio,
                                            size_t frames, bool enabled)
{
    static const bad_kt_coefficients_t v[5] = {
        {32.3031158f, -49.711998f, 17.4109821f, 1.f, -1.64801204f, .649378121f},
        {49.711998f, 18.4116688f, .112930797f, 1.f, -.406564802f, .193023905f},
        {.367977113f, -.672738612f, .305536896f, 1.f, -1.68387628f,
         .701041281f},
        {.672738612f, -.259003401f, .399539292f, 1.f, -.204550803f,
         .581599772f},
        {.448970795f, .672738612f, .287007689f, 1.f, 1.18667901f, .523832023f}};
    size_t n, i;
    if (a == NULL || !a->configured || gain > 99u || presence > 99u ||
        master > 99u || (frames && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    a->gain_target = (float)gain_curve[gain] * .00005f;
    a->presence_target = (float)presence * .01f;
    a->master_target = (float)master * .01f;
    for (n = 0; n < frames; ++n)
    {
        float x, k, b0, a1;
        a->gain = glide(a->gain, a->gain_target);
        a->presence = glide(a->presence, a->presence_target);
        a->master = glide(a->master, a->master_target);
        if (!enabled)
            continue;
        x = audio[n * 2u];
        x = filter(&a->voicing[0], v[0], x);
        x = filter(&a->voicing[1], v[1], x);
        x = filter(&a->gain_filter, gain_coefficients(a->gain), x);
        for (i = 2u; i < 5u; ++i)
            x = filter(&a->voicing[i], v[i], x);
        x = power_stage(a, x * .1f);
        k = tanf(((a->presence * 8000.f + 850.f) * 3.1415927f) / 44100.f);
        b0 = k / (k + 1.f);
        a1 = (k - 1.f) / (k + 1.f);
        x = filter(&a->presence_filter,
                   (bad_kt_coefficients_t){b0, b0, 0.f, 1.f, a1, 0.f}, x);
        x *= a->master * .604604f;
        audio[n * 2u] = x;
        audio[n * 2u + 1u] = x;
    }
    return true;
}
