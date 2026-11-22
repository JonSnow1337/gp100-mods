#include "flagman_amplifier.h"
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
static float coordinate(uint8_t raw)
{
    unsigned i = (unsigned)((float)raw * 2.55f);
    if (i <= 2)
        return 0;
    if (i >= 253)
        return 1;
    return (float)(i - 2) / 251.f;
}
static float taper(uint8_t raw, float exponent)
{
    float x = coordinate(raw);
    return floorf(powf(x, exponent) * 10000.f) * .0001f;
}
static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
static float filter(gp100_flagman_filter_t *s, coefficients_t c, float x)
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
static coefficients_t tone(float o, float b, float m, float t)
{
    float m2 = m * m, t2 = t * t, bm = b * m, bt = b * t, mt = m * t;
    return (coefficients_t){
        {o * (.0005297113f - .0231963f * m2 + .02118845f * b + .02362213f * m +
              .02603876f * t + .9278518f * bm + bt - .025f * mt),
         o * (.06607508f * m2 - .0004572767f - .01829107f * b - .06643009f * m -
              .07602267f * t - 2.643003f * bm - 3.f * bt + .075f * mt),
         o * (.06213545f * m - .06256128f * m2 - .02118845f * b - .0005297113f +
              .07396124f * t + 2.502451f * bm + 3.f * bt - .075f * mt),
         o * (.0004572767f + .01968249f * m2 + .01829107f * b - .01932748f * m -
              .02397733f * t - .7872997f * bm - bt + .025f * mt)},
        {b * 1.60339f - m * .003864769f + t * .01436126f + bm * 1.391493f +
             bt * .5517906f - mt * .004361764f - b * .5078084f * t2 +
             m * .002835092f * t2 - m2 * .009059105f * t - m2 * .03478734f -
             t2 * .01321459f + m2 * .009469697f * t2 + bm * t * .3623642f -
             bm * .3787879f * t2 + .04171623f,
         m * .01172484f - b * 4.478805f - t * .03922365f - bm * 3.96117f -
             bt * 1.550406f + mt * .01295778f + b * 1.507808f * t2 -
             m * .008895698f * t2 + m2 * .0254985f * t + m2 * .09902925f +
             t2 * .03820654f - m2 * .02840909f * t2 - bm * t * 1.01994f +
             bm * 1.136364f * t2 - .113305f,
         b * 4.156616f - m * .01164432f + t * .0356402f + bm * 3.748143f +
             bt * 1.448209f - mt * .01275945f - b * 1.492192f * t2 +
             m * .00928612f * t2 - m2 * .02381968f * t - m2 * .09370357f -
             t2 * .03678541f + m2 * .02840909f * t2 + bm * t * .9527873f -
             bm * 1.136364f * t2 + .1022881f,
         m * .003784248f - b * 1.281189f - t * .01077489f - bm * 1.178466f -
             bt * .4495937f + mt * .004163429f + b * .4921916f * t2 -
             m * .003225514f * t2 + m2 * .007380289f * t + m2 * .02946166f +
             t2 * .01179346f - m2 * .009469697f * t2 - bm * t * .2952115f +
             bm * .3787879f * t2 - .03069065f},
        3};
}
static float shape(float x)
{
    float magnitude = fabsf(x), p, slope = x < 0 ? 1.2f : 1.5f;
    unsigned i;
    if (magnitude >= 3)
        return copysignf(3, x);
    p = 1 - magnitude * slope / 1024.f;
    for (i = 0; i < 10; i++)
        p *= p;
    return copysignf((1 - p) * 3, x);
}
static float power_stage(gp100_flagman_amplifier_t *a, float x)
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
bool gp100_flagman_amplifier_reset(gp100_flagman_amplifier_t *a,
                                        uint8_t g, uint8_t p, uint8_t o,
                                        uint8_t b, uint8_t m, uint8_t t)
{
    if (!a || g > 99 || p > 99 || o > 99 || b > 99 || m > 99 || t > 99)
        return false;
    memset(a, 0, sizeof(*a));
    a->gain = a->gain_target = taper(g, 2);
    a->presence = taper(p, 1.5f);
    a->master = a->master_target = o * .01f;
    a->bass = a->bass_target = taper(b, 3);
    a->middle = a->middle_target = m * .01f;
    a->treble = a->treble_target = t * .01f;
    a->configured = true;
    return true;
}
bool gp100_flagman_amplifier_process(gp100_flagman_amplifier_t *a,
                                          uint8_t g, uint8_t p, uint8_t o,
                                          uint8_t b, uint8_t m, uint8_t t,
                                          float *audio, size_t frames,
                                          bool enabled)
{
    static const coefficients_t in[4] = {
        {{8.1513786f, -16.128481f, 7.9783192f, 0},
         {1, -1.91803336f, .920446813f, 0},
         2},
        {{11.986228f, -16.128481f, 5.1517954f, 0},
         {1, -1.48205388f, .549051285f, 0},
         2},
        {{16.128481f, 14.572543f, 3.3417206f, 0},
         {1, .600556076f, .0013756f, 0},
         2},
        {{.929604471f, -1.80545402f, .875849187f, 0},
         {.999999881f, -1.80533898f, .805568278f, 0},
         2}};
    size_t n, i;
    if (!a || !a->configured || g > 99 || p > 99 || o > 99 || b > 99 ||
        m > 99 || t > 99 || (frames && !audio) || frames > SIZE_MAX / 2)
        return false;
    a->gain_target = taper(g, 2);
    a->presence = taper(p, 1.5f);
    a->master_target = o * .01f;
    a->bass_target = taper(b, 3);
    a->middle_target = m * .01f;
    a->treble_target = t * .01f;
    for (n = 0; n < frames; n++)
    {
        float x, angle, q, scale, c, k;
        a->gain = glide(a->gain, a->gain_target);
        a->master = glide(a->master, a->master_target);
        a->bass = glide(a->bass, a->bass_target);
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        x = audio[n * 2];
        for (i = 0; i < 4; i++)
            x = filter(&a->input[i], in[i], x);
        angle = .498665541f;
        c = cosf(angle);
        k = sinf(angle) / 1.414f;
        x = filter(&a->notch,
                   (coefficients_t){{(1 - c) * .5f, 1 - c, (1 - c) * .5f, 0},
                                    {k + 1, -2 * c, 1 - k, 0},
                                    2},
                   x);
        x = power_stage(a, x * a->gain);
        x = filter(&a->tone, tone(a->master, a->bass, a->middle, a->treble),
                   x) *
            2;
        angle = ((4543 - a->presence * 1300) / 44100) * 6.2831855f;
        scale = powf(10, a->presence * 6.25f / 40);
        q = sinf(angle) / .728f;
        x = filter(&a->presence_filter,
                   (coefficients_t){
                       {q * scale + 1, -2 * cosf(angle), 1 - q * scale, 0},
                       {q / scale + 1, -2 * cosf(angle), 1 - q / scale, 0},
                       2},
                   x) *
            .33f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
