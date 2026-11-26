#include "mess_bass_amplifier.h"
#include <math.h>
#include <string.h>
typedef struct coefficients
{
    float b[4], a[4];
    size_t order;
} coefficients_t;
static const uint16_t bass_curve[100] = {
    0,    0,    1,    4,    10,   15,   26,   35,   51,   63,   84,   107,
    124,  152,  172,  205,  229,  266,  293,  335,  381,  412,  462,  497,
    552,  590,  650,  691,  755,  800,  869,  941,  990,  1067, 1120, 1201,
    1257, 1343, 1402, 1493, 1587, 1651, 1750, 1817, 1920, 1991, 2099, 2172,
    2285, 2362, 2480, 2600, 2682, 2807, 2892, 3022, 3111, 3245, 3337, 3476,
    3619, 3715, 3862, 3962, 4114, 4217, 4373, 4479, 4641, 4750, 4916, 5085,
    5200, 5373, 5491, 5669, 5790, 5973, 6097, 6285, 6476, 6605, 6801, 6933,
    7133, 7269, 7474, 7612, 7822, 7964, 8179, 8396, 8543, 8765, 8915, 9142,
    9295, 9527, 9683, 9920};
static const uint16_t volume_curve[100] = {
    0,    0,    0,    0,    1,    3,    6,    8,    13,   17,   25,   34,
    41,   53,   62,   77,   89,   107,  121,  143,  168,  186,  214,  235,
    267,  291,  328,  354,  396,  425,  471,  521,  555,  610,  647,  707,
    748,  813,  858,  928,  1001, 1052, 1131, 1186, 1271, 1330, 1420, 1483,
    1580, 1647, 1750, 1857, 1930, 2043, 2121, 2241, 2323, 2449, 2536, 2669,
    2807, 2901, 3045, 3143, 3295, 3398, 3557, 3665, 3830, 3943, 4117, 4294,
    4415, 4601, 4727, 4920, 5051, 5251, 5388, 5596, 5810, 5955, 6176, 6326,
    6556, 6712, 6949, 7110, 7357, 7523, 7778, 8037, 8213, 8481, 8663, 8940,
    9127, 9413, 9606, 9900};
static const float fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};
static float cv(const uint16_t *t, uint8_t v)
{
    return t[v] * .0001f;
}
static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
static float filter(gp100_mess_bass_filter_t *s, coefficients_t c, float x)
{
    float y = c.b[0] * x;
    size_t i;
    for (i = 0; i < c.order; i++)
        y += c.b[i + 1] * s->x[i] - c.a[i + 1] * s->y[i];
    y /= c.a[0];
    for (i = c.order; i > 1; i--)
    {
        s->x[i - 1] = s->x[i - 2];
        s->y[i - 1] = s->y[i - 2];
    }
    s->x[0] = x;
    s->y[0] = y;
    return y;
}
static coefficients_t tone(float b, float m, float t)
{
    return (coefficients_t){
        {b * .001946392f + m * .0001831003f + t * .001419534f +
             b * .1291185f * m + b * t,
         -b * .001739622f - m * .0001573252f - t * .001417346f -
             b * .3691185f * m - b * 3.f * t,
         b * .3508815f * m - m * .0001831003f - t * .001419534f -
             b * .001946392f + b * 3.f * t,
         b * .001739622f + m * .0001573252f + t * .001417346f -
             b * .1108815f * m - b * t},
        {b * 1.092649f + m * .0001831003f + b * .1291185f * m + .00154829f,
         -b * 3.092443f - m * .0001573252f - b * .3691185f * m - .001288391f,
         b * 2.907351f - m * .0001831003f + b * .3508815f * m - .001547893f,
         m * .0001573252f - b * .9075574f - b * .1108815f * m + .001288788f},
        3};
}
static coefficients_t volume_filter(float v)
{
    return (coefficients_t){{-.47f * v, .47f * v, 0, 0},
                            {v * v - v * 1.000241f - 1.970475f,
                             v * .9997588f - v * v + 1.969525f, 0, 0},
                            1};
}
static float saturate(float x)
{
    return copysignf((1.f - expf(-fabsf(x) * .25f)) * 40.f, x);
}
static float nonlinear(gp100_mess_bass_amplifier_t *a, float x)
{
    float step = (x - a->previous) / 3.f, y = 0;
    size_t i;
    memmove(&a->nonlinear_history[4], a->nonlinear_history, 36 * sizeof(float));
    for (i = 0; i < 4; i++)
        a->nonlinear_history[3 - i] = saturate(a->previous + step * i);
    a->previous = x;
    for (i = 0; i < 40; i++)
        y += a->nonlinear_history[i] * fir[i];
    return y;
}
static bool valid(uint8_t v, uint8_t o, uint8_t b, uint8_t m, uint8_t t)
{
    return v <= 99 && o <= 99 && b <= 99 && m <= 99 && t <= 99;
}
bool gp100_mess_bass_amplifier_reset(gp100_mess_bass_amplifier_t *a,
                                          uint8_t v, uint8_t o, uint8_t b,
                                          uint8_t m, uint8_t t)
{
    (void)b;
    (void)m;
    if (!a || !valid(v, o, b, m, t))
        return false;
    memset(a, 0, sizeof(*a));
    a->volume = a->volume_target = cv(volume_curve, v);
    a->master = a->master_target = o * .01f;
    a->treble = a->treble_target = t * .01f;
    a->configured = true;
    return true;
}
bool gp100_mess_bass_amplifier_process(gp100_mess_bass_amplifier_t *a,
                                            uint8_t v, uint8_t o, uint8_t b,
                                            uint8_t m, uint8_t t, float *x,
                                            size_t frames, bool enabled)
{
    static const coefficients_t input = {{1941.625244f, -3880.860107f,
                                          1939.236206f, 0},
                                         {1, -1.990116596f, .9901446104f, 0},
                                         2},
                                fixed = {{1.006299973f, -2.f, .9937000275f, 0},
                                         {1.004500031f, -2.f, .9955000281f, 0},
                                         2};
    static const coefficients_t post[6] = {
        {{.644333601f, -1.285225749f, .6409416795f, 0},
         {1, -1.997684598f, .9977682829f, 0},
         2},
        {{.7104644179f, -1.285225749f, .5748234987f, 0},
         {1, -1.474851489f, .4787512124f, 0},
         2},
        {{.656866312f, -1.285225749f, .6289175749f, 0},
         {1, -1.764615655f, .7647920847f, 0},
         2},
        {{1.285225749f, -.8134096861f, .09583680332f, 0},
         {1, -1.003889441f, .06300500035f, 0},
         2},
        {{.9784265161f, -1.285225749f, .4932883978f, 0},
         {1, -1.333031535f, .5722206235f, 0},
         2},
        {{1.285225749f, .4723738134f, .002846200019f, 0},
         {1, .06284759939f, .0000429f, 0},
         2}};
    size_t n, i;
    if (!a || !a->configured || !valid(v, o, b, m, t) || (frames && !x) ||
        frames > SIZE_MAX / 2)
        return false;
    a->volume_target = cv(volume_curve, v);
    a->master_target = o * .01f;
    a->treble_target = t * .01f;
    for (n = 0; n < frames; n++)
    {
        float y;
        a->volume = glide(a->volume, a->volume_target);
        a->master = glide(a->master, a->master_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        y = filter(&a->input, input, x[n * 2]);
        y = filter(&a->volume_filter, volume_filter(a->volume), y);
        y = filter(&a->fixed_filter, fixed, y);
        y = nonlinear(a, y * .1f);
        y = filter(&a->tone, tone(cv(bass_curve, b), m * .01f, a->treble), y);
        y *= .4f * a->master;
        for (i = 0; i < 6; i++)
            y = filter(&a->post[i], post[i], y);
        y *= .9941f * 1.5f * 1.1f * .5f * .35f;
        x[n * 2] = y;
        x[n * 2 + 1] = y;
    }
    return true;
}
