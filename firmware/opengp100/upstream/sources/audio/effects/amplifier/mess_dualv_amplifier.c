#include "mess_dualv_amplifier.h"
#include <math.h>
#include <string.h>
typedef struct coefficients
{
    float b[4], a[4];
    size_t order;
} coefficients_t;
static const uint16_t gain_curve[100] = {
    0,    0,    0,    0,    1,    3,    6,    8,    13,   17,   25,   34,
    41,   53,   62,   77,   89,   107,  121,  143,  168,  186,  214,  235,
    267,  291,  328,  354,  396,  425,  471,  521,  555,  610,  647,  707,
    748,  813,  858,  928,  1001, 1052, 1131, 1186, 1271, 1330, 1420, 1483,
    1580, 1647, 1750, 1857, 1930, 2043, 2121, 2241, 2323, 2449, 2536, 2669,
    2807, 2901, 3045, 3143, 3295, 3398, 3557, 3665, 3830, 3943, 4117, 4294,
    4415, 4601, 4727, 4920, 5051, 5251, 5388, 5596, 5810, 5955, 6176, 6326,
    6556, 6712, 6949, 7110, 7357, 7523, 7778, 8037, 8213, 8481, 8663, 8940,
    9127, 9413, 9606, 9900};
static const uint16_t bass_curve[100] = {
    0,    0,    0,    0,    0,    0,    1,    2,    3,    5,    7,    11,
    13,   18,   22,   29,   34,   43,   50,   61,   74,   83,   99,   111,
    129,  143,  165,  181,  207,  226,  256,  288,  311,  348,  374,  416,
    445,  492,  525,  577,  632,  671,  732,  774,  841,  888,  961,  1012,
    1092, 1148, 1235, 1326, 1389, 1487, 1555, 1661, 1735, 1849, 1927, 2050,
    2177, 2264, 2400, 2494, 2639, 2738, 2892, 2998, 3162, 3274, 3447, 3626,
    3749, 3939, 4069, 4269, 4406, 4617, 4761, 4983, 5212, 5368, 5609, 5773,
    6025, 6197, 6461, 6642, 6918, 7107, 7397, 7694, 7896, 8207, 8418, 8742,
    8962, 9299, 9529, 9881};
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
static float filter(gp100_mess_dualv_filter_t *s, coefficients_t c,
                    float x)
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
    float q;
    if (g < .3f)
        g = .3f;
    q = g * g;
    return (coefficients_t){
        {q * .5380433f - g * .5441436f, q * -.526879f + g * .5206522f,
         q * -.5380433f + g * .5438904f, q * .526879f - g * .5209054f},
        {q - g * .9947544f - .01652161f,
         q * -1.871518f + g * 1.871956f - .001895662f,
         q * .7658116f - g * .7710572f + .01613695f,
         q * .1057066f - g * .1061443f + .001511003f},
        3};
}
static coefficients_t tone(float b, float m, float t)
{
    float m2 = m * m, bm = b * m, bt = b * t, mt = m * t;
    return (coefficients_t){
        {b * .01771808f + m * .01736307f + t * .02614062f + bm * .6813673f +
             bt - mt * .025f - m2 * .01703418f + .000442952f,
         mt * .075f - m * .04890412f - t * .07612695f - bm * 1.945197f -
             bt * 3.f - b * .01547534f + m2 * .04862993f - .0003868835f,
         m * .04582842f - b * .01771808f + t * .07385938f + bm * 1.846292f +
             bt * 3.f - mt * .075f - m2 * .04615731f - .000442952f,
         b * .01547534f - m * .01428737f - t * .02387305f - bm * .5824625f -
             bt + mt * .025f + m2 * .01456156f + .0003868835f},
        {b * 1.108421f - m * .009904502f + bm * .6813673f - m2 * .01703418f +
             .02895461f,
         m * .02836346f - b * 3.106178f - bm * 1.945197f + m2 * .04862993f -
             .07867671f,
         b * 2.891579f - m * .02690401f + bm * 1.846292f - m2 * .04615731f +
             .07104787f,
         m * .008445053f - b * .8938217f - bm * .5824625f + m2 * .01456156f -
             .02132081f},
        3};
}
static coefficients_t presence(float p)
{
    float hi = powf(10, p * 8.f / 20.f), lo = powf(10, -p / 20.f),
          kh = tanf(.07123792917f), kl = tanf(.00712379301f);
    float hf = 2 / (hi + 1), hr = 2 / (1 / hi + 1), lf = 2 / (lo + 1),
          lr = 2 / (1 / lo + 1);
    float p0 = kh + hf, p1 = kh - hf, q0 = kl * lr + 1, q1 = kl * lr - 1,
          r0 = kl * lf + 1, r1 = kl * lf - 1, d = p0 * r0;
    return (coefficients_t){{(kh + hr) * q1 / d,
                             ((kh + hr) * q0 + (kh - hr) * q1) / d,
                             (kh - hr) * q0 / d, 0},
                            {1, (p0 * r1 + p1 * r0) / d, p1 * r1 / d, 0},
                            2};
}
static float shape(float x, float negative_drive, float positive_drive,
                   float limit)
{
    float p, m = fabsf(x), drive = x < 0 ? negative_drive : positive_drive;
    unsigned i;
    if (m >= limit)
        return copysignf(limit, x);
    p = 1 - m * drive / 1024.f;
    for (i = 0; i < 10; i++)
        p *= p;
    return copysignf((1 - p) * limit, x);
}
static float stage(gp100_mess_dualv_amplifier_t *a, size_t k, float x,
                   float nd, float pd, float limit)
{
    float p[4], d = (x - a->previous_nonlinear_input[k]) / 3.f, y = 0;
    size_t i;
    p[0] = a->previous_nonlinear_input[k];
    p[1] = p[0] + d;
    p[2] = p[0] + 2 * d;
    p[3] = x;
    memmove(&a->fir_history[k][4], a->fir_history[k], 36 * sizeof(float));
    for (i = 0; i < 4; i++)
        a->fir_history[k][i] = shape(p[3 - i], nd, pd, limit);
    for (i = 0; i < 40; i++)
        y += a->fir_history[k][i] * fir[i];
    a->previous_nonlinear_input[k] = x;
    return y;
}
static bool valid(uint8_t g, uint8_t p, uint8_t o, uint8_t b, uint8_t m,
                  uint8_t t)
{
    return g <= 99 && p <= 99 && o <= 99 && b <= 99 && m <= 99 && t <= 99;
}
static void targets(gp100_mess_dualv_amplifier_t *a, uint8_t g, uint8_t p,
                    uint8_t o, uint8_t b, uint8_t m, uint8_t t)
{
    a->gain_target = curve(gain_curve, g);
    a->presence_target = p * .01f;
    a->master_target = o * .01f;
    a->bass_target = curve(bass_curve, b);
    a->middle_target = m * .01f;
    a->treble_target = t * .01f;
}
bool gp100_mess_dualv_amplifier_reset(gp100_mess_dualv_amplifier_t *a,
                                           uint8_t g, uint8_t p, uint8_t o,
                                           uint8_t b, uint8_t m, uint8_t t)
{
    if (!a || !valid(g, p, o, b, m, t))
        return false;
    memset(a, 0, sizeof(*a));
    targets(a, g, p, o, b, m, t);
    a->gain = a->gain_target;
    a->presence = a->presence_target;
    a->master = a->master_target;
    a->bass = a->bass_target;
    a->middle = a->middle_target;
    a->treble = a->treble_target;
    a->configured = true;
    return true;
}
bool gp100_mess_dualv_amplifier_process(
    gp100_mess_dualv_amplifier_t *a, uint8_t g, uint8_t p, uint8_t o,
    uint8_t b, uint8_t m, uint8_t t, float *audio, size_t frames, bool enabled)
{
    static const coefficients_t in = {
        {3776.152588f, -7508.197266f, 3732.237305f, 0},
        {1, -1.940287232f, .9413198829f, 0},
        2};
    static const coefficients_t aa = {
        {.03138384071f, .06276768143f, .03138384071f, 0},
        {1.246609321f, -1.874464637f, .7533906794f, 0},
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
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        a->presence = a->presence_target;
        a->bass = a->bass_target;
        x = filter(&a->input_filter, in, audio[n * 2]);
        x = filter(&a->gain_filter, gain_voicing(a->gain), x * a->gain);
        x = filter(&a->anti_alias_filter, aa, x);
        x = stage(a, 0, x, 1.f, 1.f, 25.f) * .1f;
        x = filter(&a->tone_filter, tone(a->bass, a->middle, a->treble), x) *
            .8f * a->master;
        x = stage(a, 1, x, 2.f, 1.f, 3.f);
        x = filter(&a->presence_filter, presence(a->presence), x);
        x *= .35f * .0794f * 9.f * .8f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
