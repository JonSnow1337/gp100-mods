#include "mess2c1_amplifier.h"
#include <math.h>
#include <string.h>

typedef struct coefficients
{
    float b[4], a[4];
    size_t order;
} coefficients_t;
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
static const uint16_t treble_curve[100] = {
    0,    0,    0,    0,    0,    0,    1,    2,    3,    5,    7,    11,
    13,   18,   22,   29,   34,   43,   50,   61,   74,   83,   99,   111,
    129,  143,  165,  181,  207,  226,  256,  288,  311,  348,  374,  416,
    445,  492,  525,  577,  632,  671,  732,  774,  841,  888,  961,  1012,
    1092, 1148, 1235, 1326, 1389, 1487, 1555, 1661, 1735, 1849, 1927, 2050,
    2177, 2264, 2400, 2494, 2639, 2738, 2892, 2998, 3162, 3274, 3447, 3626,
    3749, 3939, 4069, 4269, 4406, 4617, 4761, 4983, 5212, 5368, 5609, 5773,
    6025, 6197, 6461, 6642, 6918, 7107, 7397, 7694, 7896, 8207, 8418, 8742,
    8962, 9299, 9529, 9881};
static const uint16_t bass_curve[100] = {
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,
    1,    2,    3,    4,    5,    7,    8,    11,   14,   17,   21,   24,
    30,   34,   42,   47,   57,   64,   75,   88,   98,   113,  125,  144,
    158,  180,  196,  223,  251,  272,  306,  330,  368,  396,  440,  472,
    522,  558,  615,  676,  719,  788,  836,  913,  967,  1053, 1113, 1208,
    1309, 1380, 1492, 1570, 1692, 1778, 1913, 2007, 2154, 2256, 2417, 2586,
    2704, 2887, 3015, 3214, 3353, 3568, 3718, 3951, 4194, 4363, 4625, 4807,
    5089, 5284, 5586, 5795, 6119, 6343, 6689, 7050, 7298, 7683, 7948, 8358,
    8641, 9077, 9377, 9841};
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
static float filter(gp100_mess2c1_filter_t *s, coefficients_t c, float x)
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
static coefficients_t gain_voicing(float g)
{
    float q = g * g;
    return (coefficients_t){{q - g * 1.094482f, g * .9055178f - q, 0, 0},
                            {(q - g) - .09448224f, (g - q) - .09448224f, 0, 0},
                            1};
}
static coefficients_t tone(float b, float m, float t)
{
    float t2 = t * t;
    return (coefficients_t){
        {b * .004269454f + m * .0002243567f + t * .001419534f +
             b * .1581859f * m + b * t,
         b * -.00339205f - m * .0001728065f - t * .001417346f -
             b * .4381859f * m - b * 3 * t,
         b * .4018141f * m - m * .0002243567f - t * .001419534f -
             b * .004269454f + b * 3 * t,
         b * .00339205f + m * .0001728065f + t * .001417346f -
             b * .1218141f * m - b * t},
        {b * 1.186095f + m * .0002411271f + t * .0004192614f +
             b * .1700134f * m + b * .2957091f * t + m * .00002769849f * t -
             b * .2506031f * t2 - m * .00003546099f * t2 - t2 * .0003548834f +
             b * .01952381f * m * t - b * .025f * m * t2 + .001677046f,
         b * .7506031f * t2 - m * .0001844008f - t * .0002898592f -
             b * .4700093f * m - b * .7954908f * t - m * .0000148547f * t -
             b * 3.185088f + m * .00003546099f * t2 + t2 * .0003543364f -
             b * .04952381f * m * t + b * .075f * m * t2 - .001159437f,
         b * 2.813906f - m * .0002411192f - t * .0004190629f +
             b * .4299866f * m + b * .7042909f * t - m * .00002769849f * t -
             b * .7493969f * t2 + m * .00003546099f * t2 + t2 * .0003548834f +
             b * .04047619f * m * t - b * .075f * m * t2 - .001676252f,
         m * .0001844088f - b * .814912f + t * .0002900577f -
             b * .1299907f * m - b * .2045092f * t + m * .0000148547f * t +
             b * .2493969f * t2 - m * .00003546099f * t2 - t2 * .0003543364f -
             b * .01047619f * m * t + b * .025f * m * t2 + .001160231f},
        3};
}
static coefficients_t presence(float p)
{
    float hi, lo, k, hf, hr, lf, lr, p0, p1, q0, q1, r0, r1, d;
    if (p == 0)
        p = .01f;
    hi = powf(10, p * 10.f / 20.f);
    lo = powf(10, p / 20.f);
    k = tanf(.1068568975f);
    hf = 2 / (hi + 1);
    hr = 2 / (1 / hi + 1);
    lf = 2 / (lo + 1);
    lr = 2 / (1 / lo + 1);
    p0 = k + hf;
    p1 = k - hf;
    q0 = k * lr + 1;
    q1 = k * lr - 1;
    r0 = k * lf + 1;
    r1 = k * lf - 1;
    d = p0 * r0;
    return (coefficients_t){{(k + hr) * q1 / d,
                             ((k + hr) * q0 + (k - hr) * q1) / d,
                             (k - hr) * q0 / d, 0},
                            {1, (p0 * r1 + p1 * r0) / d, p1 * r1 / d, 0},
                            2};
}
static float shape(float x)
{
    float m = fabsf(x), p, drive = x < 0 ? 4.f : 5.f;
    unsigned i;
    if (m >= 1.3f)
        return copysignf(1.3f, x);
    p = 1 - m * drive / 1024.f;
    for (i = 0; i < 10; i++)
        p *= p;
    return copysignf((1 - p) * 1.3f, x);
}
static float stage(gp100_mess2c1_amplifier_t *a, float x)
{
    float p[4], d = (x - a->previous_nonlinear_input) / 3.f, y = 0;
    size_t i;
    p[0] = a->previous_nonlinear_input;
    p[1] = p[0] + d;
    p[2] = p[0] + 2 * d;
    p[3] = x;
    memmove(&a->fir_history[4], a->fir_history, 36 * sizeof(float));
    for (i = 0; i < 4; i++)
        a->fir_history[i] = shape(p[3 - i]);
    for (i = 0; i < 40; i++)
        y += a->fir_history[i] * fir[i];
    a->previous_nonlinear_input = x;
    return y;
}
static bool valid(uint8_t g, uint8_t p, uint8_t o, uint8_t b, uint8_t m,
                  uint8_t t)
{
    return g <= 99 && p <= 99 && o <= 99 && b <= 99 && m <= 99 && t <= 99;
}
static void targets(gp100_mess2c1_amplifier_t *a, uint8_t g, uint8_t p,
                    uint8_t o, uint8_t b, uint8_t m, uint8_t t)
{
    a->gain_target = curve(gain_curve, g);
    a->presence = p * .01f;
    a->master_target = o * .01f;
    a->bass_target = curve(bass_curve, b);
    a->middle_target = m * .01f;
    a->treble_target = curve(treble_curve, t) * .99f + .01f;
}
bool gp100_mess2c1_amplifier_reset(gp100_mess2c1_amplifier_t *a,
                                        uint8_t g, uint8_t p, uint8_t o,
                                        uint8_t b, uint8_t m, uint8_t t)
{
    if (!a || !valid(g, p, o, b, m, t))
        return false;
    memset(a, 0, sizeof(*a));
    targets(a, g, p, o, b, m, t);
    a->gain = a->gain_target;
    a->master = a->master_target;
    a->bass = a->bass_target;
    a->middle = a->middle_target;
    a->treble = a->treble_target;
    a->configured = true;
    return true;
}
bool gp100_mess2c1_amplifier_process(gp100_mess2c1_amplifier_t *a,
                                          uint8_t g, uint8_t p, uint8_t o,
                                          uint8_t b, uint8_t m, uint8_t t,
                                          float *audio, size_t frames,
                                          bool enabled)
{
    static const coefficients_t in = {
        {2465.733154f, -4912.225098f, 2446.551758f, 0},
        {1, -1.956368804f, .9569137096f, 0},
        2};
    static const coefficients_t aa = {
        {.02016239081f, .04032478163f, .02016239081f, 0},
        {1.198805618f, -1.919350437f, .8011943819f, 0},
        2};
    size_t n;
    if (!a || !a->configured || !valid(g, p, o, b, m, t) ||
        (frames && !audio) || frames > SIZE_MAX / 2)
        return false;
    targets(a, g, p, o, b, m, t);
    for (n = 0; n < frames; n++)
    {
        float x;
        a->gain = glide(a->gain, a->gain_target);
        a->master = glide(a->master, a->master_target);
        a->bass = glide(a->bass, a->bass_target);
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        x = filter(&a->input_filter, in, audio[n * 2]);
        x = filter(&a->anti_alias_filter, aa, x);
        x = filter(&a->tone_filter, tone(a->bass, a->middle, a->treble), x);
        x = filter(&a->gain_filter, gain_voicing(a->gain), x);
        x = stage(a, x) * a->master;
        x = filter(&a->presence_filter, presence(a->presence), x);
        x *= .3f * .1148f * 9.f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
