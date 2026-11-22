#include "uk45_amplifier.h"
#include <math.h>
#include <string.h>
typedef struct uk45_coefficients
{
    float b[4], a[4];
    size_t order;
} uk45_coefficients_t;
static const uint16_t volume_bass_curve[100] = {
    0,    0,    0,    0,    0,    0,    1,    2,    3,    5,    7,    11,
    13,   18,   22,   29,   34,   43,   50,   61,   74,   83,   99,   111,
    129,  143,  165,  181,  207,  226,  256,  288,  311,  348,  374,  416,
    445,  492,  525,  577,  632,  671,  732,  774,  841,  888,  961,  1012,
    1092, 1148, 1235, 1326, 1389, 1487, 1555, 1661, 1735, 1849, 1927, 2050,
    2177, 2264, 2400, 2494, 2639, 2738, 2892, 2998, 3162, 3274, 3447, 3626,
    3749, 3939, 4069, 4269, 4406, 4617, 4761, 4983, 5212, 5368, 5609, 5773,
    6025, 6197, 6461, 6642, 6918, 7107, 7397, 7694, 7896, 8207, 8418, 8742,
    8962, 9299, 9529, 9881};
static const uint16_t presence_curve[100] = {
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
static float curve(const uint16_t *t, uint8_t v)
{
    return (float)t[v] * .0001f;
}
static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
static float filter(gp100_uk45_filter_t *s, uk45_coefficients_t c, float x)
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
static uk45_coefficients_t tone(float b, float m, float t)
{
    float m2 = m * m, bm = b * m, bt = b * t, mt = m * t;
    return (uk45_coefficients_t){
        {b * .01638156f + m * .01585626f + t * .02613953f + bm * .6224268f +
             bt - mt * .025f - m2 * .01556067f + .000409539f,
         mt * .075f - m * .04309267f - t * .07612805f - bm * 1.715284f -
             bt * 3.f - b * .01293493f + m2 * .0428821f - .0003233732f,
         m * .0387866f - b * .01638156f + t * .07386047f + bm * 1.563288f +
             bt * 3.f - mt * .075f - m2 * .03908219f - .000409539f,
         b * .01293493f - m * .01155019f - t * .02387195f - bm * .4704304f -
             bt + mt * .025f + m2 * .01176076f + .0003233732f},
        {b * 1.18435f - m * .01334295f + bm * .6224268f - m2 * .01556067f +
             .03093968f,
         m * .03610654f - b * 3.180903f - bm * 1.715284f + m2 * .0428821f -
             .0804573f,
         b * 2.81565f - m * .03201419f + bm * 1.563288f - m2 * .03908219f +
             .06906418f,
         m * .009250599f - b * .8190966f - bm * .4704304f + m2 * .01176076f -
             .01953884f},
        3};
}
static float shape(float x, float drive, float limit)
{
    float p, m = fabsf(x);
    unsigned i;
    if (m >= limit)
        return copysignf(limit, x);
    p = 1.f - m * drive / 1024.f;
    for (i = 0; i < 10; i++)
        p *= p;
    return copysignf((1.f - p) * limit, x);
}
static float stage(gp100_uk45_amplifier_t *a, size_t k, float x,
                   float drive, float limit)
{
    float p[4], d = (x - a->previous_nonlinear_input[k]) / 3.f, y = 0;
    size_t i;
    p[0] = a->previous_nonlinear_input[k];
    p[1] = p[0] + d;
    p[2] = p[0] + 2 * d;
    p[3] = x;
    memmove(&a->fir_history[k][4], a->fir_history[k], 36 * sizeof(float));
    for (i = 0; i < 4; i++)
        a->fir_history[k][i] = shape(p[3 - i], drive, limit);
    for (i = 0; i < 40; i++)
        y += a->fir_history[k][i] * fir[i];
    a->previous_nonlinear_input[k] = x;
    return y;
}
bool gp100_uk45_amplifier_reset(gp100_uk45_amplifier_t *a, uint8_t v,
                                     uint8_t p, uint8_t o, uint8_t b, uint8_t m,
                                     uint8_t t)
{
    if (!a || v > 99 || p > 99 || o > 99 || b > 99 || m > 99 || t > 99)
        return false;
    memset(a, 0, sizeof(*a));
    a->volume = curve(volume_bass_curve, v);
    a->presence = curve(presence_curve, p);
    a->output = a->output_target = o * .01f;
    a->bass = curve(volume_bass_curve, b);
    a->middle = m * .01f;
    a->treble = a->treble_target = t * .01f;
    a->configured = true;
    return true;
}
bool gp100_uk45_amplifier_process(gp100_uk45_amplifier_t *a,
                                       uint8_t v, uint8_t p, uint8_t o,
                                       uint8_t b, uint8_t m, uint8_t t,
                                       float *audio, size_t frames,
                                       bool enabled)
{
    static const uk45_coefficients_t in[3] = {
        {{8.77729034f, -12.0002813f, 4.11548996f, 0},
         {1, -1.67277324f, .672938585f, 0},
         2},
        {{6.99821186f, 9.25474644f, 2.73505974f, 0},
         {1, -.904553115f, .152290598f, 0},
         2},
        {{11.6897001f, -12.0002813f, .312188506f, 0},
         {1, .878455997f, .0216278005f, 0},
         2}};
    size_t n, i;
    if (!a || !a->configured || v > 99 || p > 99 || o > 99 || b > 99 ||
        m > 99 || t > 99 || (frames && !audio) || frames > SIZE_MAX / 2)
        return false;
    a->volume = curve(volume_bass_curve, v);
    a->presence = curve(presence_curve, p);
    a->output_target = o * .01f;
    a->bass = curve(volume_bass_curve, b);
    a->middle = m * .01f;
    a->treble_target = t * .01f;
    for (n = 0; n < frames; n++)
    {
        float x, angle, k, c, q, scale;
        a->output = glide(a->output, a->output_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        x = audio[n * 2];
        for (i = 0; i < 3; i++)
            x = filter(&a->input_filter[i], in[i], x);
        x = filter(&a->volume_filter,
                   (uk45_coefficients_t){
                       {a->volume * .9994334f, a->volume * -.9994334f, 0, 0},
                       {1, -.9988669f, 0, 0},
                       1},
                   x);
        angle = .213713795f;
        k = sinf(angle);
        c = cosf(angle);
        x = filter(
            &a->notch_filter,
            (uk45_coefficients_t){{(1 - c) * .5f, 1 - c, (1 - c) * .5f, 0},
                                  {k / 1.414f + 1, -2 * c, 1 - k / 1.414f, 0},
                                  2},
            x);
        x = stage(a, 0, x * .13f, .2f, 40.f) * .1f;
        x = filter(&a->tone_filter, tone(a->bass, a->middle, a->treble), x);
        x = stage(a, 1, x, 7.f, 1.5f);
        angle = 2.66116405f;
        scale = powf(10.f, a->presence * 22.7f / 40.f);
        q = sinf(angle) / .3f;
        x = filter(&a->presence_filter,
                   (uk45_coefficients_t){
                       {q * scale + 1, -2 * cosf(angle), 1 - q * scale, 0},
                       {q / scale + 1, -2 * cosf(angle), 1 - q / scale, 0},
                       2},
                   x);
        x *= a->output * .6989675f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
