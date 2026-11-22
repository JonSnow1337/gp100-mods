#include "eagle120_amplifier.h"
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
static float filter(gp100_eagle120_filter_t *s, coefficients_t c, float x)
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
    float t2 = t * t, m2 = m * m;
    return (coefficients_t){
        {b * .01506004f + m * .01105262f + t * .02075923f + b * .5470032f * m +
             b * t - m * .02f * t - m2 * .01094006f + .0003012008f,
         m * .06f * t - m * .03125409f - t * .06075394f - b * 1.558067f * m -
             b * 3.f * t - b * .01292279f + m2 * .03116134f - .0002584557f,
         m * .02938993f - b * .01506004f + t * .05924077f + b * 1.475125f * m +
             b * 3.f * t - m * .06f * t - m2 * .02950249f - .0003012008f,
         b * .01292279f - m * .009188467f - t * .01924606f - b * .4640607f * m -
             b * t + m * .02f * t + m2 * .009281213f + .0002584557f},
        {b * 1.119877f - m * .006468749f + t * .05735517f + b * .7674272f * m +
             b * 2.763697f * t - m * .03523414f * t - b * 2.527413f * t2 +
             m * .02846678f * t2 - m2 * .0193091f * t - m2 * .01534854f -
             t2 * .05244634f + m2 * .0212766f * t2 + b * .9654549f * m * t -
             b * 1.06383f * m * t2 + .02323004f,
         m * .01827124f - b * 3.116217f - t * .1568696f - b * 2.178293f * m -
             b * 7.758459f * t + m * .1007305f * t + b * 7.527413f * t2 -
             m * .08591358f * t2 + m2 * .05386229f * t + m2 * .04356586f +
             t2 * .1524331f - m2 * .06382979f * t2 - b * 2.693114f * m * t +
             b * 3.191489f * m * t2 - .06300451f,
         b * 2.880133f - m * .0170887f + t * .1426474f + b * 2.0547f * m +
             b * 7.236303f * t - m * .09565948f * t - b * 7.472587f * t2 +
             m * .08642684f * t2 - m2 * .04979729f * t - m2 * .04109401f -
             t2 * .1475537f + m2 * .06382979f * t2 + b * 2.489864f * m * t -
             b * 3.191489f * m * t2 + .05677118f,
         m * .005286202f - b * .8837728f - t * .04312783f - b * .6438347f * m -
             b * 2.241541f * t + m * .03016315f * t + b * 2.472587f * t2 -
             m * .02898003f * t2 + m2 * .01524409f * t + m2 * .01287669f +
             t2 * .04756689f - m2 * .0212766f * t2 - b * .7622047f * m * t +
             b * 1.06383f * m * t2 - .01699426f},
        3};
}
static coefficients_t presence(float p)
{
    float g, ka, kb, gf, gr, u = 1.f, p0, p1, q0, q1, r0, r1, d;
    if (p == 0)
        p = .01f;
    g = powf(10, (p * 12.f - 6.f) / 20.f);
    ka = tanf(.1424758583f);
    kb = tanf(.005699034315f);
    gf = 2 / (g + 1);
    gr = 2 / (1 / g + 1);
    p0 = ka + gf;
    p1 = ka - gf;
    q0 = ka + gr;
    q1 = ka - gr;
    r0 = kb + u;
    r1 = kb - u;
    d = p0 * r0;
    return (coefficients_t){
        {q0 * r1 / d, (q0 * (kb + u) + q1 * r1) / d, q1 * (kb + u) / d, 0},
        {1, (p0 * r1 + p1 * r0) / d, p1 * r1 / d, 0},
        2};
}
static float shape(float x)
{
    float m = fabsf(x), p, drive = x < 0 ? 2.8f : 3.f;
    unsigned i;
    if (m >= 1.f)
        return copysignf(1.f, x);
    p = 1 - m * drive / 1024.f;
    for (i = 0; i < 10; i++)
        p *= p;
    return copysignf(1 - p, x);
}
static float stage(gp100_eagle120_amplifier_t *a, float x)
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
static void targets(gp100_eagle120_amplifier_t *a, uint8_t g, uint8_t p,
                    uint8_t o, uint8_t b, uint8_t m, uint8_t t)
{
    a->gain_target = curve(gain_curve, g);
    a->presence_target = p * .01f;
    a->master_target = o * .01f;
    a->bass_target = curve(gain_curve, b);
    a->middle_target = m * .01f;
    a->treble_target = t * .01f;
}
bool gp100_eagle120_amplifier_reset(gp100_eagle120_amplifier_t *a,
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
bool gp100_eagle120_amplifier_process(gp100_eagle120_amplifier_t *a,
                                           uint8_t g, uint8_t p, uint8_t o,
                                           uint8_t b, uint8_t m, uint8_t t,
                                           float *audio, size_t frames,
                                           bool enabled)
{
    static const coefficients_t in = {
        {3776.152588f, -7508.197266f, 3732.237305f, 0},
        {1, -1.940287232f, .9413198829f, 0},
        2};
    static const coefficients_t aa = {
        {.03138384071f, .06276768142f, .03138384071f, 0},
        {1.246609321f, -1.874464637f, .7533906795f, 0},
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
        a->bass = a->bass_target;
        a->presence = a->presence_target;
        x = filter(&a->input_filter, in, audio[n * 2]);
        x = filter(&a->gain_filter, gain_voicing(a->gain), x);
        x = filter(&a->anti_alias_filter, aa, x) * .5f * a->gain;
        x = stage(a, x);
        x = filter(&a->tone_filter, tone(a->bass, a->middle, a->treble), x) *
            4.f * a->master;
        x = filter(&a->presence_filter, presence(a->presence), x);
        x *= .65f * .7499f * 2.5f * .5f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
