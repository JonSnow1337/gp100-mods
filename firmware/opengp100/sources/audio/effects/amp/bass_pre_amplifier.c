#include "bass_pre_amplifier.h"
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
    99,   99,   99,   99,   159,  199,  258,  298,  358,  398,  458,  517,
    557,  617,  657,  717,  756,  816,  856,  916,  976,  1016, 1075, 1115,
    1175, 1215, 1275, 1315, 1375, 1415, 1475, 1535, 1575, 1635, 1675, 1736,
    1776, 1837, 1877, 1938, 2000, 2041, 2102, 2144, 2206, 2248, 2312, 2354,
    2418, 2462, 2528, 2594, 2639, 2708, 2754, 2824, 2872, 2946, 2995, 3072,
    3150, 3204, 3286, 3343, 3430, 3490, 3583, 3647, 3746, 3815, 3922, 4034,
    4112, 4234, 4318, 4451, 4543, 4688, 4789, 4948, 5117, 5235, 5420, 5550,
    5756, 5900, 6127, 6287, 6539, 6716, 6996, 7294, 7503, 7834, 8066, 8434,
    8692, 9101, 9388, 9842};
static float cv(const uint16_t *t, uint8_t v)
{
    return (float)t[v] / 10000.0f;
}
static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
static float filter(gp100_bass_pre_filter_t *s, coefficients_t c, float x)
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
static coefficients_t tone(float b, float m, float t)
{
    return (coefficients_t){
        {b * .004269454f + m * .0002243567f + t * .001419534f +
             b * .1581859f * m + b * t,
         -b * .00339205f - m * .0001728065f - t * .001417346f -
             b * .4381859f * m - b * 3.f * t,
         b * .4018141f * m - m * .0002243567f - t * .001419534f -
             b * .004269454f + b * 3.f * t,
         b * .00339205f + m * .0001728065f + t * .001417346f -
             b * .1218141f * m - b * t},
        {b * 1.185675f + m * .0002243567f + b * .1581859f * m + .001677046f,
         -b * 3.184798f - m * .0001728065f - b * .4381859f * m - .001159437f,
         b * 2.814325f - m * .0002243567f + b * .4018141f * m - .001676252f,
         m * .0001728065f - b * .8152021f - b * .1218141f * m + .001160231f},
        3};
}
static bool valid(uint8_t v, uint8_t b, uint8_t m, uint8_t t)
{
    return v <= 99 && b <= 99 && m <= 99 && t <= 99;
}
bool gp100_bass_pre_amplifier_reset(gp100_bass_pre_amplifier_t *a,
                                         uint8_t v, bool bright, uint8_t b,
                                         uint8_t m, uint8_t t)
{
    (void)bright;
    if (!a || !valid(v, b, m, t))
        return false;
    memset(a, 0, sizeof(*a));
    a->volume = a->volume_target = cv(linear, v);
    a->bass = a->bass_target = cv(bass_curve, b);
    a->middle = a->middle_target = cv(linear, m);
    a->treble = a->treble_target = cv(linear, t);
    a->configured = true;
    return true;
}
bool gp100_bass_pre_amplifier_process(gp100_bass_pre_amplifier_t *a,
                                           uint8_t v, bool bright, uint8_t b,
                                           uint8_t m, uint8_t t, float *x,
                                           size_t frames, bool enabled)
{
    static const coefficients_t in[2] = {
        {{5.588955879f, -9.255999565f, 3.667083740f, 0},
         {1, -1.656741977f, .6567491889f, 0},
         2},
        {{9.255999565f, 2.803436518f, .08509229869f, 0},
         {1, .1632519960f, .00093799998f, 0},
         2}};
    static const coefficients_t off[2] = {
        {{3.022164583f, -.01484730002f, .0000027f, 0},
         {1, -.4131614864f, .0000011f, 0},
         2},
        {{5.038599968f, 1.947124958f, .05081399903f, 0},
         {1, .1937119961f, .0000005f, 0},
         2}};
    static const coefficients_t on[2] = {
        {{4.365490437f, -7.311999798f, 2.951437712f, 0},
         {1, -1.660222769f, .6614282727f, 0},
         2},
        {{7.311999798f, .0004087f, .0069188001f, 0},
         {1, -.0000573f, .0010827f, 0},
         2}};
    size_t n;
    if (!a || !a->configured || !valid(v, b, m, t) || (frames && !x) ||
        frames > SIZE_MAX / 2)
        return false;
    a->volume_target = cv(linear, v);
    a->bass_target = cv(bass_curve, b);
    a->middle_target = cv(linear, m);
    a->treble_target = cv(linear, t);
    for (n = 0; n < frames; n++)
    {
        float y;
        coefficients_t tc;
        a->volume = glide(a->volume, a->volume_target);
        a->bass = glide(a->bass, a->bass_target);
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        tc = tone(a->bass, a->middle, a->treble);
        y = x[n * 2];
        y = filter(&a->input[0], in[0], y);
        y = filter(&a->input[1], in[1], y);
        y = filter(&a->tone, tc, y);
        if (bright)
        {
            y = filter(&a->bright_on[0], on[0], y);
            y = filter(&a->bright_on[1], on[1], y);
        }
        else
        {
            y = filter(&a->bright_off[0], off[0], y);
            y = filter(&a->bright_off[1], off[1], y);
        }
        y *= a->volume * .025f * .2239f * 1.5f * .35f;
        x[n * 2] = y;
        x[n * 2 + 1] = y;
    }
    return true;
}
