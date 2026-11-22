#include "dizz_vh_amplifier.h"
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
static const uint16_t bass_curve[100] = {
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
static float filter(gp100_dizz_vh_filter_t *s, coefficients_t c, float x)
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
    return (coefficients_t){{g * .9222159f, g * -.9222159f, 0, 0},
                            {g + .8655279f, g * -.9936631f - .7806889f, 0, 0},
                            1};
}
static coefficients_t tone(float b, float m, float t)
{
    float t2 = t * t, m2 = m * m;
    return (coefficients_t){
        {b * .01914641f + m * .01609889f + t * .02303753f + b * .7173373f * m +
             b * t - m * .022f * t - m2 * .01578142f + .000421221f,
         m * .066f * t - m * .04526589f - t * .06702391f - b * 2.045542f * m -
             b * 3.f * t - b * .01665233f + m2 * .04500193f - .0003663512f,
         m * .04234214f - b * .01914641f + t * .06496247f + b * 1.939073f * m +
             b * 3.f * t - m * .066f * t - m2 * .0426596f - .000421221f,
         b * .01665233f - m * .01317513f - t * .02097609f - b * .6108678f * m -
             b * t + m * .022f * t + m2 * .01343909f + .0003663512f},
        {b * 2.22476f - m * .01471204f + t * .01187595f + b * 1.486466f * m +
             b * .5157093f * t - m * .005796706f * t - b * .4742705f * t2 +
             m * .004352737f * t2 - m2 * .005300258f * t - m2 * .03270226f -
             t2 * .0109196f + m2 * .005809056f * t2 + b * .2409208f * m * t -
             b * .264048f * m * t2 + .05121073f,
         m * .04232096f - b * 6.219334f - t * .03235093f - b * 4.23638f * m -
             b * 1.45077f * t + m * .01685896f * t + b * 1.410441f * t2 -
             m * .01333037f * t2 + m2 * .0148588f * t + m2 * .09320035f +
             t2 * .03150897f - m2 * .01742717f * t2 - b * .6753998f * m * t +
             b * .792144f * m * t2 - .1386797f,
         b * 5.775246f - m * .04028676f + t * .02931672f + b * 4.013588f * m +
             b * 1.356631f * t - m * .01627771f * t - b * 1.39807f * t2 +
             m * .01360253f * t2 - m2 * .01381682f * t - m2 * .08829894f -
             t2 * .03027189f + m2 * .01742717f * t2 + b * .6280372f * m * t -
             b * .792144f * m * t2 + .1247944f,
         m * .01267784f - b * 1.780661f - t * .008839382f - b * 1.263675f * m -
             b * .4215704f * t + m * .005215454f * t + b * .4618997f * t2 -
             m * .004624895f * t2 + m2 * .00425828f * t + m2 * .02780085f +
             t2 * .009682518f - m2 * .005809056f * t2 - b * .1935582f * m * t +
             b * .264048f * m * t2 - .03731519f},
        3};
}
static coefficients_t presence(float p)
{
    float hi, lo, k, hf, hr, lf, lr, p0, p1, q0, q1, r0, r1, d;
    if (p == 0)
        p = .01f;
    hi = powf(10, p * 4.f / 20.f);
    lo = powf(10, p * -2.f / 20.f);
    k = tanf(.01424758602f);
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
    float m = fabsf(x), p;
    unsigned i;
    if (m >= 1.f)
        return copysignf(1.f, x);
    p = 1 - m * 4.f / 1024.f;
    for (i = 0; i < 10; i++)
        p *= p;
    return copysignf(1 - p, x);
}
static float stage(gp100_dizz_vh_amplifier_t *a, float x)
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
static void targets(gp100_dizz_vh_amplifier_t *a, uint8_t g, uint8_t p,
                    uint8_t o, uint8_t b, uint8_t m, uint8_t t)
{
    a->gain_target = curve(gain_curve, g);
    a->presence_target = curve(gain_curve, p);
    a->master_target = o * .01f;
    a->bass_target = curve(bass_curve, b);
    a->middle_target = m * .01f;
    a->treble_target = t * .01f;
}
bool gp100_dizz_vh_amplifier_reset(gp100_dizz_vh_amplifier_t *a,
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
bool gp100_dizz_vh_amplifier_process(gp100_dizz_vh_amplifier_t *a,
                                          uint8_t g, uint8_t p, uint8_t o,
                                          uint8_t b, uint8_t m, uint8_t t,
                                          float *audio, size_t frames,
                                          bool enabled)
{
    static const coefficients_t in = {
        {1574.017578f, -3129.692139f, 1555.675293f, 0},
        {1, -1.987108946f, .9871237874f, 0},
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
        a->bass = glide(a->bass, a->bass_target);
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        a->presence = a->presence_target;
        x = filter(&a->input_filter, in, audio[n * 2]);
        x = filter(&a->gain_filter, gain_voicing(a->gain), x);
        x = filter(&a->anti_alias_filter, aa, x);
        x = stage(a, x);
        x = filter(&a->tone_filter, tone(a->bass, a->middle, a->treble), x) *
            a->master;
        x = filter(&a->presence_filter, presence(a->presence), x);
        x *= 2.f * .0871f * 9.f * 1.2f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
