#include "ev51_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct coefficients
{
    float b[4], a[4];
    size_t order;
} coefficients_t;

/* Firmware lookups at runtime addresses 0x0003f960 and 0x0003fd60. */
static const uint16_t gain_and_bass_curve[100] = {
    0,    0,    0,    0,    1,    3,    6,    8,    13,   17,   25,   34,
    41,   53,   62,   77,   89,   107,  121,  143,  168,  186,  214,  235,
    267,  291,  328,  354,  396,  425,  471,  521,  555,  610,  647,  707,
    748,  813,  858,  928,  1001, 1052, 1131, 1186, 1271, 1330, 1420, 1483,
    1580, 1647, 1750, 1857, 1930, 2043, 2121, 2241, 2323, 2449, 2536, 2669,
    2807, 2901, 3045, 3143, 3295, 3398, 3557, 3665, 3830, 3943, 4117, 4294,
    4415, 4601, 4727, 4920, 5051, 5251, 5388, 5596, 5810, 5955, 6176, 6326,
    6556, 6712, 6949, 7110, 7357, 7523, 7778, 8037, 8213, 8481, 8663, 8940,
    9127, 9413, 9606, 9900};
static const uint16_t presence_curve[100] = {
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

static float curve(const uint16_t *table, uint8_t value)
{
    return (float)table[value] * .0001f;
}
static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}
static float filter(gp100_ev51_filter_t *state, coefficients_t c,
                    float input)
{
    float output = c.b[0] * input;
    size_t i;
    for (i = 0; i < c.order; i++)
        output += c.b[i + 1] * state->x[i] - c.a[i + 1] * state->y[i];
    output /= c.a[0];
    for (i = c.order; i > 1; i--)
    {
        state->x[i - 1] = state->x[i - 2];
        state->y[i - 1] = state->y[i - 2];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static coefficients_t gain_voicing(float gain)
{
    float q = gain * gain;
    return (coefficients_t){{q * .9761978f - gain * .9834938f,
                             q * -1.930508f + gain * 1.930508f,
                             q * .95431f - gain * .9470141f, 0},
                            {q - gain * .9841319f - .02346054f,
                             q * -1.929974f + gain * 1.93033f - .0009487656f,
                             q * .9310415f - gain * .9465538f + .02251178f, 0},
                            2};
}

static coefficients_t tone(float b, float m, float t)
{
    float m2 = m * m, t2 = t * t;
    float bm2 = b * .1014992f * m2, bm2x3 = b * .3044976f * m2;
    float bm = b * .1969085f * m, bmx3 = b * .5907254f * m;
    return (coefficients_t){
        {b * .01183339f + m * .01629275f + t * .01246949f + b * m +
             b * .7403759f * t + m * .0009027631f * t - b * .5154639f * m2 -
             m2 * .01286072f * t - m2 * .0161512f + .0001911234f,
         b * 1.480112f * m2 - m * .04649754f - t * .03637984f -
             b * 2.871418f * m - b * 2.221128f * t - m * .002429311f * t -
             b * .01050807f + m2 * .03830319f * t + m2 * .04637686f -
             .000169718f,
         m * .04415855f - b * .01183339f + t * .03536235f + b * 2.742836f * m +
             b * 2.221128f * t + m * .002150333f * t - b * 1.413833f * m2 -
             m2 * .03802421f * t - m2 * .0443001f - .0001911234f,
         b * .01050807f - m * .01395376f - t * .01145201f - b * .8714181f * m -
             b * .7403759f * t - m * .000623785f * t + b * .4491846f * m2 +
             m2 * .01258175f * t + m2 * .01407445f + .000169718f},
        {b * .8119448f + m * .01791989f + t * .003388399f + b * 1.040402f * m +
             b * .2012445f * t + m * .003107928f * t - b * .5362899f * m2 -
             b * .1871235f * t2 - m * .003438779f * t2 - m2 * .006299886f * t -
             m2 * .03068772f - t2 * .003150153f + m2 * .00639549f * t2 +
             bm2 * t2 + b * .1751506f * m * t - bm * t2 -
             b * .09028381f * m2 * t + .01366612f,
         b * 1.539069f * m2 - m * .05082379f - t * .009340443f -
             b * 2.985793f * m - b * .5711179f * t - m * .008643621f * t -
             b * 2.29124f + b * .5573115f * t2 + m * .01018103f * t2 +
             m2 * .01781978f * t + m2 * .0875284f + t2 * .009127739f -
             m2 * .01911672f * t2 - bm2x3 * t2 - b * .4949299f * m * t +
             bmx3 * t2 + b * .2551185f * m2 * t - .0374711f,
         b * 2.14956f + m * .0479761f + t * .008569783f + b * 2.850509f * m +
             b * .5391313f * t + m * .007984988f * t - b * 1.469335f * m2 -
             b * .5532523f * t2 - m * .01004573f * t2 - m2 * .01675099f * t -
             m2 * .08304011f - t2 * .008807807f + m2 * .01904698f * t2 +
             bm2x3 * t2 + b * .4644081f * m * t - bmx3 * t2 -
             b * .2393856f * m2 * t + .03416663f,
         b * .4665556f * m2 - m * .01507221f - t * .002617295f -
             b * .9051179f * m - b * .169258f * t - m * .002449295f * t -
             b * .670262f + b * .1830644f * t2 + m * .003303475f * t2 +
             m2 * .005231101f * t + m2 * .02619943f + t2 * .002830221f -
             m2 * .006325745f * t2 - bm2 * t2 - b * .1446288f * m * t +
             bm * t2 + b * .07455091f * m2 * t - .01035984f},
        3};
}

static coefficients_t presence(float p)
{
    float hi, lo, k, hf, hr, lf, lr, p0, p1, q0, q1, r0, r1, d;
    if (p == 0)
        p = .01f;
    hi = powf(10, p * 5.f / 20.f);
    lo = powf(10, p * -1.f / 20.f);
    k = tanf(.03561896458f);
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
static float shape(float input)
{
    float magnitude = fabsf(input), power, drive = input < 0 ? 5.f : 7.f;
    unsigned i;
    if (magnitude >= .5f)
        return copysignf(.5f, input);
    power = 1 - magnitude * drive / 1024.f;
    for (i = 0; i < 10; i++)
        power *= power;
    return copysignf((1 - power) * .5f, input);
}
static float stage(gp100_ev51_amplifier_t *a, float input)
{
    float point[4], step = (input - a->previous_nonlinear_input) / 3.f,
                    output = 0;
    size_t i;
    point[0] = a->previous_nonlinear_input;
    point[1] = point[0] + step;
    point[2] = point[0] + 2 * step;
    point[3] = input;
    memmove(&a->fir_history[4], a->fir_history, 36 * sizeof(float));
    for (i = 0; i < 4; i++)
        a->fir_history[i] = shape(point[3 - i]);
    for (i = 0; i < 40; i++)
        output += a->fir_history[i] * fir[i];
    a->previous_nonlinear_input = input;
    return output;
}
static bool valid(uint8_t g, uint8_t o, uint8_t b, uint8_t m, uint8_t t,
                  uint8_t p)
{
    return g <= 99 && o <= 99 && b <= 99 && m <= 99 && t <= 99 && p <= 99;
}
static void targets(gp100_ev51_amplifier_t *a, uint8_t g, uint8_t o,
                    uint8_t b, uint8_t m, uint8_t t, uint8_t p)
{
    a->gain_target = curve(gain_and_bass_curve, g);
    a->master_target = o * .01f;
    a->bass_target = curve(gain_and_bass_curve, b);
    a->middle_target = curve(gain_and_bass_curve, m);
    a->treble_target = t * .01f;
    a->presence_target = curve(presence_curve, p);
}

bool gp100_ev51_amplifier_reset(gp100_ev51_amplifier_t *a, uint8_t g,
                                     uint8_t o, uint8_t b, uint8_t m, uint8_t t,
                                     uint8_t p)
{
    if (!a || !valid(g, o, b, m, t, p))
        return false;
    memset(a, 0, sizeof(*a));
    targets(a, g, o, b, m, t, p);
    a->gain = a->gain_target;
    a->master = a->master_target;
    a->bass = a->bass_target;
    a->middle = a->middle_target;
    a->treble = a->treble_target;
    a->presence = a->presence_target;
    a->configured = true;
    return true;
}

bool gp100_ev51_amplifier_process(gp100_ev51_amplifier_t *a,
                                       uint8_t g, uint8_t o, uint8_t b,
                                       uint8_t m, uint8_t t, uint8_t p,
                                       float *audio, size_t frames,
                                       bool enabled)
{
    static const coefficients_t in = {
        {13.27170277f, -25.71873283f, 12.44883537f, 0},
        {1, -1.905202746f, .9086843729f, 0},
        2};
    static const coefficients_t voice = {
        {25.71873283f, 17.0326786f, 4.284955502f, 0},
        {1, -.6983562112f, .04672899842f, 0},
        2};
    static const coefficients_t aa = {
        {.008551993378f, .01710398676f, .008551993378f, 0},
        {1.130241454f, -1.965792026f, .869758546f, 0},
        2};
    size_t n;
    if (!a || !a->configured || !valid(g, o, b, m, t, p) ||
        (frames && !audio) || frames > SIZE_MAX / 2)
        return false;
    targets(a, g, o, b, m, t, p);
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
        x = filter(&a->voicing_filter, voice, x);
        x = filter(&a->gain_filter, gain_voicing(a->gain), x);
        x = filter(&a->anti_alias_filter, aa, x);
        x = stage(a, x);
        x = filter(&a->tone_filter, tone(a->bass, a->middle, a->treble), x);
        x *= 3.f * a->master;
        x = filter(&a->presence_filter, presence(a->presence), x);
        x *= .4f * .1549f * 10.f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
