#include "bogsv_od_amplifier.h"

#include "power_envelope_internal.h"
#include <math.h>
#include <string.h>
typedef struct coefficients
{
    float b[4], a[4];
    size_t order;
} coefficients_t;
static const float fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};
static float shifted_curve(uint8_t raw, float exponent)
{
    float index = floorf((float)raw * 2.55f) - 2;
    if (index < 0)
        index = 0;
    if (index > 251)
        index = 251;
    return floorf(powf(index / 251.f, exponent) * 10000.f) * .0001f;
}
static float gain_curve(uint8_t raw)
{
    float index = floorf((float)raw * 2.55f);
    return floorf(powf(index / 255.f, 3.5f) * 10000.f) * .0001f;
}
static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
static float filter(gp100_bogsv_od_filter_t *s, coefficients_t c, float x)
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
    float g2 = g * g;
    return (coefficients_t){{g * -.1f, g * .190236f, g * -.09023599f, 0},
                            {g2 - g * 1.028099f - .1028099f,
                             g2 * -1.90236f + g * 1.901857f + .1901857f,
                             g2 * .9023599f - g * .8747641f - .08747641f, 0},
                            2};
}
static coefficients_t tone(float b, float m, float t)
{
    float t2 = t * t, bm = b * m, bt = b * t, mt = m * t,
          m_small = m * .0001717859f, bm_sixth = bm * .1666667f,
          bm_half = bm * .5f;
    return (coefficients_t){
        {b * .02194821f + m * .0009793692f + t * .001038764f + bm * .9510713f +
             bt,
         -b * .01865573f - m * .0008165742f - t * .001022667f - bm * 2.693496f -
             bt * 3,
         bm * 2.533777f - m * .0009793692f - t * .001038764f - b * .02194821f +
             bt * 3,
         b * .01865573f + m * .0008165742f + t * .001022667f - bm * .791353f -
             bt},
        {b * 1.126166f + m * .001008015f + t * .0002520827f + bm * .9790787f +
             bt * .2454268f + mt * .000163523f - b * .2234357f * t2 -
             m_small * t2 - t2 * .000228528f + bm * t * .1584548f -
             bm_sixth * t2 + .001145831f,
         b * .6634357f * t2 - m * .0008394227f - t * .000201067f -
             bm * 2.771422f - bt * .6847347f - mt * .000128513f -
             b * 3.122641f + m_small * t2 + t2 * .0002249867f -
             bm * t * .4417882f + bm_half * t2 - .000913941f,
         b * 2.873838f - m * .001007932f - t * .0002513527f + bm * 2.60577f +
             bt * .6345732f - mt * .000163523f - b * .6565643f * t2 +
             m_small * t2 + t2 * .000228528f + bm * t * .4082118f -
             bm_half * t2 - .001142512f,
         m * .0008395057f - b * .8773555f + t * .000201797f - bm * .813426f -
             bt * .1952653f + mt * .000128513f + b * .2165643f * t2 -
             m_small * t2 - t2 * .0002249867f - bm * t * .1248785f +
             bm_sixth * t2 + .0009172592f},
        3};
}
static coefficients_t presence(float p)
{
    float angle = 1.28228283f, scale = powf(10, p * 8.f / 40.f),
          q = sinf(angle) / .46f, c = -2 * cosf(angle);
    return (coefficients_t){{q * scale + 1, c, 1 - q * scale, 0},
                            {q / scale + 1, c, 1 - q / scale, 0},
                            2};
}
static float nonlinear_stage(gp100_bogsv_od_amplifier_t *a, float input,
                             float level)
{
    float drive = level * 1.7f + 1.5f,
          normalization = fmaxf(1 / (drive * 3 + 1), .15f), point[4],
          step = (input - a->previous_nonlinear_input) / 3, output = 0;
    size_t i;
    point[0] = a->previous_nonlinear_input;
    point[1] = point[0] + step;
    point[2] = point[0] + step * 2;
    point[3] = input;
    memmove(&a->fir_history[4], a->fir_history, 36 * sizeof(float));
    for (i = 0; i < 40; i++)
        output += a->fir_history[i] * fir[i];
    for (i = 0; i < 4; i++)
    {
        float x = point[3 - i];
        a->fir_history[i] =
            copysignf(.15f * (1 - expf(-drive * fabsf(x))), x) * normalization;
    }
    a->previous_nonlinear_input = input;
    return output;
}
static bool valid(uint8_t g, uint8_t p, uint8_t o, uint8_t b, uint8_t m,
                  uint8_t t)
{
    return g <= 99 && p <= 99 && o <= 99 && b <= 99 && m <= 99 && t <= 99;
}
static void targets(gp100_bogsv_od_amplifier_t *a, uint8_t g, uint8_t p,
                    uint8_t o, uint8_t b, uint8_t m, uint8_t t)
{
    a->gain_target = gain_curve(g);
    a->presence = shifted_curve(p, 2);
    a->master_target = o * .01f;
    a->bass_target = shifted_curve(b, 3);
    a->middle_target = m * .01f;
    a->treble_target = shifted_curve(t, 3);
}
bool gp100_bogsv_od_amplifier_reset(gp100_bogsv_od_amplifier_t *a,
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
bool gp100_bogsv_od_amplifier_process(gp100_bogsv_od_amplifier_t *a,
                                           uint8_t g, uint8_t p, uint8_t o,
                                           uint8_t b, uint8_t m, uint8_t t,
                                           float *audio, size_t frames,
                                           bool enabled)
{
    static const coefficients_t input[2] = {
        {{7.67387056f, -8.25576496f, .706500411f, 0},
         {1, -1.13062215f, .154391602f, 0},
         2},
        {{8.25576496f, 4.35470963f, .0000098f, 0},
         {1, .471049488f, .000013f, 0},
         2}};
    static const coefficients_t fixed = {
        {28.6603832f, -50.7708473f, 22.112299f, 0},
        {1, -1.73669052f, .744376719f, 0},
        2};
    static const coefficients_t envelope = {
        {GP100_ENVELOPE_50_MS_B0, 0.f, 0.f, 0.f},
        {1.f, GP100_ENVELOPE_50_MS_A1, GP100_ENVELOPE_50_MS_A2, 0.f},
        2u};
    size_t n, i;
    if (!a || !a->configured || !valid(g, p, o, b, m, t) ||
        (frames && !audio) || frames > SIZE_MAX / 2)
        return false;
    targets(a, g, p, o, b, m, t);
    for (n = 0; n < frames; n++)
    {
        float x, level, angle = .42742759f, k = sinf(angle) / 1.414f, c = cosf(angle);
        a->gain = glide(a->gain, a->gain_target);
        a->master = glide(a->master, a->master_target);
        a->bass = glide(a->bass, a->bass_target);
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        x = audio[n * 2] * 1.1f;
        for (i = 0; i < 2; i++)
            x = filter(&a->input[i], input[i], x);
        x = filter(&a->gain_filter, gain_voicing(a->gain), x);
        x = filter(&a->notch,
                   (coefficients_t){{(1 - c) * .5f, 1 - c, (1 - c) * .5f, 0},
                                    {k + 1, -2 * c, 1 - k, 0},
                                    2},
                   x);
        x = filter(&a->voicing, fixed, x) * .25f;
        level = filter(&a->envelope, envelope, fabsf(x));
        x = nonlinear_stage(a, x, level);
        x = filter(&a->tone, tone(a->bass, a->middle, a->treble), x) * 45 *
            a->master;
        x = filter(&a->presence_filter, presence(a->presence), x) * 2.49172f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
