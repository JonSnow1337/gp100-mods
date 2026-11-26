#include "mini_bass_amplifier.h"
#include <math.h>
#include <string.h>
typedef struct coefficients
{
    float b[5], a[5];
    size_t order;
} coefficients_t;
static const float fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};
static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
static float filter(gp100_mini_bass_filter_t *s, coefficients_t c, float x)
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
    const float b2 = b * b, t2 = t * t, bt = b * t;

    /* The fourth-order tone section is sensitive to rounding. Keep the
     * factory coefficients and the order of its single-precision operations. */
    return (coefficients_t){
        {b2 * 0.6744701862f * t2 - b2 * 0.989997983f * t - b2 * 0.005994991865f
             - b * 0.6813899875f * t2 + bt + b * 0.006055563223f
             - t2 * 0.0007354724221f + t * 0.001090748003f + 6.604810096e-06f,
         b2 * -2.631768942f * t2 + b2 * 3.784213066f * t + b2 * 0.01087842975f
             + b * 2.644859076f * t2 - b * 3.80262208f * t
             - b * 0.01086828019f + t2 * 0.001454454032f - t * 0.002049078001f
             + 8.526806141e-07f,
         b2 * 3.8484869f * t2 - b2 * 5.420369148f * t + b2 * 0.001060195034f
             - b * 3.847737074f * t2 + b * 5.41885519f * t
             - b * 0.001170643955f + t2 * 1.649041042e-05f
             - t * 0.0001306275954f - 1.234493993e-05f,
         b2 * -2.499546051f * t2 + b2 * 3.44809103f * t - b2 * 0.01087842975f
             + b * 2.486455917f * t2 - b * 3.429682016f * t
             + b * 0.01086936984f - t2 * 0.001454454032f + t * 0.002049078001f
             - 8.286900766e-07f,
         b2 * 0.6083586216f * t2 - b2 * 0.8219370246f * t + b2 * 0.004934796132f
             - b * 0.6021884084f * t2 + b * 0.8134486079f * t
             - b * 0.004883829039f + t2 * 0.0007189820171f
             - t * 0.0009601201164f + 5.764125035e-06f},
        {b2 * 7.99706316f * t2 - b2 * 8.431119919f * t - b2 * 0.3485597074f
             - b * 8.074666023f * t2 + b * 8.512934685f * t
             + b * 0.3519422114f - t2 * 0.009094790556f + t * 0.009588428773f
             + 0.0003944328055f,
         b2 * -31.14772987f * t2 + b2 * 31.9702301f * t + b2 * 1.208724022f
             + b * 31.30238914f * t2 - b * 32.12485123f * t - b * 1.214020014f
             + t2 * 0.01718411036f - t * 0.01712954044f - 0.0005801385269f,
         b2 * 45.46638107f * t2 - b2 * 45.42105865f * t - b2 * 1.562782049f
             - b * 45.46583176f * t2 + b * 45.41212082f * t + b * 1.561439991f
             + t2 * 0.001005470986f - t * 0.001938172965f - 0.0001772360993f,
         b2 * -29.47780991f * t2 + b2 * 28.65530968f * t + b2 * 0.8922603726f
             + b * 29.32315063f * t2 - b * 28.50069046f * t
             - b * 0.8869634867f - t2 * 0.01718411036f + t * 0.01712954044f
             + 0.0005816012854f,
         b2 * 7.162097931f * t2 - b2 * 6.773361206f * t - b2 * 0.1896511018f
             - b * 7.085044861f * t2 + b * 6.700489998f * t + b * 0.187610805f
             + t2 * 0.008089319803f - t * 0.007650255226f - 0.000215733904f},
        4};
}
static coefficients_t volume_filter(float v)
{
    float v2 = v * v;
    return (coefficients_t){
        {v2 - v * 1.002268f, -2.f * v2 + 2.f * v, v2 - v * .9977324f, 0, 0},
        {v2 - v * .9994331f - .002835753f, -2.f * v2 + 2.f * v - .000002570945f,
         v2 - v * 1.000567f + .002833182f, 0, 0},
        2};
}
static float saturate(float x)
{
    float m = fabsf(x);
    float y;
    if (m >= 40.f)
        y = 40.f;
    else
    {
        float p = 1.f - m * .000390625f;
        int i;
        for (i = 0; i < 10; i++)
            p *= p;
        y = (1.f - p) * 40.f;
    }
    return copysignf(y, x);
}
static float nonlinear(gp100_mini_bass_amplifier_t *a, float x)
{
    float step = (x - a->previous) / 3.f, y = 0;
    size_t i;
    memmove(&a->nonlinear_history[4], a->nonlinear_history, 36 * sizeof(float));
    for (i = 0; i < 4; i++)
        a->nonlinear_history[3 - i] = saturate(a->previous + step * (float)i);
    a->previous = x;
    for (i = 40; i > 0; i--)
        y += a->nonlinear_history[i - 1] * fir[i - 1];
    return y;
}
static bool valid(uint8_t v, uint8_t b, uint8_t t)
{
    return v <= 99 && b <= 99 && t <= 99;
}
bool gp100_mini_bass_amplifier_reset(gp100_mini_bass_amplifier_t *a,
                                          uint8_t v, uint8_t b, uint8_t t)
{
    if (!a || !valid(v, b, t))
        return false;
    memset(a, 0, sizeof(*a));
    a->volume = a->volume_target = v * .01f;
    a->bass = a->bass_target = b * .01f;
    a->treble = a->treble_target = t * .01f;
    a->configured = true;
    return true;
}
bool gp100_mini_bass_amplifier_process(gp100_mini_bass_amplifier_t *a,
                                            uint8_t v, uint8_t b, uint8_t t,
                                            float *x, size_t frames,
                                            bool enabled)
{
    static const coefficients_t in = {
        {15.34284592f, 14.39490891f, 1.956452489f, 0, 0},
        {1, .7974165082f, .02852440067f, 0, 0},
        2};
    size_t n;
    if (!a || !a->configured || !valid(v, b, t) || (frames && !x) ||
        frames > SIZE_MAX / 2)
        return false;
    a->volume_target = v * .01f;
    a->bass_target = b * .01f;
    a->treble_target = t * .01f;
    for (n = 0; n < frames; n++)
    {
        float y;
        a->volume = glide(a->volume, a->volume_target);
        a->bass = glide(a->bass, a->bass_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        y = filter(&a->input, in, x[n * 2]);
        y = filter(&a->tone, tone(a->bass, a->treble), y);
        y = filter(&a->volume_filter, volume_filter(a->volume), y);
        y = nonlinear(a, y * .5f) * .1047f * .7f * .8f;
        x[n * 2] = y;
        x[n * 2 + 1] = y;
    }
    return true;
}
