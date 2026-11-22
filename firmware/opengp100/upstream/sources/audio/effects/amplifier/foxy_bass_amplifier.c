#include "foxy_bass_amplifier.h"
#include <math.h>
#include <string.h>
typedef struct coefficients
{
    float b[4], a[4];
    size_t order;
} coefficients_t;
static const uint16_t linear[100] = {
    0,    0,    119,  199,  318,  398,  517,  597,  717,  796,  916,  1035,
    1115, 1235, 1314, 1434, 1513, 1633, 1713, 1832, 1952, 2031, 2151, 2231,
    2350, 2430, 2549, 2629, 2749, 2828, 2948, 3067, 3147, 3266, 3346, 3466,
    3545, 3665, 3745, 3864, 3984, 4063, 4183, 4262, 4382, 4462, 4581, 4661,
    4780, 4860, 4980, 5099, 5179, 5298, 5378, 5498, 5577, 5697, 5776, 5896,
    6015, 6095, 6215, 6294, 6414, 6494, 6613, 6693, 6812, 6892, 7011, 7131,
    7211, 7330, 7410, 7529, 7609, 7729, 7808, 7928, 8047, 8127, 8247, 8326,
    8446, 8525, 8645, 8725, 8844, 8924, 9043, 9163, 9243, 9362, 9442, 9561,
    9641, 9760, 9840, 9960};
static const uint16_t bass_curve[100] = {
    100,  10,   21,   28,   39,   47,   58,   66,   77,   84,   96,   107,
    114,  126,  133,  144,  152,  163,  171,  182,  193,  201,  212,  219,
    231,  238,  250,  257,  269,  276,  288,  300,  308,  319,  328,  340,
    348,  360,  369,  382,  395,  404,  418,  428,  443,  453,  469,  480,
    498,  510,  529,  549,  564,  586,  602,  628,  646,  674,  695,  728,
    763,  788,  829,  858,  905,  938,  992,  1031, 1093, 1137, 1209, 1287,
    1343, 1433, 1497, 1601, 1675, 1794, 1879, 2016, 2165, 2271, 2441, 2562,
    2757, 2896, 3117, 3275, 3528, 3707, 3994, 4303, 4523, 4873, 5121, 5517,
    5797, 6242, 6558, 7059};
static float cv(const uint16_t *t, uint8_t v)
{
    return (float)t[v] / 10000.f;
}
static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
static float filter(gp100_foxy_bass_filter_t *s, coefficients_t c, float x)
{
    float y = c.b[0] * x;
    size_t i;
    for (i = 0; i < c.order; i++)
        y += c.b[i + 1] * s->x[i];
    for (i = 0; i < c.order; i++)
        y -= c.a[i + 1] * s->y[i];
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
static coefficients_t tone(float b, float t)
{
    float b2 = b * b, bt = b * t, b2t = b2 * t;
    return (coefficients_t){
        {b2t * .9911131f - t * .00103443f - bt - b * .1387753f +
             b2 * .1388379f - .0001355619f,
         b * .3545216f + t * .001029112f + bt * 3.002054f - b2t * 2.973339f -
             b2 * .3545611f + .00008918025f,
         t * .00103443f - b * .2973145f - bt * 3.004097f + b2t * 2.973339f +
             b2 * .2972518f + .0001355619f,
         b * .08156816f - t * .001029112f + bt * 1.002043f - b2t * .9911131f -
             b2 * .08152867f - .00008918025f},
        {b2 * 1.354693f - b * 1.365533f - .001404556f,
         -b2 * 3.552642f + b * 3.583798f + .0008825223f,
         b2 * 3.045849f - b * 3.074651f + .001402145f,
         -b2 * .8478995f + b * .8563906f - .0008849338f},
        3};
}
static bool valid(uint8_t v, uint8_t b, uint8_t t)
{
    return v <= 99 && b <= 99 && t <= 99;
}
bool gp100_foxy_bass_amplifier_reset(gp100_foxy_bass_amplifier_t *a,
                                          uint8_t v, uint8_t b, uint8_t t)
{
    if (!a || !valid(v, b, t))
        return false;
    memset(a, 0, sizeof(*a));
    a->volume = a->volume_target = cv(linear, v);
    a->bass = a->bass_target = cv(bass_curve, b) * .9f;
    a->treble = a->treble_target = cv(linear, t);
    a->configured = true;
    return true;
}
bool gp100_foxy_bass_amplifier_process(gp100_foxy_bass_amplifier_t *a,
                                            uint8_t v, uint8_t b, uint8_t t,
                                            float *x, size_t frames,
                                            bool enabled)
{
    static const coefficients_t in[2] = {
        {{.731082499f, -1.334223151f, .6036581993f, 0},
         {1, -1.830879211f, .831715405f, 0},
         2},
        {{1.334223151f, .2823829949f, .03215700015f, 0},
         {1, .2000547051f, .03097309917f, 0},
         2}};
    size_t n;
    if (!a || !a->configured || !valid(v, b, t) || (frames && !x) ||
        frames > SIZE_MAX / 2)
        return false;
    a->volume_target = cv(linear, v);
    a->bass_target = cv(bass_curve, b) * .9f;
    a->treble_target = cv(linear, t);
    for (n = 0; n < frames; n++)
    {
        float y;
        a->volume = glide(a->volume, a->volume_target);
        a->bass = glide(a->bass, a->bass_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        y = filter(&a->input[0], in[0], x[n * 2]);
        y = filter(&a->input[1], in[1], y);
        y = filter(&a->tone, tone(a->bass, a->treble), y);
        y *= a->volume * 50.f * .1549f * .9f * .35f;
        x[n * 2] = y;
        x[n * 2 + 1] = y;
    }
    return true;
}
